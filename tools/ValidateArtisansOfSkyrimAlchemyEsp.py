#!/usr/bin/env python3
"""Validate every generated Artisans of Skyrim ALCH record in the staged ESP."""

from __future__ import annotations

import argparse
import math
import struct
import tomllib
from dataclasses import dataclass
from pathlib import Path

import GenerateArtisansOfSkyrimEsp as generator


EXPECTED_ALCH_COUNT = 63
EXPECTED_ALCH_EDITOR_IDS = (
    "AOS_ALCH_PoisonDamageHealthAdept",
    "AOS_ALCH_PoisonDamageHealthExpert",
    "AOS_ALCH_PoisonDamageHealthMaster",
    "AOS_ALCH_PoisonDamageMagickaAdept",
    "AOS_ALCH_PoisonDamageMagickaExpert",
    "AOS_ALCH_PoisonDamageMagickaMaster",
    "AOS_ALCH_PoisonDamageStaminaAdept",
    "AOS_ALCH_PoisonDamageStaminaExpert",
    "AOS_ALCH_PoisonDamageStaminaMaster",
    "AOS_ALCH_PoisonDamageHealthPlusLingeringDamageHealthMaster",
    "AOS_ALCH_PoisonFearMaster",
    "AOS_ALCH_PoisonFrenzyMaster",
    "AOS_ALCH_PoisonParalysisMaster",
    "AOS_ALCH_PoisonSlowExpert",
    "AOS_ALCH_PoisonSlowMaster",
    "AOS_ALCH_PotionFortifyDestructionExpert",
    "AOS_ALCH_PotionFortifyDestructionMaster",
    "AOS_ALCH_PotionFortifyIllusionExpert",
    "AOS_ALCH_PotionFortifyIllusionMaster",
    "AOS_ALCH_PotionFortifyArcheryExpert",
    "AOS_ALCH_PotionFortifyArcheryMaster",
    "AOS_ALCH_PotionFortifyOneHandedExpert",
    "AOS_ALCH_PotionFortifyOneHandedMaster",
    "AOS_ALCH_PotionFortifyTwoHandedExpert",
    "AOS_ALCH_PotionFortifyTwoHandedMaster",
    "AOS_ALCH_PotionFortifySneakExpert",
    "AOS_ALCH_PotionFortifySneakMaster",
    "AOS_ALCH_PotionResistFireExpert",
    "AOS_ALCH_PotionResistFireMaster",
    "AOS_ALCH_PotionResistFrostExpert",
    "AOS_ALCH_PotionResistFrostMaster",
    "AOS_ALCH_PotionResistShockExpert",
    "AOS_ALCH_PotionResistShockMaster",
    "AOS_ALCH_PotionResistMagicMaster",
    "AOS_ALCH_PotionResistPoisonExpert",
    "AOS_ALCH_PotionResistPoisonMaster",
    "AOS_ALCH_PotionRestoreHealthAdept",
    "AOS_ALCH_PotionRestoreHealthExpert",
    "AOS_ALCH_PotionRestoreHealthMaster",
    "AOS_ALCH_PotionRestoreMagickaAdept",
    "AOS_ALCH_PotionRestoreMagickaExpert",
    "AOS_ALCH_PotionRestoreMagickaMaster",
    "AOS_ALCH_PotionRestoreStaminaAdept",
    "AOS_ALCH_PotionRestoreStaminaExpert",
    "AOS_ALCH_PotionRestoreStaminaMaster",
    "AOS_ALCH_PotionRestoreMagickaPlusRegenerateMagickaMaster",
    "AOS_ALCH_PotionRestoreMagickaPlusFortifyDestructionMaster",
    "AOS_ALCH_PotionRestoreMagickaPlusFortifyIllusionMaster",
    "AOS_ALCH_PotionRestoreStaminaPlusRegenerateStaminaMaster",
    "AOS_ALCH_PotionCureDiseaseAdept",
    "AOS_ALCH_PotionFortifyCarryWeightExpert",
    "AOS_ALCH_PotionFortifyCarryWeightMaster",
    "AOS_ALCH_PotionWaterbreathingMaster",
    "AOS_ALCH_PotionInvisibilityMaster",
    "AOS_ALCH_PotionHealthOverTimeAdept",
    "AOS_ALCH_PotionHealthOverTimeExpert",
    "AOS_ALCH_PotionHealthOverTimeMaster",
    "AOS_ALCH_PotionMagickaOverTimeAdept",
    "AOS_ALCH_PotionMagickaOverTimeExpert",
    "AOS_ALCH_PotionMagickaOverTimeMaster",
    "AOS_ALCH_PotionStaminaOverTimeAdept",
    "AOS_ALCH_PotionStaminaOverTimeExpert",
    "AOS_ALCH_PotionStaminaOverTimeMaster",
)


