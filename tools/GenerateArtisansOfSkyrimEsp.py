#!/usr/bin/env python3
"""Generate the unified ArtisansOfSkyrim.esp from the source catalog.

This tool intentionally emits a small, auditable plugin instead of asking the
maintainer to create the dialogue quest, courier BOOK pool, and ALCH records by hand. Dialogue
responses are selected from a reviewed source-data table and reuse vanilla
SharedInfo records, so the game supplies the matching subtitle and voice asset.
"""

from __future__ import annotations

import csv
import math
import json
import re
import struct
import tomllib
from dataclasses import dataclass, replace
from pathlib import Path

import project_version


ROOT = Path(__file__).resolve().parents[1]
PLUGIN_NAME = "ArtisansOfSkyrim.esp"
PLUGIN_INDEX = 0x03

CSV_PATH = ROOT / "source-data" / "alchemy" / "alchemy_records.csv"
CATALOG_PATH = ROOT / "source-data" / "alchemy" / "alchemy_catalog.csv"
NPC_TIERS_PATH = ROOT / "source-data" / "npc_tiers.csv"
VOICE_RESPONSES_PATH = ROOT / "source-data" / "dialogue" / "vanilla_voice_responses.csv"
CONFIG_PATH = ROOT / "config" / "ArtisansOfSkyrim.toml"
DIST_CONFIG_PATH = ROOT / "dist" / "Data" / "SKSE" / "Plugins" / "ArtisansOfSkyrim.toml"
ESP_PATH = ROOT / "dist" / "Data" / PLUGIN_NAME
SEQ_PATH = ROOT / "dist" / "Data" / "SEQ" / "ArtisansOfSkyrim.seq"
MANIFEST_PATH = ROOT / "source-data" / "esp_records_manifest.csv"
FORMID_EXPORT_PATH = ROOT / "source-data" / "alchemy" / "alchemy_formids.csv"
SNAPSHOT_PATH = ROOT / "source-data" / "vanilla_record_snapshots.json"
COURIER_BOOK_FIRST_LOCAL_ID = 0x0B00
COURIER_BOOK_COUNT = 1024
ORDER_AVAILABILITY_GLOBAL_LOCAL_ID = 0x084F
SMITH_SERVICES_GLOBAL_LOCAL_ID = 0x08F0
SERVICE_ACCESS_GLOBALS = {
    "Smithing": (0x08F1, "AOS_SmithAccessForCurrentSpeaker"),
    "Enchanting": (0x08F2, "AOS_EnchantAccessForCurrentSpeaker"),
    "Alchemy": (0x08F3, "AOS_AlchemyAccessForCurrentSpeaker"),
}
COLLECTION_INFO_FIRST_LOCAL_ID = 0x0900
# Fallback INFOs: any supported artisan whose voice type has no matching entry
# (for example mod-added NPCs). They come last in each topic, so actor- and
# voice-specific responses keep priority. 0x0930-0x0933 stay retired.
FALLBACK_INFO_LOCAL_IDS = {
    "Smithing Craft": 0x0940,
    "Smithing Improve": 0x0941,
    "Enchanting": 0x0942,
    "Alchemy": 0x0943,
}
# Generic vanilla replies reused for the fallback. Voice types that have the
# line play it; the others show the subtitle and continue normally.
FALLBACK_SHARED_RESPONSES = {
    "Smithing Craft": ("Skyrim.esm", 0x0A9636),
    "Smithing Improve": ("Skyrim.esm", 0x0DBA22),
    "Enchanting": ("Skyrim.esm", 0x07F6BC),
    "Alchemy": ("Skyrim.esm", 0x07F6BC),
}
NELOTH_ACTOR = ("Dragonborn.esm", 0x01773A)
# Optional MCM Helper quest; free slot between the dialogue topics and INFOs.
MCM_QUEST_LOCAL_ID = 0x08F6
PLAYER_REF = 0x00000014
MGEF_ARCHETYPE_PEAK_VALUE_MODIFIER = 34

EXCLUSIVE_FORMS = {
    "mgef_silver_blessing": 0x0F00,
    "mgef_elemental_burst": 0x0F01,
    "mgef_berserker_soul": 0x0F02,
    "mgef_honed_metal": 0x0F03,
    "mgef_unburden": 0x0F04,
    "mgef_reinforcement": 0x0F05,
    "ench_silver_blessing": 0x0F06,
    "ench_elemental_burst": 0x0F07,
    "ench_berserker_soul": 0x0F08,
    "ench_honed_metal": 0x0F09,
    "ench_unburden": 0x0F0A,
    "ench_reinforcement": 0x0F0B,
    "mgef_health_over_time": 0x0F0C,
    "mgef_magicka_over_time": 0x0F0D,
    "mgef_stamina_over_time": 0x0F0E,
    "keyword_health_over_time": 0x0F0F,
    "keyword_magicka_over_time": 0x0F10,
    "keyword_stamina_over_time": 0x0F11,
    "perk_unburden_master": 0x0F12,
    "perk_reinforcement_master": 0x0F13,
    "mgef_silver_master": 0x0F18,
    "perk_elemental_expert": 0x0F19,
    "perk_elemental_master": 0x0F1A,
    "skyforge_mace": 0x0F1B,
    "skyforge_warhammer": 0x0F1C,
    "skyforge_plate_boots": 0x0F1D,
    "skyforge_plate_cuirass": 0x0F1E,
    "skyforge_plate_gauntlets": 0x0F1F,
    "skyforge_plate_helmet": 0x0F20,
    "temper_skyforge_mace": 0x0F21,
    "temper_skyforge_warhammer": 0x0F22,
    "temper_skyforge_plate_boots": 0x0F23,
    "temper_skyforge_plate_cuirass": 0x0F24,
    "temper_skyforge_plate_gauntlets": 0x0F25,
    "temper_skyforge_plate_helmet": 0x0F26,
    "list_ring": 0x0F27,
    "list_cuirass": 0x0F28,
    "skyforge_shield": 0x0F29,
    "temper_skyforge_shield": 0x0F2A,
    "perk_unburden_expert": 0x0F2B,
    "perk_reinforcement_expert": 0x0F2C,
    "perk_elemental_combined": 0x0F2D,
    "perk_elemental_maximum": 0x0F2E,
    "skyforge_dagger": 0x0F31,
    "skyforge_sword": 0x0F32,
    "skyforge_war_axe": 0x0F33,
    "skyforge_greatsword": 0x0F34,
    "skyforge_battleaxe": 0x0F35,
    "temper_skyforge_dagger": 0x0F36,
    "temper_skyforge_sword": 0x0F37,
    "temper_skyforge_war_axe": 0x0F38,
    "temper_skyforge_greatsword": 0x0F39,
    "temper_skyforge_battleaxe": 0x0F3A,
}

# Supported service magnitudes. Apparel PERK multipliers and direct weapon-hit
# calculations are derived from this table; the runtime separately rejects a
# config whose six exclusive enchantments do not match these values.
EXCLUSIVE_MAGNITUDES = {
    "silver_blessing": {"Expert": 25.0, "Master": 50.0},
    "berserker_soul": {"Expert": 20.0, "Master": 40.0},
    "honed_metal": {"Expert": 15.0, "Master": 30.0},
    "unburden": {"Expert": 15.0, "Master": 30.0},
    "reinforcement": {"Expert": 20.0, "Master": 40.0},
    "elemental_burst": {"Expert": 25.0, "Master": 50.0},
}


def bonus_multiplier(effect: str, tier: str) -> float:
    return 1.0 + EXCLUSIVE_MAGNITUDES[effect][tier] / 100.0


def reduction_multiplier(effect: str, tier: str) -> float:
    return 1.0 - EXCLUSIVE_MAGNITUDES[effect][tier] / 100.0

DUAL_PUBLIC_NAMES = {
    "poison_damage_health_plus_lingering_damage_health_master": "Poison of Lingering Damage",
    "potion_restore_magicka_plus_regenerate_magicka_master": "Potion of Magicka Recovery",
    "potion_restore_magicka_plus_fortify_destruction_master": "Potion of Destruction Focus",
    "potion_restore_magicka_plus_fortify_illusion_master": "Potion of Illusion Focus",
    "potion_restore_stamina_plus_regenerate_stamina_master": "Potion of Stamina Recovery",
}


def zstring(value: str) -> bytes:
    return value.encode("utf-8") + b"\0"


def u16(value: int) -> bytes:
    return struct.pack("<H", value)


def u32(value: int) -> bytes:
    return struct.pack("<I", value)


def f32(value: float) -> bytes:
    return struct.pack("<f", value)


def local_to_form(local_id: int) -> int:
    return (PLUGIN_INDEX << 24) | local_id


def field(signature: str, data: bytes) -> bytes:
    if len(signature) != 4:
        raise ValueError(signature)
    if len(data) <= 0xFFFF:
        return signature.encode("ascii") + u16(len(data)) + data
    return b"XXXX" + u16(4) + u32(len(data)) + signature.encode("ascii") + u16(0) + data


def record(signature: str, form_id: int, body: bytes, flags: int = 0) -> bytes:
    return (
        signature.encode("ascii")
        + u32(len(body))
        + u32(flags)
        + u32(form_id)
        + u32(0)
        + u16(44)
        + u16(0)
        + body
    )


def group(label: bytes, group_type: int, contents: bytes) -> bytes:
    if len(label) != 4:
        raise ValueError(label)
    return b"GRUP" + u32(len(contents) + 24) + label + u32(group_type) + u32(0) + u32(0) + contents


def form_bytes(form_id: int) -> bytes:
    return u32(form_id)


def ctda(
    function_index: int,
    parameter_1: int,
    run_on: int = 0,
    reference: int = 0,
    flags: int = 0,
    comparison: float = 1.0,
) -> bytes:
    # Matches the CTDA layout used by the original dialogue ESPs:
    # Default comparison to 1.0, Run On Subject, no second parameter.
    return (
        u32(flags)
        + f32(comparison)
        + u32(function_index)
        + u32(parameter_1)
        + u32(0)
        + u32(run_on)
        + u32(reference)
        + u32(0xFFFFFFFF)
    )


def vmad_fragment(script_name: str) -> bytes:
    name = zstringless(script_name)
    fragment = zstringless("Fragment_0")
    # VMAD version 5, object format 2, one attached script and one fragment.
    return (
        u16(5)
        + u16(2)
        + u16(1)
        + u16(len(name))
        + name
        + bytes([0])
        + u16(0)
        + bytes([2, 2])
        + u16(len(name))
        + name
        + bytes([1])
        + u16(len(name))
        + name
        + u16(len(fragment))
        + fragment
    )


def zstringless(value: str) -> bytes:
    return value.encode("utf-8")


@dataclass(frozen=True)
class DialogueTopic:
    service: str
    branch_local: int
    dial_local: int
    branch_edid: str
    topic_edid: str
    topic_text: str
    fragment_script: str
    responses: list["DialogueResponse"]
    quest_form_id: int = 0


