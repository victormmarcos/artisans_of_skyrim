#!/usr/bin/env python3
"""Validate vanilla SharedInfo dialogue responses in ArtisansOfSkyrim.esp."""

from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path

import GenerateArtisansOfSkyrimEsp as generator
from ValidateArtisansOfSkyrimAlchemyEsp import (
    one_field,
    parse_fields,
    read_u32,
    walk_records,
    ztext,
)


# Voiced responses plus one voice-independent fallback per service topic.
EXPECTED_RESPONSE_COUNTS = {
    "Smithing Craft": 27,
    "Smithing Improve": 28,
    "Enchanting": 7,
    "Alchemy": 21,
    "Smithing Orders": 28,
}
EXPECTED_INFO_TOTAL = 111


def validate_source_semantics(
    responses: dict[str, list[generator.DialogueResponse]],
) -> None:
    for service in ("Smithing Craft", "Smithing Improve"):
        for response in responses[service]:
            lowered = response.response_text.lower()
            assert not any(word in lowered for word in ("potion", "alchemy", "staff")), (
                f"{service} has a profession-incompatible response: "
                f"{response.response_text!r}"
            )

    for response in responses["Enchanting"]:
        lowered = response.response_text.lower()
        assert "potion" not in lowered and "alchem" not in lowered, (
            f"Enchanting has an alchemy-specific response for {response.selector_name}: "
            f"{response.response_text!r}"
        )

    feran = [
        response
        for response in responses["Enchanting"]
        if response.selector_name == "Feran Sadri"
    ]
    assert len(feran) == 1
    assert feran[0].response_text == "Take a look."
    assert feran[0].response_plugin == "Skyrim.esm"
    assert feran[0].response_local_id == 0x07F6BC

    florentius = [
        response
        for response in responses["Alchemy"]
        if response.selector_name == "Florentius Baenius"
    ]
    assert len(florentius) == 1
    assert florentius[0].selector_type == "Actor"
    assert florentius[0].response_plugin == "Dawnguard.esm"
    assert florentius[0].response_local_id == 0x00E7A9
    assert florentius[0].conditions == (
        (
            generator.GET_IS_ID,
            generator.FORMS["Florentius"],
            generator.RUN_ON_SUBJECT,
            0,
        ),
    )

    eorlund = [
        response
        for response in responses["Smithing Craft"]
        if response.selector_name == "Eorlund Gray-Mane"
    ]
    assert len(eorlund) == 1
    assert eorlund[0].response_text == "Gods be praised."
    assert eorlund[0].response_plugin == "Skyrim.esm"
    assert eorlund[0].response_local_id == 0x0B80C8

    gunmar = [
        response
        for response in responses["Smithing Improve"]
        if response.selector_name == "Gunmar"
    ]
    assert {(entry.response_text, entry.response_local_id) for entry in gunmar} == {
        ("Aye lad.", 0x01056C),
        ("Aye lass.", 0x01056D),
    }
    for response in responses["Smithing Improve"]:
        if response.selector_name == "Gunmar":
            continue
        assert response.response_text == "Of course."
        assert response.response_plugin == "Skyrim.esm"
        assert response.response_local_id == 0x0DBA22


def parse_condition(value: bytes) -> tuple:
    assert len(value) == 32, f"CTDA length {len(value)} != 32"
    comparison = struct.unpack_from("<f", value, 4)[0]
    assert comparison in (0.0, 1.0)
    parsed = (
        read_u32(value, 8),
        read_u32(value, 12),
        read_u32(value, 20),
        read_u32(value, 24),
    )
    return parsed if comparison == 1.0 else parsed + (0.0,)


def validate_info_record(
    record,
    topic: generator.DialogueTopic,
    response: generator.DialogueResponse,
) -> None:
    assert record.form_id == generator.local_to_form(response.info_local_id)
    fields = parse_fields(record.body)
    signatures = [signature for signature, _value in fields]
    assert signatures == [
        "VMAD",
        "ENAM",
        "CNAM",
        "DNAM",
        *("CTDA" for _condition in response.conditions),
        *(["CTDA"] if response.access_global else []),
    ], f"INFO {record.form_id:08X}: unexpected fields {signatures}"
    assert one_field(fields, "VMAD") == generator.vmad_fragment(topic.fragment_script), (
        f"INFO {record.form_id:08X}: fragment does not match its voice/lock branch"
    )
    assert one_field(fields, "ENAM") == generator.u32(2)
    assert one_field(fields, "CNAM") == b"\0"
    assert read_u32(one_field(fields, "DNAM"), 0) == response.shared_info_form_id
    condition_fields = [value for signature, value in fields if signature == "CTDA"]
    if response.access_global:
        access = condition_fields.pop()
        assert access == generator.ctda(generator.GET_GLOBAL_VALUE,
            generator.local_to_form(response.access_global))
    actual_conditions = tuple(
        parse_condition(value) for value in condition_fields
    )
    assert actual_conditions == response.conditions, (
        f"INFO {record.form_id:08X}: conditions changed: "
        f"{actual_conditions} != {response.conditions}"
    )