def read_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def read_f32(data: bytes, offset: int = 0) -> float:
    return struct.unpack_from("<f", data, offset)[0]


@dataclass(frozen=True)
class EspRecord:
    signature: str
    form_id: int
    header: bytes
    body: bytes


def walk_records(data: bytes, start: int = 0, end: int | None = None):
    if end is None:
        end = len(data)
    offset = start
    while offset < end:
        if offset + 8 > end:
            raise ValueError(f"Truncated record/group header at 0x{offset:X}")
        signature = data[offset : offset + 4]
        size = read_u32(data, offset + 4)
        if signature == b"GRUP":
            if size < 24 or offset + size > end:
                raise ValueError(f"Invalid GRUP size {size} at 0x{offset:X}")
            yield from walk_records(data, offset + 24, offset + size)
            offset += size
            continue
        record_end = offset + 24 + size
        if record_end > end:
            raise ValueError(f"Truncated {signature!r} record at 0x{offset:X}")
        yield EspRecord(
            signature.decode("ascii"),
            read_u32(data, offset + 12),
            data[offset : offset + 24],
            data[offset + 24 : record_end],
        )
        offset = record_end
    if offset != end:
        raise ValueError(f"Record walk ended at 0x{offset:X}, expected 0x{end:X}")


def parse_fields(body: bytes) -> list[tuple[str, bytes]]:
    fields: list[tuple[str, bytes]] = []
    offset = 0
    extended_size: int | None = None
    while offset < len(body):
        if offset + 6 > len(body):
            raise ValueError(f"Truncated subrecord at body offset 0x{offset:X}")
        signature = body[offset : offset + 4].decode("ascii")
        size = read_u16(body, offset + 4)
        offset += 6
        if signature == "XXXX":
            if size != 4 or offset + 4 > len(body):
                raise ValueError("Invalid XXXX extended-size subrecord")
            extended_size = read_u32(body, offset)
            offset += 4
            continue
        if extended_size is not None:
            size = extended_size
            extended_size = None
        if offset + size > len(body):
            raise ValueError(f"Truncated {signature} subrecord")
        fields.append((signature, body[offset : offset + size]))
        offset += size
    if extended_size is not None:
        raise ValueError("Dangling XXXX extended-size subrecord")
    return fields


def ztext(value: bytes) -> str:
    return value.rstrip(b"\0").decode("utf-8")


def one_field(fields: list[tuple[str, bytes]], signature: str) -> bytes:
    values = [value for field_signature, value in fields if field_signature == signature]
    if len(values) != 1:
        raise AssertionError(f"Expected one {signature} field, found {len(values)}")
    return values[0]


def expected_model(item: generator.AlchemyRecord) -> str:
    if item.effects[0][0] in {"Invisibility", "Waterbreathing"}:
        return generator.INVISIBILITY_MODEL
    if item.item_type == "Poison":
        return generator.POISON_MODEL
    return generator.POTION_MODEL