@dataclass(frozen=True)
class DialogueResponse:
    info_local_id: int
    selector_type: str
    selector_name: str
    shared_info_form_id: int
    response_plugin: str
    response_local_id: int
    response_text: str
    voice_type: str
    # (function, parameter, run on, reference[, comparison value])
    conditions: tuple[tuple, ...]
    access_global: int = 0


@dataclass
class AlchemyRecord:
    local_id: int
    editor_id: str
    display_name: str
    item_type: str
    tier: str
    group: str
    batch_size: int
    batch_price: int
    alch_value: int
    weight: float
    effects: list[tuple[str, float, int]]
    source_id: str

    @property
    def form_id(self) -> int:
        return local_to_form(self.local_id)

    @property
    def form_key(self) -> str:
        return f"{PLUGIN_NAME}|{self.local_id:06X}"


@dataclass(frozen=True)
class NpcDialogueCondition:
    display_name: str
    service: str
    plugin: str
    local_id: int

    @property
    def primary_key(self) -> str:
        return f"{self.plugin}|{self.local_id:08X}"

    @property
    def form_id(self) -> int:
        master_indices = {
            "Skyrim.esm": SKYRIM,
            "Dawnguard.esm": DAWNGUARD,
            "Dragonborn.esm": DRAGONBORN,
        }
        try:
            master_index = master_indices[self.plugin]
        except KeyError as exc:
            raise ValueError(
                f"Unsupported NPC master '{self.plugin}' for {self.display_name}"
            ) from exc
        return master_ref(master_index, self.local_id)


SKYRIM = 0x00
DAWNGUARD = 0x01
DRAGONBORN = 0x02


def master_ref(master_index: int, local_id: int) -> int:
    return (master_index << 24) | local_id


FORMS = {
    "JobBlacksmithFaction": master_ref(SKYRIM, 0x05091D),
    "JobApothecaryFaction": master_ref(SKYRIM, 0x05091C),
    "Ulfberth": master_ref(SKYRIM, 0x013B9F),
    "Beirand": master_ref(SKYRIM, 0x013261),
    "Ghorza": master_ref(SKYRIM, 0x01339A),
    "Farengar": master_ref(SKYRIM, 0x013BBB),
    "Calcelmo": master_ref(SKYRIM, 0x01338E),
    "Sergius": master_ref(SKYRIM, 0x01C23E),
    "Sybille": master_ref(SKYRIM, 0x0132AA),
    "Neloth": master_ref(DRAGONBORN, 0x01773A),
    "FeranSadri": master_ref(DAWNGUARD, 0x003366),
    "Florentius": master_ref(DAWNGUARD, 0x00336D),
    "FlorentiusRecruitmentQuest": master_ref(DAWNGUARD, 0x013F1F),
    "Babette": master_ref(SKYRIM, 0x01D4B7),
    "Bothela": master_ref(SKYRIM, 0x01338B),
    "Nurelion": master_ref(SKYRIM, 0x014148),
    "Arcadia": master_ref(SKYRIM, 0x013BA4),
    "MiloreIenth": master_ref(DRAGONBORN, 0x0182A1),
    "Elgrim": master_ref(SKYRIM, 0x013357),
    "Frida": master_ref(SKYRIM, 0x013614),
    "QuintusNavale": master_ref(SKYRIM, 0x01414C),
}


MGEF = {
    "Damage Health": master_ref(SKYRIM, 0x03EB42),
    "Lingering Damage Health": master_ref(SKYRIM, 0x10AA4A),
    "Damage Magicka": master_ref(SKYRIM, 0x03A2B6),
    "Damage Stamina": master_ref(SKYRIM, 0x03A2C6),
    "Fear": master_ref(SKYRIM, 0x073F20),
    "Frenzy": master_ref(SKYRIM, 0x073F29),
    "Paralysis": master_ref(SKYRIM, 0x073F30),
    "Slow": master_ref(SKYRIM, 0x073F25),
    "Fortify Destruction": master_ref(SKYRIM, 0x03EB26),
    "Fortify Illusion": master_ref(SKYRIM, 0x03EB27),
    "Fortify Archery": master_ref(SKYRIM, 0x03EB1B),
    "Fortify One-Handed": master_ref(SKYRIM, 0x03EB19),
    "Fortify Two-Handed": master_ref(SKYRIM, 0x03EB1A),
    "Fortify Sneak": master_ref(SKYRIM, 0x03EB22),
    "Fortify Carry Weight": master_ref(SKYRIM, 0x03EB01),
    "Resist Fire": master_ref(SKYRIM, 0x03EAEA),
    "Resist Frost": master_ref(SKYRIM, 0x03EAEB),
    "Resist Shock": master_ref(SKYRIM, 0x03EAEC),
    "Resist Magic": master_ref(SKYRIM, 0x039E51),
    "Resist Poison": master_ref(SKYRIM, 0x090041),
    "Restore Health": master_ref(SKYRIM, 0x03EB15),
    "Restore Magicka": master_ref(SKYRIM, 0x03EB17),
    "Restore Stamina": master_ref(SKYRIM, 0x03EB16),
    "Regenerate Magicka": master_ref(SKYRIM, 0x03EB07),
    "Regenerate Stamina": master_ref(SKYRIM, 0x03EB08),
    "Cure Disease": master_ref(SKYRIM, 0x0AE722),
    "Waterbreathing": master_ref(SKYRIM, 0x03AC2D),
    "Invisibility": master_ref(SKYRIM, 0x03EB3D),
    "Health Over Time": local_to_form(EXCLUSIVE_FORMS["mgef_health_over_time"]),
    "Magicka Over Time": local_to_form(EXCLUSIVE_FORMS["mgef_magicka_over_time"]),
    "Stamina Over Time": local_to_form(EXCLUSIVE_FORMS["mgef_stamina_over_time"]),
}


MGEF_EDITOR_IDS = {
    "Damage Health": "AlchDamageHealth",
    "Lingering Damage Health": "AlchDamageHealthDuration",
    "Damage Magicka": "AlchDamageMagicka",
    "Damage Stamina": "AlchDamageStamina",
    "Fear": "AlchInfluenceConfDown",
    "Frenzy": "AlchInfluenceAggUp",
    "Paralysis": "AlchParalysis",
    "Slow": "AlchDamageSpeed",
    "Fortify Destruction": "AlchFortifyDestruction",
    "Fortify Illusion": "AlchFortifyIllusion",
    "Fortify Archery": "AlchFortifyMarksman",
    "Fortify One-Handed": "AlchFortifyOneHanded",
    "Fortify Two-Handed": "AlchFortifyTwoHanded",
    "Fortify Sneak": "AlchFortifySneak",
    "Fortify Carry Weight": "AlchFortifyCarryWeight",
    "Resist Fire": "AlchResistFire",
    "Resist Frost": "AlchResistFrost",
    "Resist Shock": "AlchResistShock",
    "Resist Magic": "AlchResistMagic",
    "Resist Poison": "AlchResistPoison",
    "Restore Health": "AlchRestoreHealth",
    "Restore Magicka": "AlchRestoreMagicka",
    "Restore Stamina": "AlchRestoreStamina",
    "Regenerate Magicka": "AlchFortifyMagickaRate",
    "Regenerate Stamina": "AlchFortifyStaminaRate",
    "Cure Disease": "AlchCureDisease",
    "Waterbreathing": "AlchWaterbreathing",
    "Invisibility": "AlchInvisibillity",
    "Health Over Time": "AOS_MGEF_HealthOverTime",
    "Magicka Over Time": "AOS_MGEF_MagickaOverTime",
    "Stamina Over Time": "AOS_MGEF_StaminaOverTime",
}

POTION_OBND = bytes.fromhex("fcfffcff0000040004001000")
POISON_OBND = bytes.fromhex("fdfffdff0000030003000f00")
POTION_KWDA = form_bytes(master_ref(SKYRIM, 0x08CDEC))
POISON_KWDA = form_bytes(master_ref(SKYRIM, 0x08CDED))
POTION_MODEL = r"Clutter\Potions\PotionHealthLesser.nif"
POISON_MODEL = r"Clutter\Potions\PoisonDamageHealth01.nif"
INVISIBILITY_MODEL = r"Clutter\Potions\PotionInvisibiltyLesser.nif"
POTION_MODT = bytes.fromhex(
    "02000000050000000000000063df5c0164647300bffa25dab9fb95cf64647300"
    "bffa25dafae0bba4646473007f66a5c0ce353c0964647300262c333bfd5a8562"
    "64647300bffa25da"
)
POISON_MODT = bytes.fromhex(
    "02000000090000000000000063df5c0164647300bffa25dab9fb95cf64647300"
    "bffa25dafae0bba4646473007f66a5c0ce353c0964647300262c333bfd5a8562"
    "64647300bffa25daf9be93cc64647300bffa25da28058e7e64647300bffa25da"
    "f63890ac64647300262c333bd1e23ce564647300bffa25da"
)
PICKUP_SOUND = form_bytes(master_ref(SKYRIM, 0x03EDBD))
PUTDOWN_SOUND = form_bytes(master_ref(SKYRIM, 0x03EDC0))
ALCH_FLAG_NO_AUTO_CALC = 0x00000001
ALCH_FLAG_POISON = 0x00020000
POTION_ENIT_TAIL = bytes.fromhex("000000000000000035640b00")
POISON_ENIT_TAIL = bytes.fromhex("000000000000000014661000")


def read_snapshots() -> dict[str, dict[str, object]]:
    with SNAPSHOT_PATH.open("r", encoding="utf-8") as handle:
        return json.load(handle)


SNAPSHOTS = read_snapshots()


def snapshot_fields(form_id: int) -> list[tuple[str, bytes]]:
    try:
        entry = SNAPSHOTS[f"{form_id:08X}"]
    except KeyError as exc:
        raise ValueError(f"Missing vanilla snapshot {form_id:08X}") from exc
    return [
        (item["signature"], bytes.fromhex(item["data"]))
        for item in entry["fields"]
    ]


def snapshot_flags(form_id: int) -> int:
    try:
        return int(SNAPSHOTS[f"{form_id:08X}"]["flags"])
    except KeyError as exc:
        raise ValueError(f"Missing vanilla snapshot {form_id:08X}") from exc


def assemble_fields(entries: list[tuple[str, bytes]]) -> bytes:
    return b"".join(field(signature, data) for signature, data in entries)


def replace_first(
    entries: list[tuple[str, bytes]],
    signature: str,
    data: bytes,
) -> list[tuple[str, bytes]]:
    result = list(entries)
    for index, (candidate, _old) in enumerate(result):
        if candidate == signature:
            result[index] = (signature, data)
            return result
    raise ValueError(f"Snapshot has no {signature} field")


def replace_or_insert_after(
    entries: list[tuple[str, bytes]],
    signature: str,
    data: bytes,
    after_signature: str,
) -> list[tuple[str, bytes]]:
    result = list(entries)
    for index, (candidate, _old) in enumerate(result):
        if candidate == signature:
            result[index] = (signature, data)
            return result
    for index, (candidate, _old) in enumerate(result):
        if candidate == after_signature:
            result.insert(index + 1, (signature, data))
            return result
    raise ValueError(f"Snapshot has no {after_signature} field for {signature} insertion")