def validate(esp_path: Path) -> None:
    npc_conditions = generator.read_npc_dialogue_conditions()
    responses = generator.read_dialogue_responses(npc_conditions)
    validate_source_semantics(responses)
    topics = generator.dialogue_topics(responses)
    expected_by_info_id = {
        generator.local_to_form(response.info_local_id): (topic, response)
        for topic in topics
        for response in topic.responses
    }

    records = list(walk_records(esp_path.read_bytes()))
    info_records = [record for record in records if record.signature == "INFO"]
    dial_records = [record for record in records if record.signature == "DIAL"]
    glob_records = [record for record in records if record.signature == "GLOB"]
    assert len(info_records) == sum(EXPECTED_RESPONSE_COUNTS.values()) == EXPECTED_INFO_TOTAL

    # Each service topic ends with exactly one voice-independent fallback.
    for topic in topics:
        if topic.service == "Enchanting Neloth":
            continue
        fallbacks = [entry for entry in topic.responses if entry.selector_type == "Fallback"]
        assert len(fallbacks) == 1 and topic.responses[-1] is fallbacks[0], (
            f"{topic.topic_edid}: fallback must be the single last response"
        )
        assert not any(
            condition[0] == generator.GET_IS_VOICE_TYPE for condition in fallbacks[0].conditions
        ), f"{topic.topic_edid}: fallback must not require a voice type"
    assert {record.form_id for record in info_records} == set(expected_by_info_id)

    assert len(glob_records) == 5
    globals_by_name = {}
    for global_record in glob_records:
        global_fields = parse_fields(global_record.body)
        assert [signature for signature, _value in global_fields] == [
            "EDID",
            "FNAM",
            "FLTV",
        ]
        name = ztext(one_field(global_fields, "EDID"))
        globals_by_name[name] = global_record.form_id
        assert one_field(global_fields, "FNAM") == b"f"
        assert struct.unpack("<f", one_field(global_fields, "FLTV"))[0] == 0.0
    assert globals_by_name == {
        "AOS_HasOrdersForCurrentSpeaker": generator.local_to_form(0x084F),
        "AOS_SmithServicesForCurrentSpeaker": generator.local_to_form(0x08F0),
        **{name: generator.local_to_form(local_id)
           for local_id, name in generator.SERVICE_ACCESS_GLOBALS.values()},
    }

    for record in info_records:
        topic, response = expected_by_info_id[record.form_id]
        validate_info_record(record, topic, response)

    # Inspect emitted access conditions independently: unsupported selects neither,
    # unlocked selects voiced INFOs; unavailable and legacy locked state select none.
    by_id = {record.form_id: record for record in info_records}
    for topic in [
        entry for entry in topics
        if entry.service in {
            "Smithing Craft",
            "Smithing Improve",
            "Enchanting",
            "Enchanting Neloth",
            "Alchemy",
        }
    ]:
        for state in (0.0, 1.0, 2.0):
            eligible = []
            for response in topic.responses:
                fields = parse_fields(by_id[generator.local_to_form(response.info_local_id)].body)
                condition = [raw for name, raw in fields if name == "CTDA"][-1]
                if struct.unpack_from("<f", condition, 4)[0] == state:
                    eligible.append(response)
            if state == 0.0:
                assert not eligible
            elif state == 1.0:
                assert eligible and all(entry.shared_info_form_id for entry in eligible)
            else:
                assert not eligible

    expected_topic_counts = {
        topic.topic_edid: len(topic.responses) for topic in topics
    }
    actual_topic_counts = {}
    for record in dial_records:
        fields = parse_fields(record.body)
        actual_topic_counts[ztext(one_field(fields, "EDID"))] = read_u32(
            one_field(fields, "TIFC"), 0
        )
    assert actual_topic_counts == expected_topic_counts

    seq_data = generator.SEQ_PATH.read_bytes()
    assert seq_data == generator.form_bytes(generator.local_to_form(0x0800)), (
        "SEQ does not contain the generated dialogue quest FormID"
    )

    audio_extensions = {".fuz", ".xwm", ".wav", ".lip"}
    distributed_audio = [
        path
        for path in (generator.ROOT / "dist").rglob("*")
        if path.is_file() and path.suffix.lower() in audio_extensions
    ]
    assert not distributed_audio, f"Unexpected redistributed voice files: {distributed_audio}"

    with generator.MANIFEST_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        manifest_info_count = sum(
            row["Signature"] == "INFO" for row in csv.DictReader(handle)
        )
    assert manifest_info_count == EXPECTED_INFO_TOTAL

    print(f"Validated {EXPECTED_INFO_TOTAL} INFO records (voiced plus fallbacks) in {esp_path}")
    print("Unavailable and legacy locked states: no dialogue; available state: voiced dialogue OK")
    print("SharedInfo links, actor/faction/VoiceType conditions and end fragments: OK")
    print("Feran Sadri: neutral Enchanting response only ('Take a look.'): OK")
    print("Florentius: exact actor; recruitment gate enforced by runtime: OK")
    print("Eorlund craft: 'Gods be praised.' subtitle and SharedInfo: OK")
    print("Improve/collection: neutral response, including sex-aware Gunmar lines: OK")
    print("Smithing service and collection runtime globals: OK")
    print("SEQ: matches the existing AOS_DialogueQuest FormID")
    print("Redistributed voice/audio files: none")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--esp", type=Path, default=generator.ESP_PATH)
    args = parser.parse_args()
    validate(args.esp)


if __name__ == "__main__":
    main()