def validate_item(record: EspRecord, item: generator.AlchemyRecord) -> None:
    fields = parse_fields(record.body)
    signatures = [signature for signature, _value in fields]
    expected_effects = [
        (generator.MGEF[name], magnitude, 0, duration)
        for name, magnitude, duration in item.effects
    ]
    expected_signatures = [
        "EDID", "OBND", "FULL", "KSIZ", "KWDA", "MODL", "MODT",
        "YNAM", "ZNAM", "DATA", "ENIT",
    ] + [part for _effect in expected_effects for part in ("EFID", "EFIT")]
    assert signatures == expected_signatures, (
        f"{item.editor_id}: unexpected subrecord order/content: {signatures}"
    )
    assert record.form_id == item.form_id, (
        f"{item.editor_id}: FormID {record.form_id:08X} != {item.form_id:08X}"
    )
    assert ztext(one_field(fields, "EDID")) == item.editor_id
    assert ztext(one_field(fields, "FULL")) == item.display_name

    is_poison = item.item_type == "Poison"
    assert one_field(fields, "OBND") == (
        generator.POISON_OBND if is_poison else generator.POTION_OBND
    )
    assert one_field(fields, "KSIZ") == generator.u32(1)
    assert one_field(fields, "KWDA") == (
        generator.POISON_KWDA if is_poison else generator.POTION_KWDA
    )
    assert ztext(one_field(fields, "MODL")) == expected_model(item)
    assert one_field(fields, "MODT") == (
        generator.POISON_MODT if is_poison else generator.POTION_MODT
    )
    assert one_field(fields, "YNAM") == generator.PICKUP_SOUND
    assert one_field(fields, "ZNAM") == generator.PUTDOWN_SOUND
    assert read_f32(one_field(fields, "DATA")) == item.weight

    enit = one_field(fields, "ENIT")
    assert len(enit) == 20, f"{item.editor_id}: ENIT length is {len(enit)}, expected 20"
    value = read_u32(enit, 0)
    flags = read_u32(enit, 4)
    expected_flags = generator.ALCH_FLAG_NO_AUTO_CALC
    if is_poison:
        expected_flags |= generator.ALCH_FLAG_POISON
    assert value == item.alch_value, (
        f"{item.editor_id}: ENIT value {value} != ALCHValuePerFlask {item.alch_value}"
    )
    assert flags == expected_flags, (
        f"{item.editor_id}: flags 0x{flags:08X} != 0x{expected_flags:08X}"
    )
    assert enit[8:] == (
        generator.POISON_ENIT_TAIL if is_poison else generator.POTION_ENIT_TAIL
    )

    effect_fields = [entry for entry in fields if entry[0] in {"EFID", "EFIT"}]
    actual_effects = []
    for index in range(0, len(effect_fields), 2):
        efid_signature, efid = effect_fields[index]
        efit_signature, efit = effect_fields[index + 1]
        assert efid_signature == "EFID" and efit_signature == "EFIT"
        assert len(efid) == 4 and len(efit) == 12
        actual_effects.append(
            (read_u32(efid, 0), read_f32(efit, 0), read_u32(efit, 4), read_u32(efit, 8))
        )
    assert actual_effects == expected_effects, f"{item.editor_id}: effects changed"