def drop_fields(
    entries: list[tuple[str, bytes]],
    signatures: set[str],
) -> list[tuple[str, bytes]]:
    return [(signature, data) for signature, data in entries if signature not in signatures]


def clone_record(
    source_form_id: int,
    signature: str,
    local_id: int,
    editor_id: str,
    display_name: str | None = None,
    replacements: dict[str, bytes] | None = None,
    drop: set[str] | None = None,
) -> bytes:
    entries = snapshot_fields(source_form_id)
    if drop:
        entries = drop_fields(entries, drop)
    entries = replace_first(entries, "EDID", zstring(editor_id))
    if display_name is not None:
        entries = replace_first(entries, "FULL", zstring(display_name))
    for field_signature, data in (replacements or {}).items():
        entries = replace_first(entries, field_signature, data)
    return record(
        signature,
        local_to_form(local_id),
        assemble_fields(entries),
        flags=snapshot_flags(source_form_id),
    )


def marker_mgef_record(
    local_id: int,
    editor_id: str,
    display_name: str,
    description: str,
    weapon: bool,
) -> bytes:
    # Weapon markers keep the proven Fire-and-Forget/Contact layout of
    # VoiceDragonrendBlankHostile. Apparel markers keep the Constant
    # Effect/Self layout of EnchFortifyDestructionConstantSelf. Both are
    # converted to an inert Script archetype with no actor value, so the
    # created enchantment remains visible and mechanically harmless while the
    # Artisans runtime/perks read its unique marker.
    source_form_id = 0x00017192 if weapon else 0x0007A0F6
    entries = snapshot_fields(source_form_id)
    entries = drop_fields(entries, {"CTDA", "SNDD", "VMAD", "KSIZ", "KWDA"})
    entries = replace_first(entries, "EDID", zstring(editor_id))
    entries = replace_or_insert_after(
        entries,
        "FULL",
        zstring(display_name),
        "EDID",
    )
    data = bytearray(next(value for signature, value in entries if signature == "DATA"))
    if len(data) != 152:
        raise ValueError(f"Unexpected MGEF DATA size for {editor_id}: {len(data)}")
    flags = 0x00000010 | 0x00000200 | 0x00000800 | 0x08000000
    if weapon:
        flags |= 0x00000001 | 0x00000004
    struct.pack_into("<I", data, 0, flags)
    struct.pack_into("<f", data, 4, 1.0)
    struct.pack_into("<I", data, 8, 0)
    struct.pack_into("<i", data, 12, -1)
    struct.pack_into("<i", data, 16, -1)
    for offset in (24, 32, 36, 72, 76, 92, 96, 100, 108, 116, 120, 124, 128, 132, 136):
        struct.pack_into("<I", data, offset, 0)
    struct.pack_into("<I", data, 64, 1)  # Script archetype
    struct.pack_into("<i", data, 68, -1)
    struct.pack_into("<i", data, 88, -1)
    entries = replace_first(entries, "DATA", bytes(data))
    entries = replace_first(
        entries,
        "DNAM",
        zstring(description),
    )
    return record("MGEF", local_to_form(local_id), assemble_fields(entries))


def overtime_mgef_record(
    source_form_id: int,
    local_id: int,
    editor_id: str,
    display_name: str,
    primary_actor_value: int,
    keyword_local_id: int,
) -> bytes:
    entries = snapshot_fields(source_form_id)
    entries = drop_fields(entries, {"KSIZ", "KWDA"})
    entries = replace_first(entries, "EDID", zstring(editor_id))
    entries = replace_first(entries, "FULL", zstring(display_name))
    entries = replace_or_insert_after(entries, "KSIZ", u32(1), "FULL")
    entries = replace_or_insert_after(
        entries,
        "KWDA",
        form_bytes(local_to_form(keyword_local_id)),
        "KSIZ",
    )
    data = bytearray(next(value for signature, value in entries if signature == "DATA"))
    if len(data) != 152:
        raise ValueError(f"Unexpected duration MGEF DATA size for {editor_id}: {len(data)}")
    # Preserve the vanilla duration-food delivery and direct restoration
    # behavior, but use the same Peak Value Modifier exclusivity mechanism as
    # AlchFortifyDestruction. Tiers in one family share a private associated
    # keyword, so only the highest active magnitude applies; equal doses do not
    # add together, while Health, Magicka, and Stamina remain independent.
    struct.pack_into("<I", data, 8, local_to_form(keyword_local_id))
    struct.pack_into("<I", data, 64, MGEF_ARCHETYPE_PEAK_VALUE_MODIFIER)
    struct.pack_into("<i", data, 68, primary_actor_value)
    entries = replace_first(entries, "DATA", bytes(data))
    entries = replace_first(
        entries,
        "DNAM",
        zstring(
            f"Restores <mag> points of "
            f"{display_name.removesuffix(' Over Time').lower()} per second for <dur> seconds."
        ),
    )
    return record("MGEF", local_to_form(local_id), assemble_fields(entries))


def exclusive_keyword_records() -> list[tuple[str, str, int, bytes]]:
    specs = [
        ("keyword_health_over_time", "AOS_KYWD_HealthOverTime"),
        ("keyword_magicka_over_time", "AOS_KYWD_MagickaOverTime"),
        ("keyword_stamina_over_time", "AOS_KYWD_StaminaOverTime"),
    ]
    return [
        (
            "KYWD",
            editor_id,
            EXCLUSIVE_FORMS[key],
            record(
                "KYWD",
                local_to_form(EXCLUSIVE_FORMS[key]),
                field("EDID", zstring(editor_id)),
            ),
        )
        for key, editor_id in specs
    ]


def exclusive_mgef_records() -> list[tuple[str, str, int, bytes]]:
    result: list[tuple[str, str, int, bytes]] = []
    markers = [
        ("mgef_silver_blessing", "AOS_MGEF_SilverBlessing", "Silver Blessing", "Attacks with this weapon do <mag>% more damage against undead and werebeasts.", True),
        ("mgef_silver_master", "AOS_MGEF_SilverBlessingMaster", "Silver Blessing", "Attacks with this weapon do <mag>% more damage against undead and werebeasts.", True),
        ("mgef_elemental_burst", "AOS_MGEF_ElementalBurst", "Elemental Burst", "Fire, Frost, and Shock spells are <mag>% stronger.", False),
        ("mgef_berserker_soul", "AOS_MGEF_BerserkerSoul", "Berserker Soul", "Attacks with this weapon ignore <mag>% of armor.", True),
        ("mgef_honed_metal", "AOS_MGEF_HonedMetal", "Honed Metal", "Attacks with this weapon do <mag>% more damage.", True),
        ("mgef_unburden", "AOS_MGEF_Unburden", "Unburden", "Worn armor weighs nothing and does not slow you down. Sprinting and power attacks cost <mag>% less Stamina.", False),
        ("mgef_reinforcement", "AOS_MGEF_Reinforcement", "Reinforcement", "You stagger <mag>% less.", False),
    ]
    for key, editor_id, display_name, description, weapon in markers:
        local_id = EXCLUSIVE_FORMS[key]
        result.append((
            "MGEF",
            editor_id,
            local_id,
            marker_mgef_record(local_id, editor_id, display_name, description, weapon),
        ))
    overtime = [
        (
            0x001058A2,
            "mgef_health_over_time",
            "keyword_health_over_time",
            "AOS_MGEF_HealthOverTime",
            "Health Over Time",
            24,
        ),
        (
            0x001058A7,
            "mgef_magicka_over_time",
            "keyword_magicka_over_time",
            "AOS_MGEF_MagickaOverTime",
            "Magicka Over Time",
            25,
        ),
        (
            0x001058A3,
            "mgef_stamina_over_time",
            "keyword_stamina_over_time",
            "AOS_MGEF_StaminaOverTime",
            "Stamina Over Time",
            26,
        ),
    ]
    for source, key, keyword_key, editor_id, name, actor_value in overtime:
        local_id = EXCLUSIVE_FORMS[key]
        result.append((
            "MGEF",
            editor_id,
            local_id,
            overtime_mgef_record(
                source,
                local_id,
                editor_id,
                name,
                actor_value,
                EXCLUSIVE_FORMS[keyword_key],
            ),
        ))
    return result


def form_list_record(local_id: int, editor_id: str, forms: list[int]) -> bytes:
    body = field("EDID", zstring(editor_id))
    for form_id in forms:
        body += field("LNAM", form_bytes(form_id))
    return record("FLST", local_to_form(local_id), body)


def exclusive_ench_record(
    local_id: int,
    editor_id: str,
    display_name: str,
    marker_local_id: int,
    weapon: bool,
    worn_list_local_id: int | None = None,
) -> bytes:
    entries = snapshot_fields(0x0010FB95 if weapon else 0x0010FB75)
    entries = replace_first(entries, "EDID", zstring(editor_id))
    entries = replace_first(entries, "FULL", zstring(display_name))
    enit = bytearray(next(value for signature, value in entries if signature == "ENIT"))
    if worn_list_local_id is not None:
        struct.pack_into("<I", enit, 32, local_to_form(worn_list_local_id))
    entries = replace_first(entries, "ENIT", bytes(enit))
    entries = replace_first(entries, "EFID", form_bytes(local_to_form(marker_local_id)))
    entries = replace_first(entries, "EFIT", f32(1.0) + u32(0) + u32(0))
    return record("ENCH", local_to_form(local_id), assemble_fields(entries))


def exclusive_ench_records() -> list[tuple[str, str, int, bytes]]:
    specs = [
        ("ench_silver_blessing", "AOS_ENCH_SilverBlessing", "Silver Blessing", "mgef_silver_blessing", True, None),
        ("ench_elemental_burst", "AOS_ENCH_ElementalBurst", "Elemental Burst", "mgef_elemental_burst", False, "list_ring"),
        ("ench_berserker_soul", "AOS_ENCH_BerserkerSoul", "Berserker Soul", "mgef_berserker_soul", True, None),
        ("ench_honed_metal", "AOS_ENCH_HonedMetal", "Honed Metal", "mgef_honed_metal", True, None),
        ("ench_unburden", "AOS_ENCH_Unburden", "Unburden", "mgef_unburden", False, "list_cuirass"),
        ("ench_reinforcement", "AOS_ENCH_Reinforcement", "Reinforcement", "mgef_reinforcement", False, "list_cuirass"),
    ]
    result = []
    for ench_key, editor_id, name, mgef_key, weapon, list_key in specs:
        local_id = EXCLUSIVE_FORMS[ench_key]
        worn = EXCLUSIVE_FORMS[list_key] if list_key else None
        result.append((
            "ENCH",
            editor_id,
            local_id,
            exclusive_ench_record(
                local_id,
                editor_id,
                name,
                EXCLUSIVE_FORMS[mgef_key],
                weapon,
                worn,
            ),
        ))
    return result


def perk_entry_blocks(source_form_id: int) -> list[list[tuple[str, bytes]]]:
    result: list[list[tuple[str, bytes]]] = []
    current: list[tuple[str, bytes]] | None = None
    for signature, data in snapshot_fields(source_form_id):
        if signature == "PRKE":
            if current is not None:
                raise ValueError(f"Nested PERK entry in {source_form_id:08X}")
            current = [(signature, data)]
            continue
        if current is None:
            continue
        current.append((signature, data))
        if signature == "PRKF":
            result.append(current)
            current = None
    if current is not None or not result:
        raise ValueError(f"Malformed or empty PERK entries in {source_form_id:08X}")
    return result


def select_perk_entry(source_form_id: int, entry_point: int) -> list[tuple[str, bytes]]:
    matches = []
    for block in perk_entry_blocks(source_form_id):
        data = next(value for signature, value in block if signature == "DATA")
        if data and data[0] == entry_point:
            matches.append(block)
    if len(matches) != 1:
        raise ValueError(
            f"Expected one entry point {entry_point} in {source_form_id:08X}, "
            f"found {len(matches)}"
        )
    return matches[0]


def rewrite_perk_entry(
    block: list[tuple[str, bytes]],
    multiplier: float,
    *,
    function_type: int | None = None,
    conditions: list[tuple[int, int, int]] | None = None,
) -> list[tuple[str, bytes]]:
    result = list(block)
    for index, (signature, data) in enumerate(result):
        if signature == "DATA":
            entry_data = bytearray(data)
            if len(entry_data) != 3:
                raise ValueError("Unexpected PERK entry DATA size")
            if function_type is not None:
                entry_data[1] = function_type
            result[index] = (signature, bytes(entry_data))
            break
    else:
        raise ValueError("PERK entry has no DATA")

    if conditions is not None:
        result = [
            (signature, data)
            for signature, data in result
            if signature not in {"PRKC", "CTDA"}
        ]
        insert_at = next(
            index + 1
            for index, (signature, _data) in enumerate(result)
            if signature == "DATA"
        )
        condition_fields: list[tuple[str, bytes]] = []
        active_group = None
        for parameter_group, function_index, parameter in conditions:
            if parameter_group != active_group:
                condition_fields.append(("PRKC", bytes((parameter_group,))))
                active_group = parameter_group
            condition_fields.append(("CTDA", ctda(function_index, parameter)))
        result[insert_at:insert_at] = condition_fields

    for index, (signature, _data) in enumerate(result):
        if signature == "EPFD":
            result[index] = (signature, f32(multiplier))
            break
    else:
        raise ValueError("PERK entry has no EPFD")
    return result


def perk_record_from_entries(
    local_id: int,
    editor_id: str,
    display_name: str,
    description: str,
    entries: list[list[tuple[str, bytes]]],
) -> bytes:
    body = (
        field("EDID", zstring(editor_id))
        + field("FULL", zstring(display_name))
        + field("DESC", zstring(description))
        + field("DATA", bytes.fromhex("0000010100"))
    )
    for block in entries:
        body += assemble_fields(block)
    return record("PERK", local_to_form(local_id), body)


def exclusive_perk_records() -> list[tuple[str, str, int, bytes]]:
    def unburden_entries(stamina_multiplier: float) -> list[list[tuple[str, bytes]]]:
        return (
            perk_entry_blocks(0x00058F6D)
            + perk_entry_blocks(0x00051B1C)
            + [
                rewrite_perk_entry(
                    select_perk_entry(0x00052D51, 27),
                    stamina_multiplier,
                    conditions=[],
                )
            ]
        )

    def elemental_entries(multiplier: float) -> list[list[tuple[str, bytes]]]:
        return [
            rewrite_perk_entry(select_perk_entry(source, 29), multiplier)
            for source in (0x0010FCF8, 0x0010FCF9, 0x0010FCFA)
        ]

    specs = [
        (
            "perk_unburden_expert",
            "AOS_PERK_UnburdenExpert",
            "Unburden (Expert)",
            "Worn armor weighs nothing and does not slow you down. Power attacks cost 15% less Stamina.",
            unburden_entries(reduction_multiplier("unburden", "Expert")),
        ),
        (
            "perk_unburden_master",
            "AOS_PERK_UnburdenMaster",
            "Unburden (Master)",
            "Worn armor weighs nothing and does not slow you down. Power attacks cost 30% less Stamina.",
            unburden_entries(reduction_multiplier("unburden", "Master")),
        ),
        (
            "perk_reinforcement_expert",
            "AOS_PERK_ReinforcementExpert",
            "Reinforcement (Expert)",
            "You stagger 20% less.",
            [
                rewrite_perk_entry(
                    select_perk_entry(0x000E8279, 33),
                    reduction_multiplier("reinforcement", "Expert"),
                )
            ],
        ),
        (
            "perk_reinforcement_master",
            "AOS_PERK_ReinforcementMaster",
            "Reinforcement (Master)",
            "You stagger 40% less.",
            [
                rewrite_perk_entry(
                    select_perk_entry(0x000E8279, 33),
                    reduction_multiplier("reinforcement", "Master"),
                )
            ],
        ),
        (
            "perk_elemental_expert",
            "AOS_PERK_ElementalBurstExpert",
            "Elemental Burst (25%)",
            "Fire, Frost, and Shock spells are 25% stronger.",
            elemental_entries(bonus_multiplier("elemental_burst", "Expert")),
        ),
        (
            "perk_elemental_master",
            "AOS_PERK_ElementalBurstMaster",
            "Elemental Burst (50%)",
            "Fire, Frost, and Shock spells are 50% stronger.",
            elemental_entries(bonus_multiplier("elemental_burst", "Master")),
        ),
        (
            "perk_elemental_combined",
            "AOS_PERK_ElementalBurstCombined",
            "Elemental Burst (75%)",
            "Fire, Frost, and Shock spells are 75% stronger.",
            elemental_entries(
                1.0
                + (
                    EXCLUSIVE_MAGNITUDES["elemental_burst"]["Expert"]
                    + EXCLUSIVE_MAGNITUDES["elemental_burst"]["Master"]
                )
                / 100.0
            ),
        ),
        (
            "perk_elemental_maximum",
            "AOS_PERK_ElementalBurstMaximum",
            "Elemental Burst (100%)",
            "Fire, Frost, and Shock spells are 100% stronger.",
            elemental_entries(
                1.0
                + 2.0
                * EXCLUSIVE_MAGNITUDES["elemental_burst"]["Master"]
                / 100.0
            ),
        ),
    ]
    return [
        (
            "PERK",
            editor_id,
            EXCLUSIVE_FORMS[key],
            perk_record_from_entries(
                EXCLUSIVE_FORMS[key],
                editor_id,
                display_name,
                description,
                entries,
            ),
        )
        for key, editor_id, display_name, description, entries in specs
    ]


def skyforge_equipment_records() -> list[tuple[str, str, int, bytes]]:
    specs = [
        (0x0009F25D, "WEAP", "skyforge_dagger", "AOS_WeapSkyforgeDagger", "Skyforge Steel Dagger", 290, 2.5, 10),
        (0x0009F25C, "WEAP", "skyforge_sword", "AOS_WeapSkyforgeSword", "Skyforge Steel Sword", 720, 10.0, 13),
        (0x0009F260, "WEAP", "skyforge_war_axe", "AOS_WeapSkyforgeWarAxe", "Skyforge Steel War Axe", 865, 12.0, 15),
        (0x00013988, "WEAP", "skyforge_mace", "AOS_WeapSkyforgeMace", "Skyforge Steel Mace", 1000, 14.0, 16),
        (0x0009F25E, "WEAP", "skyforge_greatsword", "AOS_WeapSkyforgeGreatsword", "Skyforge Steel Greatsword", 1440, 17.0, 22),
        (0x0009F25F, "WEAP", "skyforge_battleaxe", "AOS_WeapSkyforgeBattleaxe", "Skyforge Steel Battleaxe", 1585, 21.0, 23),
        (0x0001398A, "WEAP", "skyforge_warhammer", "AOS_WeapSkyforgeWarhammer", "Skyforge Steel Warhammer", 1725, 25.0, 25),
        (0x0001395B, "ARMO", "skyforge_plate_boots", "AOS_ArmorSkyforgePlateBoots", "Skyforge Steel Plate Boots", 275, 9.0, 16),
        (0x0001395C, "ARMO", "skyforge_plate_cuirass", "AOS_ArmorSkyforgePlateCuirass", "Skyforge Steel Plate Armor", 1500, 38.0, 43),
        (0x0001395D, "ARMO", "skyforge_plate_gauntlets", "AOS_ArmorSkyforgePlateGauntlets", "Skyforge Steel Plate Gauntlets", 275, 6.0, 16),
        (0x0001395E, "ARMO", "skyforge_plate_helmet", "AOS_ArmorSkyforgePlateHelmet", "Skyforge Steel Plate Helmet", 750, 6.0, 21),
        (0x00013955, "ARMO", "skyforge_shield", "AOS_ArmorSkyforgeSteelShield", "Skyforge Steel Shield", 750, 12.0, 32),
    ]
    result = []
    for source, signature, key, editor_id, name, value, weight, rating in specs:
        entries = snapshot_fields(source)
        entries = replace_first(entries, "EDID", zstring(editor_id))
        entries = replace_first(entries, "FULL", zstring(name))
        data = bytearray(next(raw for candidate, raw in entries if candidate == "DATA"))
        struct.pack_into("<I", data, 0, value)
        struct.pack_into("<f", data, 4, weight)
        entries = replace_first(entries, "DATA", bytes(data))
        if signature == "WEAP":
            struct.pack_into("<H", data, 8, rating)
            entries = replace_first(entries, "DATA", bytes(data))
        else:
            entries = replace_first(entries, "DNAM", u32(rating * 100))
        local_id = EXCLUSIVE_FORMS[key]
        result.append((
            signature,
            editor_id,
            local_id,
            record(
                signature,
                local_to_form(local_id),
                assemble_fields(entries),
                flags=snapshot_flags(source),
            ),
        ))
    return result