def validate_resale_safety(items: list[generator.AlchemyRecord]) -> None:
    with generator.CONFIG_PATH.open("rb") as handle:
        config = tomllib.load(handle)
    max_discount = float(config["pricing"]["barter"]["maxDiscount"])
    assert 0.0 <= max_discount <= 0.20
    print("Resale safety (strict batch resale < minimum discounted service price):")
    for item in items:
        minimum_price = math.ceil(item.batch_price * (1.0 - max_discount))
        maximum_value = max(0, (minimum_price - 1) // item.batch_size)
        resale_total = item.alch_value * item.batch_size
        assert resale_total < minimum_price, (
            f"{item.editor_id}: {item.alch_value} x {item.batch_size} = "
            f"{resale_total}, minimum service price is {minimum_price}"
        )
        print(
            f"  {item.form_id:08X} | {item.display_name} | {item.tier} | "
            f"batch {item.batch_size} | minimum {minimum_price} | "
            f"maximum ALCH {maximum_value} | current {item.alch_value}"
        )


def validate_courier_books(records: list[EspRecord]) -> None:
    books = [record for record in records if record.signature == "BOOK"]
    assert len(books) == generator.COURIER_BOOK_COUNT
    expected_ids = list(range(
        generator.COURIER_BOOK_FIRST_LOCAL_ID,
        generator.COURIER_BOOK_FIRST_LOCAL_ID + generator.COURIER_BOOK_COUNT,
    ))
    assert [record.form_id & 0xFFFFFF for record in books] == expected_ids
    for index, book in enumerate(books):
        fields = parse_fields(book.body)
        assert [signature for signature, _value in fields] == [
            "EDID", "OBND", "FULL", "MODL", "YNAM", "KSIZ", "KWDA",
            "DESC", "DATA", "INAM", "CNAM",
        ]
        assert ztext(one_field(fields, "EDID")) == (
            f"AOS_CourierNotice{index + 1:03d}"
        )
        assert ztext(one_field(fields, "FULL")) == "Smithing Order Notice"
        assert ztext(one_field(fields, "MODL")) == "Clutter\\Books\\Note03.nif"
        assert one_field(fields, "YNAM") == generator.form_bytes(
            generator.master_ref(generator.SKYRIM, 0x0C7A54)
        )
        assert one_field(fields, "KWDA") == generator.form_bytes(
            generator.master_ref(generator.SKYRIM, 0x0937A2)
        )
        assert one_field(fields, "INAM") == generator.form_bytes(
            generator.master_ref(generator.SKYRIM, 0x01541C)
        )
        assert one_field(fields, "DATA") == bytes.fromhex(
            "00000000ffffffff0000000000000000"
        )
        assert b"$HandwrittenFont" in one_field(fields, "DESC")


def normalized_record(record: EspRecord) -> tuple[str, int, bytes, tuple[tuple[str, bytes], ...]]:
    fields = parse_fields(record.body)
    normalized_fields: list[tuple[str, bytes]] = []
    for signature, value in fields:
        if record.signature == "ALCH" and signature == "ENIT":
            value = value[:4] + b"\0\0\0\0" + value[8:]
        normalized_fields.append((signature, value))
    return record.signature, record.form_id, record.header, tuple(normalized_fields)


def compare_with_baseline(current: list[EspRecord], baseline_path: Path) -> None:
    baseline = list(walk_records(baseline_path.read_bytes()))
    assert len(current) == len(baseline), "ESP record count changed from baseline"
    for index, (before, after) in enumerate(zip(baseline, current, strict=True)):
        assert normalized_record(before) == normalized_record(after), (
            f"Unexpected change outside ALCH ENIT flags at record {index}: "
            f"{before.signature} {before.form_id:08X}"
        )
    before_alch = [record for record in baseline if record.signature == "ALCH"]
    after_alch = [record for record in current if record.signature == "ALCH"]
    assert len(before_alch) == EXPECTED_ALCH_COUNT
    changed = 0
    for before, after in zip(before_alch, after_alch, strict=True):
        before_flags = read_u32(one_field(parse_fields(before.body), "ENIT"), 4)
        after_flags = read_u32(one_field(parse_fields(after.body), "ENIT"), 4)
        assert after_flags == (before_flags | generator.ALCH_FLAG_NO_AUTO_CALC)
        if before_flags != after_flags:
            changed += 1
    assert changed == EXPECTED_ALCH_COUNT, (
        f"Expected {EXPECTED_ALCH_COUNT} ENIT flag changes, found {changed}"
    )
    print(f"Baseline comparison: only {changed} ALCH ENIT flag fields changed")


def validate_magnitude_contract() -> None:
    for name in generator.MGEF:
        if name in generator.MAGNITUDELESS_EFFECTS:
            generator.validate_effect_magnitude(name, 0)
            continue
        for invalid in (0, -1, float("nan"), float("inf")):
            try:
                generator.validate_effect_magnitude(name, invalid)
            except ValueError:
                continue
            raise AssertionError(f"{name}: accepted invalid magnitude {invalid}")
    print("Magnitude guard: zero/negative/non-finite values rejected; duration-only effects allowed")


def validate(esp_path: Path, baseline_path: Path | None = None) -> None:
    validate_magnitude_contract()
    records = list(walk_records(esp_path.read_bytes()))
    validate_courier_books(records)
    alch_records = [record for record in records if record.signature == "ALCH"]
    items = generator.read_alchemy_records()
    assert len(items) == EXPECTED_ALCH_COUNT, (
        f"Alchemy source count {len(items)} != {EXPECTED_ALCH_COUNT}"
    )
    assert tuple(item.editor_id for item in items) == EXPECTED_ALCH_EDITOR_IDS, (
        "Alchemy source EditorID order changed unexpectedly"
    )
    assert all(
        item.display_name.startswith(f"{item.item_type} of ")
        for item in items
    ), "Public alchemy names must use 'Potion of ...' or 'Poison of ...'"
    assert len(alch_records) == EXPECTED_ALCH_COUNT, (
        f"ESP ALCH count {len(alch_records)} != {EXPECTED_ALCH_COUNT}"
    )
    for record, item in zip(alch_records, items, strict=True):
        validate_item(record, item)
    validate_resale_safety(items)

    overtime = [item for item in items if "OverTime" in item.editor_id]
    assert len(overtime) == 9
    expected_values = {"Adept": 30, "Expert": 60, "Master": 120}
    expected_magnitudes = {"Adept": 10.0, "Expert": 20.0, "Master": 40.0}
    for item in overtime:
        assert len(item.effects) == 1
        _effect, magnitude, duration = item.effects[0]
        assert duration == 10
        assert magnitude == expected_magnitudes[item.tier]
        assert item.alch_value == expected_values[item.tier]

    local_ids = [record.form_id & 0xFFFFFF for record in alch_records]
    assert local_ids == list(range(0x0A00, 0x0A3F)), (
        "ALCH FormID range/order is not 000A00..000A3E"
    )
    actual_editor_ids = tuple(
        ztext(one_field(parse_fields(record.body), "EDID")) for record in alch_records
    )
    assert actual_editor_ids == EXPECTED_ALCH_EDITOR_IDS, (
        "ESP ALCH EditorID/FormID order changed unexpectedly"
    )

    by_editor_id = {
        ztext(one_field(parse_fields(record.body), "EDID")): record
        for record in alch_records
    }
    potion = by_editor_id["AOS_ALCH_PotionRestoreHealthMaster"]
    potion_fields = parse_fields(potion.body)
    potion_enit = one_field(potion_fields, "ENIT")
    potion_efit = [value for signature, value in potion_fields if signature == "EFIT"]
    assert potion.form_id & 0xFFFFFF == 0x0A26
    assert read_u32(potion_enit, 0) == 110
    assert read_u32(potion_enit, 4) == generator.ALCH_FLAG_NO_AUTO_CALC
    assert len(potion_efit) == 1 and read_f32(potion_efit[0], 0) == 200.0

    poison = by_editor_id["AOS_ALCH_PoisonDamageHealthAdept"]
    poison_enit = one_field(parse_fields(poison.body), "ENIT")
    assert poison.form_id & 0xFFFFFF == 0x0A00
    assert read_u32(poison_enit, 0) == 30
    assert read_u32(poison_enit, 4) == (
        generator.ALCH_FLAG_NO_AUTO_CALC | generator.ALCH_FLAG_POISON
    )

    poison_count = sum(item.item_type == "Poison" for item in items)
    potion_count = len(items) - poison_count
    print(f"Validated {len(alch_records)} ALCH records in {esp_path}")
    print(f"Types: potions={potion_count}, poisons={poison_count}")
    print("FormIDs/order: 000A00..000A3E")
    print("Values, weights, names, models, keywords, effects, magnitudes and durations: OK")
    print("Flags: every ALCH has No Auto-Calc; Poison is present only on poisons")
    print(
        f"Courier letters: {generator.COURIER_BOOK_COUNT} unique BOOK records "
        "with note assets and handwritten fallback text: OK"
    )
    if baseline_path is not None:
        compare_with_baseline(records, baseline_path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--esp", type=Path, default=generator.ESP_PATH)
    parser.add_argument("--baseline", type=Path)
    args = parser.parse_args()
    validate(args.esp, args.baseline)


if __name__ == "__main__":
    main()