def skyforge_temper_records() -> list[tuple[str, str, int, bytes]]:
    specs = [
        (0x000F46AB, "temper_skyforge_dagger", "AOS_TemperSkyforgeDagger", "skyforge_dagger"),
        (0x000F46AE, "temper_skyforge_sword", "AOS_TemperSkyforgeSword", "skyforge_sword"),
        (0x000F46AF, "temper_skyforge_war_axe", "AOS_TemperSkyforgeWarAxe", "skyforge_war_axe"),
        (0x000F46AD, "temper_skyforge_mace", "AOS_TemperSkyforgeMace", "skyforge_mace"),
        (0x000F46AC, "temper_skyforge_greatsword", "AOS_TemperSkyforgeGreatsword", "skyforge_greatsword"),
        (0x000F46AA, "temper_skyforge_battleaxe", "AOS_TemperSkyforgeBattleaxe", "skyforge_battleaxe"),
        (0x000F46B0, "temper_skyforge_warhammer", "AOS_TemperSkyforgeWarhammer", "skyforge_warhammer"),
        (0x000EB856, "temper_skyforge_plate_boots", "AOS_TemperSkyforgePlateBoots", "skyforge_plate_boots"),
        (0x000EB857, "temper_skyforge_plate_cuirass", "AOS_TemperSkyforgePlateCuirass", "skyforge_plate_cuirass"),
        (0x000EB858, "temper_skyforge_plate_gauntlets", "AOS_TemperSkyforgePlateGauntlets", "skyforge_plate_gauntlets"),
        (0x000EB859, "temper_skyforge_plate_helmet", "AOS_TemperSkyforgePlateHelmet", "skyforge_plate_helmet"),
        (0x000EB85B, "temper_skyforge_shield", "AOS_TemperSkyforgeSteelShield", "skyforge_shield"),
    ]
    result = []
    for source, recipe_key, editor_id, output_key in specs:
        local_id = EXCLUSIVE_FORMS[recipe_key]
        entries = snapshot_fields(source)
        entries = replace_first(entries, "EDID", zstring(editor_id))
        entries = replace_first(entries, "CNAM", form_bytes(local_to_form(EXCLUSIVE_FORMS[output_key])))
        result.append(("COBJ", editor_id, local_id, record("COBJ", local_to_form(local_id), assemble_fields(entries))))
    return result


GET_IN_FACTION = 0x47
GET_IS_ID = 0x48
GET_GLOBAL_VALUE = 0x4A
GET_IS_SEX = 0x46
GET_IS_VOICE_TYPE = 0x1AA
GET_QUEST_COMPLETED = 0x21F
RUN_ON_SUBJECT = 0
RUN_ON_REFERENCE = 2


def read_npc_dialogue_conditions() -> dict[str, list[NpcDialogueCondition]]:
    result = {"Smithing": [], "Enchanting": [], "Alchemy": []}
    seen: set[tuple[str, str]] = set()
    with NPC_TIERS_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        for row_number, row in enumerate(csv.DictReader(handle), start=2):
            service = row["Service"].strip()
            if service not in result:
                raise ValueError(f"Unknown service '{service}' in {NPC_TIERS_PATH}:{row_number}")
            plugin = row["Plugin"].strip()
            form_text = row["FormID"].strip()
            if not plugin or not re.fullmatch(r"[0-9A-Fa-f]{6,8}", form_text):
                raise ValueError(
                    f"Invalid Plugin/FormID for {row['DisplayName']} in {NPC_TIERS_PATH}:{row_number}"
                )
            condition = NpcDialogueCondition(
                row["DisplayName"].strip(), service, plugin, int(form_text, 16)
            )
            key = (service, condition.primary_key.lower())
            if key in seen:
                raise ValueError(f"Duplicate NPC dialogue condition: {service} {condition.primary_key}")
            seen.add(key)
            result[service].append(condition)
    validate_npc_sources(result)
    return result


def validate_npc_sources(conditions: dict[str, list[NpcDialogueCondition]]) -> None:
    with CONFIG_PATH.open("rb") as handle:
        config = tomllib.load(handle)

    configured: dict[str, set[str]] = {
        "Smithing": {
            entry["form"].lower() for entry in config.get("npcs", {}).get("overrides", [])
        },
        "Enchanting": {
            config["forms"][key].lower()
            for key in ("farengar", "calcelmo", "sergius", "sybille", "neloth", "feranSadri")
        },
        "Alchemy": {
            entry["form"].lower() for entry in config.get("alchemy", {}).get("npcs", [])
        },
    }
    csv_keys = {
        service: {entry.primary_key.lower() for entry in entries}
        for service, entries in conditions.items()
    }
    errors: list[str] = []
    for service in configured:
        missing = sorted(configured[service] - csv_keys[service])
        extra = sorted(csv_keys[service] - configured[service])
        if missing:
            errors.append(f"{service} configured without ESP dialogue condition: {', '.join(missing)}")
        if extra:
            errors.append(f"{service} CSV NPC missing from runtime config: {', '.join(extra)}")
    if errors:
        raise ValueError("NPC dialogue source validation failed:\n - " + "\n - ".join(errors))


def plugin_form_id(plugin: str, local_id: int) -> int:
    master_indices = {
        "Skyrim.esm": SKYRIM,
        "Dawnguard.esm": DAWNGUARD,
        "Dragonborn.esm": DRAGONBORN,
    }
    try:
        return master_ref(master_indices[plugin], local_id)
    except KeyError as exc:
        raise ValueError(f"Unsupported master '{plugin}'") from exc


def parse_source_form_id(value: str, label: str, row_number: int) -> int:
    value = value.strip()
    if not re.fullmatch(r"[0-9A-Fa-f]{6,8}", value):
        raise ValueError(
            f"Invalid {label} '{value}' in {VOICE_RESPONSES_PATH}:{row_number}"
        )
    return int(value, 16) & 0xFFFFFF


def read_dialogue_responses(
    npc_conditions: dict[str, list[NpcDialogueCondition]],
) -> dict[str, list[DialogueResponse]]:
    expected_services = {
        "Smithing Craft": 26,
        "Smithing Improve": 27,
        "Enchanting": 6,
        "Alchemy": 20,
    }
    result: dict[str, list[DialogueResponse]] = {
        service: [] for service in expected_services
    }
    seen_info_ids: set[int] = set()

    with VOICE_RESPONSES_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        for row_number, row in enumerate(csv.DictReader(handle), start=2):
            service = row["Service"].strip()
            if service not in result:
                raise ValueError(
                    f"Unknown service '{service}' in {VOICE_RESPONSES_PATH}:{row_number}"
                )
            info_local_id = parse_source_form_id(
                row["InfoLocalID"], "InfoLocalID", row_number
            )
            if info_local_id in seen_info_ids:
                raise ValueError(
                    f"Duplicate INFO local FormID {info_local_id:06X} in "
                    f"{VOICE_RESPONSES_PATH}:{row_number}"
                )
            seen_info_ids.add(info_local_id)

            selector_type = row["SelectorType"].strip()
            selector_plugin = row["SelectorPlugin"].strip()
            selector_local_id = parse_source_form_id(
                row["SelectorFormID"], "SelectorFormID", row_number
            )
            selector_form_id = plugin_form_id(selector_plugin, selector_local_id)
            if selector_type == "Actor":
                conditions = ((GET_IS_ID, selector_form_id, RUN_ON_SUBJECT, 0),)
            elif selector_type == "FactionVoice":
                voice_plugin = row["VoicePlugin"].strip()
                voice_local_id = parse_source_form_id(
                    row["VoiceFormID"], "VoiceFormID", row_number
                )
                conditions = (
                    (GET_IN_FACTION, selector_form_id, RUN_ON_SUBJECT, 0),
                    (
                        GET_IS_VOICE_TYPE,
                        plugin_form_id(voice_plugin, voice_local_id),
                        RUN_ON_SUBJECT,
                        0,
                    ),
                )
            else:
                raise ValueError(
                    f"Unknown SelectorType '{selector_type}' in "
                    f"{VOICE_RESPONSES_PATH}:{row_number}"
                )

            player_sex = (row.get("PlayerSex") or "").strip()
            if player_sex:
                if player_sex not in {"Male", "Female"}:
                    raise ValueError(
                        f"Unknown PlayerSex '{player_sex}' in "
                        f"{VOICE_RESPONSES_PATH}:{row_number}"
                    )
                conditions += ((
                    GET_IS_SEX,
                    0 if player_sex == "Male" else 1,
                    RUN_ON_REFERENCE,
                    plugin_form_id("Skyrim.esm", 0x000014),
                ),)

            response_plugin = row["ResponsePlugin"].strip()
            response_local_id = parse_source_form_id(
                row["ResponseInfoFormID"], "ResponseInfoFormID", row_number
            )
            result[service].append(
                DialogueResponse(
                    info_local_id=info_local_id,
                    selector_type=selector_type,
                    selector_name=row["SelectorName"].strip(),
                    shared_info_form_id=plugin_form_id(
                        response_plugin, response_local_id
                    ),
                    response_plugin=response_plugin,
                    response_local_id=response_local_id,
                    response_text=row["ResponseText"].strip(),
                    voice_type=row["VoiceType"].strip(),
                    conditions=conditions,
                )
            )

    count_errors = [
        f"{service}: expected {expected}, found {len(result[service])}"
        for service, expected in expected_services.items()
        if len(result[service]) != expected
    ]
    if count_errors:
        raise ValueError(
            "Dialogue response count validation failed:\n - "
            + "\n - ".join(count_errors)
        )

    expected_actor_form_ids = {
        "Smithing Craft": {
            entry.form_id for entry in npc_conditions["Smithing"]
        },
        "Smithing Improve": {
            entry.form_id for entry in npc_conditions["Smithing"]
        },
        "Enchanting": {
            entry.form_id for entry in npc_conditions["Enchanting"]
        },
        "Alchemy": {
            entry.form_id for entry in npc_conditions["Alchemy"]
        },
    }
    for service, expected_form_ids in expected_actor_form_ids.items():
        actual_form_ids = {
            response.conditions[0][1]
            for response in result[service]
            if response.selector_type == "Actor"
        }
        if actual_form_ids != expected_form_ids:
            missing = [
                f"{value:08X}" for value in sorted(expected_form_ids - actual_form_ids)
            ]
            extra = [
                f"{value:08X}" for value in sorted(actual_form_ids - expected_form_ids)
            ]
            raise ValueError(
                f"Actor response coverage mismatch for {service}; "
                f"missing={missing}, extra={extra}"
            )

    feran = [
        response
        for response in result["Enchanting"]
        if response.selector_type == "Actor"
        and response.conditions[0][1] == plugin_form_id("Dawnguard.esm", 0x003366)
    ]
    if len(feran) != 1 or feran[0].response_text != "Take a look.":
        raise ValueError(
            "Feran Sadri must have exactly one neutral Enchanting response: "
            "'Take a look.'"
        )
    if "alchem" in feran[0].response_text.lower() or "potion" in feran[0].response_text.lower():
        raise ValueError("Feran Sadri response must not reference alchemy or potions")

    gunmar_form = plugin_form_id("Dawnguard.esm", 0x00336C)
    gunmar_improve = [
        response
        for response in result["Smithing Improve"]
        if response.selector_type == "Actor"
        and response.conditions[0][1] == gunmar_form
    ]
    if {response.response_text for response in gunmar_improve} != {
        "Aye lad.",
        "Aye lass.",
    }:
        raise ValueError(
            "Gunmar Improve responses must be the player-sex-specific "
            "'Aye lad.' and 'Aye lass.' lines"
        )
    for response in result["Smithing Improve"]:
        if response in gunmar_improve:
            continue
        if (
            response.response_plugin != "Skyrim.esm"
            or response.response_local_id != 0x0DBA22
            or response.response_text != "Of course."
        ):
            raise ValueError(
                "All generic Smithing Improve responses must reuse "
                "Skyrim.esm|000DBA22: 'Of course.'"
            )

    return result


def with_fallback_responses(
    dialogue_responses: dict[str, list[DialogueResponse]],
) -> dict[str, list[DialogueResponse]]:
    """Append one voice-independent response to each service topic."""
    result = {service: list(responses) for service, responses in dialogue_responses.items()}
    for service, local_id in FALLBACK_INFO_LOCAL_IDS.items():
        plugin, response_local_id = FALLBACK_SHARED_RESPONSES[service]
        shared = plugin_form_id(plugin, response_local_id)
        text = next(
            (entry.response_text for entry in result[service]
             if entry.shared_info_form_id == shared),
            "",
        )
        conditions: tuple[tuple, ...] = ()
        if service == "Enchanting":
            # Neloth has his own topic; keep him out of the general one.
            conditions = ((
                GET_IS_ID,
                plugin_form_id(*NELOTH_ACTOR),
                RUN_ON_SUBJECT,
                0,
                0.0,
            ),)
        result[service].append(DialogueResponse(
            info_local_id=local_id,
            selector_type="Fallback",
            selector_name="Any supported artisan",
            shared_info_form_id=shared,
            response_plugin=plugin,
            response_local_id=response_local_id,
            response_text=text,
            voice_type="",
            conditions=conditions,
        ))
    return result


def dialogue_topics(
    dialogue_responses: dict[str, list[DialogueResponse]],
) -> list[DialogueTopic]:
    dialogue_responses = with_fallback_responses(dialogue_responses)
    def require_smith_services(
        responses: list[DialogueResponse],
    ) -> list[DialogueResponse]:
        return [
            DialogueResponse(
                info_local_id=response.info_local_id,
                selector_type=response.selector_type,
                selector_name=response.selector_name,
                shared_info_form_id=response.shared_info_form_id,
                response_plugin=response.response_plugin,
                response_local_id=response.response_local_id,
                response_text=response.response_text,
                voice_type=response.voice_type,
                conditions=response.conditions
                + ((
                    GET_GLOBAL_VALUE,
                    local_to_form(SMITH_SERVICES_GLOBAL_LOCAL_ID),
                    RUN_ON_SUBJECT,
                    0,
                ),),
            )
            for response in responses
        ]

    craft_responses = require_smith_services(
        dialogue_responses["Smithing Craft"]
    )
    improve_responses = require_smith_services(
        dialogue_responses["Smithing Improve"]
    )
    collection_responses = [
        DialogueResponse(
            info_local_id=COLLECTION_INFO_FIRST_LOCAL_ID + index,
            selector_type=response.selector_type,
            selector_name=response.selector_name,
            shared_info_form_id=response.shared_info_form_id,
            response_plugin=response.response_plugin,
            response_local_id=response.response_local_id,
            response_text=response.response_text,
            voice_type=response.voice_type,
            conditions=response.conditions
            + ((
                GET_GLOBAL_VALUE,
                local_to_form(ORDER_AVAILABILITY_GLOBAL_LOCAL_ID),
                RUN_ON_SUBJECT,
                0,
            ),),
        )
        for index, response in enumerate(
            improve_responses
        )
    ]
    enchanting_responses = dialogue_responses["Enchanting"]
    neloth_responses = [
        response for response in enchanting_responses
        if response.selector_name == "Neloth"
    ]
    if len(neloth_responses) != 1:
        raise ValueError("Expected exactly one Neloth enchanting response")
    topics = [
        DialogueTopic(
            "Smithing Craft",
            0x0801,
            0x0805,
            "AOS_SmithCraftBranch",
            "AOS_SmithCraftTopic",
            "Could you craft something for me?",
            "AOS_TIF_SmithCraft",
            craft_responses,
        ),
        DialogueTopic(
            "Smithing Improve",
            0x0802,
            0x0806,
            "AOS_SmithImproveBranch",
            "AOS_SmithImproveTopic",
            "Could you improve my equipment?",
            "AOS_TIF_SmithImprove",
            improve_responses,
        ),
        DialogueTopic(
            "Enchanting",
            0x0803,
            0x0807,
            "AOS_EnchantBranch",
            "AOS_EnchantTopic",
            "Could you enchant something for me?",
            "AOS_TIF_Enchant",
            [
                response for response in enchanting_responses
                if response.selector_name != "Neloth"
            ],
        ),
        DialogueTopic(
            "Enchanting Neloth",
            0x08F4,
            0x08F5,
            "AOS_EnchantNelothBranch",
            "AOS_EnchantNelothTopic",
            "Could you enchant something for me?",
            "AOS_TIF_Enchant",
            neloth_responses,
            master_ref(DRAGONBORN, 0x017E85),
        ),
        DialogueTopic(
            "Alchemy",
            0x0804,
            0x0808,
            "AOS_AlchemyBranch",
            "AOS_AlchemyTopic",
            "Could you brew a batch for me?",
            "AOS_TIF_Alchemy",
            dialogue_responses["Alchemy"],
        ),
        DialogueTopic(
            "Smithing Orders",
            0x0850,
            0x0851,
            "AOS_CollectOrdersBranch",
            "AOS_CollectOrdersTopic",
            "I'd like to collect my orders.",
            "AOS_TIF_CollectOrders",
            collection_responses,
        ),
    ]
    for topic in topics:
        if topic.service.startswith("Smithing"):
            service = "Smithing"
        elif topic.service.startswith("Enchanting"):
            service = "Enchanting"
        elif topic.service == "Alchemy":
            service = "Alchemy"
        else:
            continue
        global_id = SERVICE_ACCESS_GLOBALS[service][0]
        topic.responses[:] = [replace(response, access_global=global_id)
                              for response in topic.responses]
    return topics


def quest_record() -> bytes:
    body = (
        field("EDID", zstring("AOS_DialogueQuest"))
        + field("FULL", zstring("Artisans of Skyrim Services"))
        + field("DNAM", bytes.fromhex("11003c000000000000000000"))
        + field("NEXT", b"")
        + field("ANAM", u32(0))
    )
    return record("QUST", local_to_form(0x0800), body)


def vmad_script(script_name: str) -> bytes:
    name = zstringless(script_name)
    # Script name, status flags 0 (local), no properties.
    return u16(len(name)) + name + bytes([0]) + u16(0)


def vmad_mcm_quest(quest_form_id: int) -> bytes:
    # VMAD version 5, object format 2. One quest script, then the quest
    # fragment block (no fragments, empty file name) and one alias script.
    return (
        u16(5)
        + u16(2)
        + u16(1)
        + vmad_script("AOS_MCM")
        + bytes([2])
        + u16(0)
        + u16(0)
        + u16(1)
        # Alias object, format 2: unused u16, alias id s16, quest FormID.
        + u16(0)
        + u16(0)
        + u32(quest_form_id)
        + u16(5)
        + u16(2)
        + u16(1)
        + vmad_script("SKI_PlayerLoadGameAlias")
    )


def mcm_quest_record() -> bytes:
    # Start Game Enabled quest for MCM Helper. MCM_ConfigBase needs a player
    # alias with SKI_PlayerLoadGameAlias so the menu re-registers after loads.
    form_id = local_to_form(MCM_QUEST_LOCAL_ID)
    body = (
        field("EDID", zstring("AOS_MCMQuest"))
        + field("VMAD", vmad_mcm_quest(form_id))
        + field("FULL", zstring("Artisans of Skyrim MCM"))
        + field("DNAM", bytes.fromhex("11003c000000000000000000"))
        + field("NEXT", b"")
        + field("ANAM", u32(1))
        + field("ALST", u32(0))
        + field("ALID", zstring("PlayerAlias"))
        + field("FNAM", u32(0))
        + field("ALFR", form_bytes(PLAYER_REF))
        + field("ALED", b"")
    )
    return record("QUST", form_id, body)


def order_availability_global_record() -> bytes:
    body = (
        field("EDID", zstring("AOS_HasOrdersForCurrentSpeaker"))
        + field("FNAM", b"f")
        + field("FLTV", f32(0.0))
    )
    return record(
        "GLOB",
        local_to_form(ORDER_AVAILABILITY_GLOBAL_LOCAL_ID),
        body,
    )


def smith_services_global_record() -> bytes:
    body = (
        field("EDID", zstring("AOS_SmithServicesForCurrentSpeaker"))
        + field("FNAM", b"f")
        + field("FLTV", f32(0.0))
    )
    return record(
        "GLOB",
        local_to_form(SMITH_SERVICES_GLOBAL_LOCAL_ID),
        body,
    )


def branch_record(topic: DialogueTopic) -> bytes:
    quest_form_id = topic.quest_form_id or local_to_form(0x0800)
    body = (
        field("EDID", zstring(topic.branch_edid))
        + field("QNAM", form_bytes(quest_form_id))
        + field("TNAM", u32(0))
        + field("DNAM", u32(1))
        + field("SNAM", form_bytes(local_to_form(topic.dial_local)))
    )
    return record("DLBR", local_to_form(topic.branch_local), body)


def dial_record(topic: DialogueTopic) -> bytes:
    quest_form_id = topic.quest_form_id or local_to_form(0x0800)
    body = (
        field("EDID", zstring(topic.topic_edid))
        + field("FULL", zstring(topic.topic_text))
        + field("PNAM", bytes.fromhex("00007042"))
        + field("BNAM", form_bytes(local_to_form(topic.branch_local)))
        + field("QNAM", form_bytes(quest_form_id))
        + field("DATA", u32(0))
        + field("SNAM", b"CUST")
        + field("TIFC", u32(len(topic.responses)))
    )
    return record("DIAL", local_to_form(topic.dial_local), body)


def info_record(form_id: int, topic: DialogueTopic, response: DialogueResponse) -> bytes:
    body = (
        field("VMAD", vmad_fragment(topic.fragment_script))
        + field("ENAM", u32(2))
        + field("CNAM", b"\0")
        + field("DNAM", form_bytes(response.shared_info_form_id))
    )
    for condition in response.conditions:
        function_index, parameter_1, run_on, reference = condition[:4]
        comparison = condition[4] if len(condition) > 4 else 1.0
        body += field("CTDA", ctda(function_index, parameter_1, run_on, reference,
                                   comparison=comparison))
    if response.access_global:
        body += field("CTDA", ctda(GET_GLOBAL_VALUE, local_to_form(response.access_global)))
    return record("INFO", form_id, body)


def parse_float(value: str) -> float:
    value = value.strip()
    return float(value) if value else 0.0


def parse_int(value: str) -> int:
    value = value.strip()
    return int(float(value)) if value else 0


MAGNITUDELESS_EFFECTS = frozenset({"Paralysis", "Waterbreathing", "Invisibility"})


def validate_effect_magnitude(effect_name: str, magnitude: float) -> None:
    # A missing magnitude used to silently create level-zero Fear/Frenzy and
    # zero-strength Slow. Only reviewed vanilla No Magnitude effects may use zero.
    if not math.isfinite(magnitude) or magnitude < 0 or (
        magnitude == 0 and effect_name not in MAGNITUDELESS_EFFECTS
    ):
        raise ValueError(f"{effect_name}: an explicit positive magnitude is required")


def read_alchemy_records() -> list[AlchemyRecord]:
    rows: list[AlchemyRecord] = []
    with CSV_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        for index, row in enumerate(reader):
            effects: list[tuple[str, float, int]] = []
            for suffix in ("1", "2"):
                effect_name = row[f"Effect{suffix}"].strip()
                if not effect_name:
                    continue
                if effect_name not in MGEF:
                    raise ValueError(f"No MGEF mapping for effect '{effect_name}'")
                magnitude = parse_float(row[f"Magnitude{suffix}"])
                validate_effect_magnitude(effect_name, magnitude)
                effects.append(
                    (
                        effect_name,
                        magnitude,
                        parse_int(row[f"Duration{suffix}"]),
                    )
                )
            if not effects:
                raise ValueError(f"{row['EditorID']} has no effects")
            rows.append(
                AlchemyRecord(
                    local_id=0x0A00 + index,
                    editor_id=row["EditorID"].strip(),
                    display_name=row["DisplayName"].strip(),
                    item_type=row["Type"].strip(),
                    tier=row["Tier"].strip(),
                    group=row["Group"].strip(),
                    batch_size=int(row["BatchSize"]),
                    batch_price=int(row["BatchPrice"]),
                    alch_value=int(row["ALCHValuePerFlask"]),
                    weight=float(row["Weight"] or "0.5"),
                    effects=effects,
                    source_id=row["SourceID"].strip(),
                )
            )
    tier_prefix = re.compile(r"^(?:Adept|Expert|Master)\s+", re.IGNORECASE)
    for item in rows:
        if tier_prefix.match(item.display_name):
            raise ValueError(
                f"Public alchemy name exposes internal tier: {item.display_name!r}"
            )
        expected_prefix = f"{item.item_type} of "
        if not item.display_name.startswith(expected_prefix):
            raise ValueError(
                f"{item.source_id} must use a public name beginning with "
                f"{expected_prefix!r}, found {item.display_name!r}"
            )
        expected = DUAL_PUBLIC_NAMES.get(item.source_id)
        if expected and item.display_name != expected:
            raise ValueError(
                f"{item.source_id} must use exact public name {expected!r}, "
                f"found {item.display_name!r}"
            )
    missing_duals = sorted(set(DUAL_PUBLIC_NAMES) - {item.source_id for item in rows})
    if missing_duals:
        raise ValueError(f"Missing dual-effect alchemy records: {missing_duals}")
    return rows


def alch_record(item: AlchemyRecord) -> bytes:
    is_poison = item.item_type == "Poison"
    enit_flags = ALCH_FLAG_NO_AUTO_CALC
    if is_poison:
        enit_flags |= ALCH_FLAG_POISON
    model = POISON_MODEL if is_poison else POTION_MODEL
    if item.effects[0][0] in {"Invisibility", "Waterbreathing"}:
        model = INVISIBILITY_MODEL
    body = (
        field("EDID", zstring(item.editor_id))
        + field("OBND", POISON_OBND if is_poison else POTION_OBND)
        + field("FULL", zstring(item.display_name))
        + field("KSIZ", u32(1))
        + field("KWDA", POISON_KWDA if is_poison else POTION_KWDA)
        + field("MODL", zstring(model))
        + field("MODT", POISON_MODT if is_poison else POTION_MODT)
        + field("YNAM", PICKUP_SOUND)
        + field("ZNAM", PUTDOWN_SOUND)
        + field("DATA", f32(item.weight))
        + field(
            "ENIT",
            u32(item.alch_value)
            + u32(enit_flags)
            + (POISON_ENIT_TAIL if is_poison else POTION_ENIT_TAIL),
        )
    )
    for effect_name, magnitude, duration in item.effects:
        body += field("EFID", form_bytes(MGEF[effect_name]))
        body += field("EFIT", f32(magnitude) + u32(0) + u32(duration))
    return record("ALCH", item.form_id, body)


def courier_book_record(index: int) -> bytes:
    local_id = COURIER_BOOK_FIRST_LOCAL_ID + index
    body = (
        field("EDID", zstring(f"AOS_CourierNotice{index + 1:03d}"))
        + field("OBND", bytes(12))
        + field("FULL", zstring("Smithing Order Notice"))
        + field("MODL", zstring("Clutter\\Books\\Note03.nif"))
        + field("YNAM", form_bytes(master_ref(SKYRIM, 0x0C7A54)))
        + field("KSIZ", u32(1))
        + field("KWDA", form_bytes(master_ref(SKYRIM, 0x0937A2)))
        + field(
            "DESC",
            zstring(
                '<font face="$HandwrittenFont">\n\n'
                "Your smithing orders are ready for collection.\n"
                "</font>"
            ),
        )
        + field("DATA", bytes.fromhex("00000000ffffffff0000000000000000"))
        + field("INAM", form_bytes(master_ref(SKYRIM, 0x01541C)))
        + field("CNAM", u32(0))
    )
    return record("BOOK", local_to_form(local_id), body)


def tes4_record(total_records_and_groups: int, next_object_id: int) -> bytes:
    body = (
        field("HEDR", f32(1.70) + u32(total_records_and_groups) + u32(next_object_id))
        + field("CNAM", zstring("Artisans of Skyrim"))
        + field("SNAM", zstring(f"Artisans of Skyrim {project_version.TEXT} services, custom effects, and Skyforge equipment."))
    )
    for master in ("Skyrim.esm", "Dawnguard.esm", "Dragonborn.esm"):
        body += field("MAST", zstring(master)) + field("DATA", bytes(8))
    return record("TES4", 0, body, flags=0x200)


def build_esp(
    alchemy_items: list[AlchemyRecord],
    dialogue_responses: dict[str, list[DialogueResponse]],
) -> bytes:
    topics = dialogue_topics(dialogue_responses)
    dial_contents = b""
    manifest: list[tuple[str, str, int, str]] = []

    for topic in topics:
        dial_contents += dial_record(topic)
        child_contents = b""
        for response in topic.responses:
            form_id = local_to_form(response.info_local_id)
            child_contents += info_record(form_id, topic, response)
            manifest.append(
                (
                    "INFO",
                    f"{topic.topic_edid}_Response_{response.info_local_id:06X}",
                    form_id,
                    f"{topic.service}; {response.selector_name}; shared "
                    f"{response.response_plugin}|{response.response_local_id:06X}",
                )
            )
        dial_contents += group(form_bytes(local_to_form(topic.dial_local)), 7, child_contents)
        manifest.append(("DIAL", topic.topic_edid, local_to_form(topic.dial_local), topic.service))

    quest_contents = quest_record() + mcm_quest_record()
    branch_contents = b"".join(branch_record(topic) for topic in topics)
    alch_contents = b"".join(alch_record(item) for item in alchemy_items)
    book_contents = b"".join(
        courier_book_record(index) for index in range(COURIER_BOOK_COUNT)
    )
    global_contents = (
        order_availability_global_record()
        + smith_services_global_record()
    )
    for local_id, editor_id in SERVICE_ACCESS_GLOBALS.values():
        global_contents += record("GLOB", local_to_form(local_id),
            field("EDID", zstring(editor_id)) + field("FNAM", b"f") + field("FLTV", f32(0.0)))
        manifest.append(("GLOB", editor_id, local_to_form(local_id),
                         "Service access: 0 unavailable, 1 available"))
    additional_records = (
        exclusive_keyword_records()
        + exclusive_mgef_records()
        + exclusive_ench_records()
        + exclusive_perk_records()
        + skyforge_equipment_records()
        + skyforge_temper_records()
        + [
            (
                "FLST",
                "AOS_FLST_EnchantRing",
                EXCLUSIVE_FORMS["list_ring"],
                form_list_record(
                    EXCLUSIVE_FORMS["list_ring"],
                    "AOS_FLST_EnchantRing",
                    [master_ref(SKYRIM, 0x10CD09)],
                ),
            ),
            (
                "FLST",
                "AOS_FLST_EnchantCuirass",
                EXCLUSIVE_FORMS["list_cuirass"],
                form_list_record(
                    EXCLUSIVE_FORMS["list_cuirass"],
                    "AOS_FLST_EnchantCuirass",
                    [master_ref(SKYRIM, 0x06C0EC)],
                ),
            ),
        ]
    )
    support_records = additional_records

    for topic in topics:
        manifest.append(("DLBR", topic.branch_edid, local_to_form(topic.branch_local), topic.service))
    manifest.append(("QUST", "AOS_DialogueQuest", local_to_form(0x0800), "Dialogue"))
    manifest.append(("QUST", "AOS_MCMQuest", local_to_form(MCM_QUEST_LOCAL_ID), "Optional MCM Helper menu"))
    manifest.append((
        "GLOB",
        "AOS_HasOrdersForCurrentSpeaker",
        local_to_form(ORDER_AVAILABILITY_GLOBAL_LOCAL_ID),
        "Dynamic smithing-order dialogue availability",
    ))
    manifest.append((
        "GLOB",
        "AOS_SmithServicesForCurrentSpeaker",
        local_to_form(SMITH_SERVICES_GLOBAL_LOCAL_ID),
        "Dynamic smithing-service dialogue availability",
    ))
    for item in alchemy_items:
        manifest.append(("ALCH", item.editor_id, item.form_id, f"{item.item_type}/{item.tier}"))
    for index in range(COURIER_BOOK_COUNT):
        local_id = COURIER_BOOK_FIRST_LOCAL_ID + index
        manifest.append((
            "BOOK",
            f"AOS_CourierNotice{index + 1:03d}",
            local_to_form(local_id),
            "Unique runtime smithing courier notice",
        ))
    for signature, editor_id, local_id, _payload in additional_records:
        manifest.append((
            signature,
            editor_id,
            local_to_form(local_id),
            f"Artisans of Skyrim {project_version.TEXT} generated support record",
        ))
    additional_groups: dict[str, bytes] = {}
    for signature in ("KYWD", "MGEF", "ENCH", "PERK", "WEAP", "ARMO", "COBJ", "FLST"):
        payload = b"".join(
            record_data
            for record_signature, _editor_id, _local_id, record_data in support_records
            if record_signature == signature
        )
        if payload:
            additional_groups[signature] = group(signature.encode("ascii"), 0, payload)

    groups = [
        group(b"DIAL", 0, dial_contents),
        group(b"QUST", 0, quest_contents),
        group(b"DLBR", 0, branch_contents),
        group(b"GLOB", 0, global_contents),
        additional_groups.pop("KYWD"),
        additional_groups.pop("MGEF"),
        group(b"ALCH", 0, alch_contents),
        group(b"BOOK", 0, book_contents),
    ] + list(additional_groups.values())
    total = 1 + sum(1 for _ in manifest) + len(groups) + len(topics)
    next_object_id = max(
        0x0A00 + len(alchemy_items) + 1,
        COURIER_BOOK_FIRST_LOCAL_ID + COURIER_BOOK_COUNT,
        max(EXCLUSIVE_FORMS.values()) + 1,
    )
    plugin = tes4_record(total, next_object_id) + b"".join(groups)
    write_manifest(manifest)
    return plugin


def write_manifest(rows: list[tuple[str, str, int, str]]) -> None:
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    with MANIFEST_PATH.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["Signature", "EditorID", "FormID", "LocalFormID", "Purpose"])
        for signature, editor_id, form_id, purpose in rows:
            writer.writerow([signature, editor_id, f"{form_id:08X}", f"{form_id & 0xFFFFFF:06X}", purpose])


def update_alchemy_csv(alchemy_items: list[AlchemyRecord]) -> None:
    by_editor_id = {item.editor_id: item for item in alchemy_items}
    with CSV_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
        fieldnames = list(reader.fieldnames or [])
    for row in rows:
        item = by_editor_id[row["EditorID"]]
        row["RuntimeItemFormKey"] = item.form_key
        row["ValidationStatus"] = "GENERATED_IN_ARTISANSOF_SKYRIM_ESP"
        row["Notes"] = (
            f"Generated in {PLUGIN_NAME} as FormID {item.form_id:08X}; "
            f"service batch price {item.batch_price}, ALCH value per flask {item.alch_value}."
        )
    with CSV_PATH.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    with FORMID_EXPORT_PATH.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "EditorID",
                "DisplayName",
                "FormKey",
                "FormIDInPluginFile",
                "LocalFormID",
                "Type",
                "Tier",
                "BatchPrice",
                "ALCHValuePerFlask",
            ]
        )
        for item in alchemy_items:
            writer.writerow(
                [
                    item.editor_id,
                    item.display_name,
                    item.form_key,
                    f"{item.form_id:08X}",
                    f"{item.local_id:06X}",
                    item.item_type,
                    item.tier,
                    item.batch_price,
                    item.alch_value,
                ]
            )


def update_catalog_csv(alchemy_items: list[AlchemyRecord]) -> None:
    by_source_id = {item.source_id: item for item in alchemy_items}
    with CATALOG_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        rows = [row for row in reader if row["id"] in by_source_id]
        fieldnames = list(reader.fieldnames or [])
    for row in rows:
        item = by_source_id.get(row["id"])
        if not item:
            continue
        row["displayName"] = item.display_name
        row["itemType"] = item.item_type
        row["effect"] = " + ".join(effect[0] for effect in item.effects)
        row["group"] = item.group
        row["tier"] = item.tier
        row["batchSize"] = str(item.batch_size)
        row["batchPrice"] = str(item.batch_price)
        row["alchValue"] = str(item.alch_value)
        if "_over_time_" in item.source_id:
            _name, magnitude, duration = item.effects[0]
            row["valueText"] = (
                f"{magnitude:g} points per second for {duration} seconds"
            )
        row["itemForm"] = item.form_key
        row["notes"] = (
            f"Generated in {PLUGIN_NAME} as FormID {item.form_id:08X}; "
            "runtime resolves this plugin-local FormID."
        )
    with CATALOG_PATH.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


ALCHEMY_NPCS = [
    ("Babette", "Babette", "Skyrim.esm", "0001D4B7", "Master", "", ""),
    ("Bothela", "Bothela", "Skyrim.esm", "0001338B", "Master", "", ""),
    ("Nurelion", "Nurelion", "Skyrim.esm", "00014148", "Master", "", ""),
    (
        "Florentius Baenius",
        "DLC1FlorentiusBaenius",
        "Dawnguard.esm",
        "0000336D",
        "Master",
        "Dawnguard.esm|00013F1F",
        "Florentius will offer this service after he has joined the Dawnguard.",
    ),
    ("Arcadia", "Arcadia", "Skyrim.esm", "00013BA4", "Expert", "", ""),
    ("Milore Ienth", "DLC2RRMiloreIenth", "Dragonborn.esm", "000182A1", "Expert", "", ""),
    ("Elgrim", "Elgrim", "Skyrim.esm", "00013357", "Expert", "", ""),
    ("Frida", "Frida", "Skyrim.esm", "00013614", "Expert", "", ""),
    ("Quintus Navale", "QuintusNavale", "Skyrim.esm", "0001414C", "Expert", "", ""),
]


def alchemy_npc_blocks() -> str:
    blocks = []
    for name, editor_id, plugin, form_id, tier, required_quest, locked_reason in ALCHEMY_NPCS:
        optional_lines = []
        if required_quest:
            optional_lines.extend(
                [
                    f'requiredQuest = "{required_quest}"',
                    f'lockedReason = "{locked_reason}"',
                ]
            )
        blocks.append(
            "\n".join(
                [
                    "[[alchemy.npcs]]",
                    f'name = "{name}"',
                    f'editorID = "{editor_id}"',
                    f'plugin = "{plugin}"',
                    f'form = "{plugin}|{form_id}"',
                    f'tier = "{tier}"',
                    *optional_lines,
                    'validation = "RESOLVED_FROM_MASTER"',
                ]
            )
        )
    return "\n\n".join(blocks) + "\n\n"


def update_config(alchemy_items: list[AlchemyRecord]) -> None:
    text = CONFIG_PATH.read_text(encoding="utf-8")
    text = text.replace('alchemyFaction = "JobApothecaryFaction"', 'alchemyFaction = "Skyrim.esm|0005091C"')
    text = re.sub(
        r"^\[\[alchemy\.npcs\]\].*?(?=^\[\[enchantments\]\])",
        alchemy_npc_blocks(),
        text,
        flags=re.S | re.M,
    )

    by_source_id = {item.source_id: item for item in alchemy_items}
    form_by_id = {item.source_id: item.form_key for item in alchemy_items}

    def replace_item_block(match: re.Match[str]) -> str:
        block = match.group(0)
        id_match = re.search(r'^id = "([^"]+)"', block, flags=re.M)
        if not id_match:
            return block
        item_id = id_match.group(1)
        form_key = form_by_id.get(item_id)
        if not form_key:
            # The source CSV is authoritative. Removing a row must also remove
            # its stale runtime service block instead of leaving dead content.
            return ""
        item = by_source_id[item_id]
        block = re.sub(
            r'^displayName = ".*?"',
            f'displayName = "{item.display_name}"',
            block,
            flags=re.M,
        )
        effect_text = " + ".join(effect[0] for effect in item.effects)
        block = re.sub(
            r'^effect = ".*?"',
            f'effect = "{effect_text}"',
            block,
            flags=re.M,
        )
        groups = [value.strip() for value in item.group.split(";") if value.strip()]
        group_line = (
            "groups = [" + ", ".join(f'"{value}"' for value in groups) + "]"
            if len(groups) > 1
            else f'group = "{groups[0]}"'
        )
        if re.search(r'^(?:group|groups) = ', block, flags=re.M):
            block = re.sub(r'^(?:group|groups) = .*$', group_line, block, flags=re.M)
        else:
            block += "\n" + group_line
        block = re.sub(r'^itemForm = ".*?"', f'itemForm = "{form_key}"', block, flags=re.M)
        block = re.sub(
            r'^batchPrice = .*$',
            f'batchPrice = {item.batch_price}',
            block,
            flags=re.M,
        )
        block = re.sub(
            r'^alchValue = .*$',
            f'alchValue = {item.alch_value}',
            block,
            flags=re.M,
        )
        if "_over_time_" in item_id:
            _name, magnitude, duration = item.effects[0]
            value_text = (
                f"{magnitude:g} points per second for {duration} seconds"
            )
            block = re.sub(
                r'^valueText = ".*?"',
                f'valueText = "{value_text}"',
                block,
                flags=re.M,
            )
        if re.search(r'^formStatus = ', block, flags=re.M):
            block = re.sub(
                r'^formStatus = ".*?"',
                'formStatus = "GENERATED_IN_ARTISANSOF_SKYRIM_ESP"',
                block,
                flags=re.M,
            )
        else:
            block += '\nformStatus = "GENERATED_IN_ARTISANSOF_SKYRIM_ESP"'
        block = re.sub(
            r'^notes = ".*?"',
            'notes = "ALCH record generated in ArtisansOfSkyrim.esp; runtime itemForm is a real plugin-local FormID."',
            block,
            flags=re.M,
        )
        return block

    # Anchored to line starts: header comments also mention [[alchemy.items]].
    text = re.sub(r"^\[\[alchemy\.items\]\].*?(?=\n\[\[alchemy\.items\]\]|\Z)", replace_item_block, text, flags=re.S | re.M)
    CONFIG_PATH.write_text(text, encoding="utf-8", newline="\n")
    DIST_CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    DIST_CONFIG_PATH.write_text(text, encoding="utf-8", newline="\n")


def update_npc_tiers() -> None:
    with NPC_TIERS_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
        fieldnames = list(reader.fieldnames or [])
    by_name = {entry[0]: entry for entry in ALCHEMY_NPCS}
    for row in rows:
        if row["Service"] != "Alchemy":
            continue
        entry = by_name[row["DisplayName"]]
        _, editor_id, plugin, form_id, tier, required_quest, _locked_reason = entry
        row["Tier"] = tier
        row["Plugin"] = plugin
        row["FormID"] = form_id
        row["EditorID"] = editor_id
        row["PrimaryKey"] = f"{plugin}|{form_id}"
        row["ValidationStatus"] = "RESOLVED_FROM_MASTER"
        row["Notes"] = (
            f"Service requires completed quest {required_quest}."
            if required_quest
            else "Resolved from local Skyrim masters; display name is documentation only."
        )
    with NPC_TIERS_PATH.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    alchemy_items = read_alchemy_records()
    update_config(alchemy_items)
    update_npc_tiers()
    npc_conditions = read_npc_dialogue_conditions()
    dialogue_responses = read_dialogue_responses(npc_conditions)
    ESP_PATH.parent.mkdir(parents=True, exist_ok=True)
    ESP_PATH.write_bytes(build_esp(alchemy_items, dialogue_responses))
    SEQ_PATH.parent.mkdir(parents=True, exist_ok=True)
    SEQ_PATH.write_bytes(form_bytes(local_to_form(0x0800)))
    update_alchemy_csv(alchemy_items)
    update_catalog_csv(alchemy_items)
    print(f"Generated {ESP_PATH}")
    print(f"Alchemy records: {len(alchemy_items)}")
    print(
        "Dialogue named NPC sources: "
        + ", ".join(
            f"{service}={len(entries)}" for service, entries in npc_conditions.items()
        )
    )
    print(
        "Dialogue voiced responses: "
        + ", ".join(
            f"{service}={len(entries)}" for service, entries in dialogue_responses.items()
        )
    )
    print(f"Regenerated {SEQ_PATH}")
    print(f"Manifest: {MANIFEST_PATH}")
    print(f"FormID export: {FORMID_EXPORT_PATH}")


if __name__ == "__main__":
    main()
