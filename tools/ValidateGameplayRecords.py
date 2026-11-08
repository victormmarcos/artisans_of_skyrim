#!/usr/bin/env python3
"""Validate the generated gameplay support records (weapons, armor, perks, potions) and compatibility ranges."""

from __future__ import annotations

import struct
import tomllib

import GenerateArtisansOfSkyrimEsp as generator
from ValidateArtisansOfSkyrimAlchemyEsp import (
    one_field,
    parse_fields,
    walk_records,
    ztext,
)


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def f32(data: bytes, offset: int = 0) -> float:
    return struct.unpack_from("<f", data, offset)[0]


def i32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def perk_entries(fields: list[tuple[str, bytes]]) -> list[list[tuple[str, bytes]]]:
    result = []
    current = None
    for signature, data in fields:
        if signature == "PRKE":
            assert current is None
            current = [(signature, data)]
            continue
        if current is None:
            continue
        current.append((signature, data))
        if signature == "PRKF":
            result.append(current)
            current = None
    assert current is None
    return result


def perk_entry_data(block: list[tuple[str, bytes]]) -> bytes:
    return next(data for signature, data in block if signature == "DATA")


def perk_entry_value(block: list[tuple[str, bytes]]) -> float:
    return f32(next(data for signature, data in block if signature == "EPFD"))


def perk_conditions(block: list[tuple[str, bytes]]) -> list[tuple[int, int, int]]:
    result = []
    parameter_group = None
    for signature, data in block:
        if signature == "PRKC":
            parameter_group = data[0]
        elif signature == "CTDA":
            assert parameter_group is not None
            result.append((parameter_group, u32(data, 8), u32(data, 12)))
    return result


def top_level_group_order(blob: bytes) -> list[str]:
    result = []
    offset = 0
    while offset + 24 <= len(blob):
        signature = blob[offset : offset + 4]
        size = u32(blob, offset + 4)
        if signature == b"GRUP":
            result.append(blob[offset + 8 : offset + 12].decode("ascii"))
            offset += size
        else:
            offset += 24 + size
    assert offset == len(blob)
    return result


def main() -> None:
    plugin = generator.ESP_PATH.read_bytes()
    records = list(walk_records(plugin))
    group_order = top_level_group_order(plugin)
    assert group_order.index("KYWD") < group_order.index("MGEF")
    assert group_order.index("MGEF") < group_order.index("ALCH")
    by_editor_id = {}
    for entry in records:
        fields = parse_fields(entry.body)
        edids = [value for signature, value in fields if signature == "EDID"]
        if edids:
            by_editor_id[ztext(edids[0])] = entry

    expected_counts = {
        "ALCH": 63,
        "BOOK": 1024,
        "KYWD": 3,
        "MGEF": 10,
        "ENCH": 6,
        "PERK": 8,
        "WEAP": 7,
        "ARMO": 5,
        "COBJ": 12,
        "FLST": 2,
    }
    for signature, expected in expected_counts.items():
        actual = sum(entry.signature == signature for entry in records)
        assert actual == expected, f"{signature}: expected {expected}, found {actual}"

    books = [entry.form_id & 0xFFFFFF for entry in records if entry.signature == "BOOK"]
    assert books == list(range(0x0B00, 0x0F00))
    generated_support = [
        entry.form_id & 0xFFFFFF
        for entry in records
        if entry.signature in {"KYWD", "MGEF", "ENCH", "PERK", "WEAP", "ARMO", "COBJ", "FLST"}
    ]
    assert min(generated_support) >= 0x0F00

    weapons = {
        "AOS_WeapSkyforgeDagger": (290, 2.5, 10),
        "AOS_WeapSkyforgeSword": (720, 10.0, 13),
        "AOS_WeapSkyforgeWarAxe": (865, 12.0, 15),
        "AOS_WeapSkyforgeMace": (1000, 14.0, 16),
        "AOS_WeapSkyforgeGreatsword": (1440, 17.0, 22),
        "AOS_WeapSkyforgeBattleaxe": (1585, 21.0, 23),
        "AOS_WeapSkyforgeWarhammer": (1725, 25.0, 25),
    }
    for editor_id, (value, weight, damage) in weapons.items():
        data = one_field(parse_fields(by_editor_id[editor_id].body), "DATA")
        assert u32(data) == value
        assert abs(f32(data, 4) - weight) < 0.0001
        assert struct.unpack_from("<H", data, 8)[0] == damage

    armor = {
        "AOS_ArmorSkyforgePlateBoots": (275, 9.0, 16),
        "AOS_ArmorSkyforgePlateCuirass": (1500, 38.0, 43),
        "AOS_ArmorSkyforgePlateGauntlets": (275, 6.0, 16),
        "AOS_ArmorSkyforgePlateHelmet": (750, 6.0, 21),
        "AOS_ArmorSkyforgeSteelShield": (750, 12.0, 32),
    }
    for editor_id, (value, weight, rating) in armor.items():
        fields = parse_fields(by_editor_id[editor_id].body)
        data = one_field(fields, "DATA")
        assert u32(data) == value
        assert abs(f32(data, 4) - weight) < 0.0001
        assert u32(one_field(fields, "DNAM")) == rating * 100
        keywords = one_field(fields, "KWDA")
        material_keywords = {
            u32(keywords, offset) for offset in range(0, len(keywords), 4)
        }
        expected_material = generator.master_ref(
            generator.SKYRIM,
            0x06BBE6 if editor_id == "AOS_ArmorSkyforgeSteelShield" else 0x06BBE7,
        )
        assert expected_material in material_keywords
    assert sum(rating for _value, _weight, rating in armor.values()) == 128
    assert sum(value for value, _weight, _rating in armor.values()) == 3550

    shield = by_editor_id["AOS_ArmorSkyforgeSteelShield"]
    assert u32(shield.header, 8) & 0x40, "Skyforge Steel Shield lost the native shield record flag"
    shield_temper = parse_fields(by_editor_id["AOS_TemperSkyforgeSteelShield"].body)
    assert u32(one_field(shield_temper, "CNAM")) == generator.local_to_form(
        generator.EXCLUSIVE_FORMS["skyforge_shield"]
    )

    marker_specs = {
        "AOS_MGEF_SilverBlessing": ("Silver Blessing", True),
        "AOS_MGEF_SilverBlessingMaster": ("Silver Blessing", True),
        "AOS_MGEF_ElementalBurst": ("Elemental Burst", False),
        "AOS_MGEF_BerserkerSoul": ("Berserker Soul", True),
        "AOS_MGEF_HonedMetal": ("Honed Metal", True),
        "AOS_MGEF_Unburden": ("Unburden", False),
        "AOS_MGEF_Reinforcement": ("Reinforcement", False),
    }
    weapon_markers = {
        "AOS_MGEF_SilverBlessing",
        "AOS_MGEF_SilverBlessingMaster",
        "AOS_MGEF_BerserkerSoul",
        "AOS_MGEF_HonedMetal",
    }
    for editor_id, (display_name, hostile) in marker_specs.items():
        fields = parse_fields(by_editor_id[editor_id].body)
        assert ztext(one_field(fields, "FULL")) == display_name
        assert ztext(one_field(fields, "DNAM")), f"{editor_id} has no visible description"
        flags = u32(one_field(fields, "DATA"))
        assert not flags & 0x00008000, f"{editor_id} is hidden from the inventory UI"
        assert bool(flags & 0x1) == hostile
        data = one_field(fields, "DATA")
        assert len(data) == 152
        assert f32(data, 4) == 1.0
        assert i32(data, 16) == -1
        assert u32(data, 64) == 1 and i32(data, 68) == -1
        assert i32(data, 88) == -1
        expected_delivery = (1, 2) if editor_id in weapon_markers else (0, 0)
        assert (u32(data, 80), u32(data, 84)) == expected_delivery

    exclusive_enchants = {
        "AOS_ENCH_SilverBlessing": "mgef_silver_blessing",
        "AOS_ENCH_ElementalBurst": "mgef_elemental_burst",
        "AOS_ENCH_BerserkerSoul": "mgef_berserker_soul",
        "AOS_ENCH_HonedMetal": "mgef_honed_metal",
        "AOS_ENCH_Unburden": "mgef_unburden",
        "AOS_ENCH_Reinforcement": "mgef_reinforcement",
    }
    for editor_id, marker_key in exclusive_enchants.items():
        fields = parse_fields(by_editor_id[editor_id].body)
        assert u32(one_field(fields, "EFID")) == generator.local_to_form(
            generator.EXCLUSIVE_FORMS[marker_key]
        )
        assert f32(one_field(fields, "EFIT")) == 1.0

    overtime_mgefs = {
        "AOS_MGEF_HealthOverTime": (0x001058A2, 24, "keyword_health_over_time"),
        "AOS_MGEF_MagickaOverTime": (0x001058A7, 25, "keyword_magicka_over_time"),
        "AOS_MGEF_StaminaOverTime": (0x001058A3, 26, "keyword_stamina_over_time"),
    }
    for editor_id, (source_form_id, primary_actor_value, keyword_key) in overtime_mgefs.items():
        fields = parse_fields(by_editor_id[editor_id].body)
        source_fields = generator.snapshot_fields(source_form_id)
        expected_data = bytearray(one_field(source_fields, "DATA"))
        keyword_form = generator.local_to_form(
            generator.EXCLUSIVE_FORMS[keyword_key]
        )
        struct.pack_into("<I", expected_data, 8, keyword_form)
        struct.pack_into(
            "<I",
            expected_data,
            64,
            generator.MGEF_ARCHETYPE_PEAK_VALUE_MODIFIER,
        )
        struct.pack_into("<i", expected_data, 68, primary_actor_value)
        assert one_field(fields, "DATA") == bytes(expected_data)
        assert one_field(fields, "SNDD") == one_field(source_fields, "SNDD")
        data = one_field(fields, "DATA")
        flags = u32(data)
        assert not flags & 0x00000002  # Recover stays off for per-second restore.
        assert not flags & 0x00000200  # The effect retains its duration.
        assert not flags & 0x00020000  # Equal doses may renew the duration.
        assert u32(data, 8) == keyword_form
        assert (
            u32(data, 64)
            == generator.MGEF_ARCHETYPE_PEAK_VALUE_MODIFIER
        )
        assert i32(data, 68) == primary_actor_value
        assert u32(one_field(fields, "KSIZ")) == 1
        assert u32(one_field(fields, "KWDA")) == keyword_form

    perk_specs = {
        "AOS_PERK_ReinforcementExpert": ([(33, 3, 2, 0.8)], 1),
        "AOS_PERK_ReinforcementMaster": ([(33, 3, 2, 0.6)], 1),
        "AOS_PERK_ElementalBurstExpert": ([(29, 3, 3, 1.25)] * 3, 3),
        "AOS_PERK_ElementalBurstMaster": ([(29, 3, 3, 1.5)] * 3, 3),
        "AOS_PERK_ElementalBurstCombined": ([(29, 3, 3, 1.75)] * 3, 3),
        "AOS_PERK_ElementalBurstMaximum": ([(29, 3, 3, 2.0)] * 3, 3),
    }
    for editor_id, (expected, expected_count) in perk_specs.items():
        blocks = perk_entries(parse_fields(by_editor_id[editor_id].body))
        assert len(blocks) == expected_count
        actual = []
        for block in blocks:
            data = perk_entry_data(block)
            actual.append((data[0], data[1], data[2], perk_entry_value(block)))
        for actual_entry, expected_entry in zip(actual, expected, strict=True):
            assert actual_entry[:3] == expected_entry[:3]
            assert abs(actual_entry[3] - expected_entry[3]) < 0.0001

    for editor_id in (
        "AOS_PERK_ReinforcementExpert",
        "AOS_PERK_ReinforcementMaster",
    ):
        reinforcement_entries = perk_entries(
            parse_fields(by_editor_id[editor_id].body)
        )
        assert all(
            perk_entry_data(block)[0] != 34
            for block in reinforcement_entries
        )
        assert perk_conditions(reinforcement_entries[0]) == []

    elemental_keywords = {
        generator.master_ref(generator.SKYRIM, 0x01CEAD),
        generator.master_ref(generator.SKYRIM, 0x01CEAE),
        generator.master_ref(generator.SKYRIM, 0x01CEAF),
    }
    for editor_id in (
        "AOS_PERK_ElementalBurstExpert",
        "AOS_PERK_ElementalBurstMaster",
        "AOS_PERK_ElementalBurstCombined",
        "AOS_PERK_ElementalBurstMaximum",
    ):
        blocks = perk_entries(parse_fields(by_editor_id[editor_id].body))
        assert {
            conditions[0][2]
            for block in blocks
            if (conditions := perk_conditions(block))
        } == elemental_keywords

    for editor_id, stamina_multiplier in (
        ("AOS_PERK_UnburdenExpert", 0.85),
        ("AOS_PERK_UnburdenMaster", 0.70),
    ):
        unburden_entries = perk_entries(
            parse_fields(by_editor_id[editor_id].body)
        )
        assert len(unburden_entries) == 9
        assert [perk_entry_data(block)[0] for block in unburden_entries].count(32) == 8
        power_entries = [
            block for block in unburden_entries
            if perk_entry_data(block)[0] == 27
        ]
        assert len(power_entries) == 1
        assert abs(perk_entry_value(power_entries[0]) - stamina_multiplier) < 0.0001
        assert not any(
            signature in {"PRKC", "CTDA"}
            for signature, _data in power_entries[0]
        )

    overtime = {
        "AOS_ALCH_PotionHealthOverTimeAdept": (10.0, 10, "Health Over Time"),
        "AOS_ALCH_PotionHealthOverTimeExpert": (20.0, 10, "Health Over Time"),
        "AOS_ALCH_PotionHealthOverTimeMaster": (40.0, 10, "Health Over Time"),
        "AOS_ALCH_PotionMagickaOverTimeAdept": (10.0, 10, "Magicka Over Time"),
        "AOS_ALCH_PotionMagickaOverTimeExpert": (20.0, 10, "Magicka Over Time"),
        "AOS_ALCH_PotionMagickaOverTimeMaster": (40.0, 10, "Magicka Over Time"),
        "AOS_ALCH_PotionStaminaOverTimeAdept": (10.0, 10, "Stamina Over Time"),
        "AOS_ALCH_PotionStaminaOverTimeExpert": (20.0, 10, "Stamina Over Time"),
        "AOS_ALCH_PotionStaminaOverTimeMaster": (40.0, 10, "Stamina Over Time"),
    }
    for editor_id, (magnitude, duration, effect_name) in overtime.items():
        fields = parse_fields(by_editor_id[editor_id].body)
        effect_fields = [
            (signature, data)
            for signature, data in fields
            if signature in {"EFID", "EFIT"}
        ]
        assert len(effect_fields) == 2
        assert u32(effect_fields[0][1]) == generator.MGEF[effect_name]
        assert f32(effect_fields[1][1]) == magnitude
        assert u32(effect_fields[1][1], 8) == duration

    assert u32(one_field(parse_fields(records[0].body), "HEDR"), 8) == 0x0F3B
    with generator.CONFIG_PATH.open("rb") as handle:
        config = tomllib.load(handle)
    assert config["pricing"]["grandSoulWeaponCharge"] == 3000
    florentius = next(
        entry
        for entry in config["alchemy"]["npcs"]
        if entry["name"] == "Florentius Baenius"
    )
    assert florentius["form"] == "Dawnguard.esm|0000336D"
    assert florentius["tier"] == "Master"
    assert florentius["requiredQuest"] == "Dawnguard.esm|00013F1F"
    assert "Skyrim.esm|0003B562" in config["smithing"]["specialCategories"]["manualAdeptItems"]
    assert config["smithing"]["specialCategories"]["manualWolfItems"] == [
        "Skyrim.esm|000CAE15",
        "Skyrim.esm|000CEE7C",
        "Skyrim.esm|000CEE7E",
        "Skyrim.esm|0004C3D0",  # Wolf Helmet (1.8)
    ]
    full_smithing_labels = {
        entry["form"]: entry["label"]
        for entry in config["smithing"]["menuLabelOverrides"]
        if entry.get("preserveFull")
    }
    assert full_smithing_labels == {
        "Dragonborn.esm|00037563": "Bonemold Pauldron Armor",
        "Dragonborn.esm|0003AB26": "Improved Bonemold Armor",
        "Dragonborn.esm|0003AB25": "Improved Bonemold Boots",
        "Dragonborn.esm|0003AB22": "Improved Bonemold Gauntlets",
        "Dragonborn.esm|0003AB23": "Improved Bonemold Helmet",
        "Dragonborn.esm|0003AB24": "Improved Bonemold Shield",
        "Dragonborn.esm|0001CD98": "Nordic Carved Gauntlets",
        "Skyrim.esm|00013952": "Steel Armor (No Shoulders)",
        "Skyrim.esm|000F6F22": "Steel Armor (Shoulders)",
        "Skyrim.esm|000F6F23": "Steel Imperial Gauntlets",
    }
    assert config["smithing"]["materialLabelOverrides"] == [
        {"form": "Skyrim.esm|000F6F23", "label": "Steel"}
    ]
    alchemy_items = {entry["id"]: entry for entry in config["alchemy"]["items"]}
    for item_id in (
        "potion_health_over_time_adept",
        "potion_health_over_time_expert",
        "potion_health_over_time_master",
        "potion_magicka_over_time_adept",
        "potion_magicka_over_time_expert",
        "potion_magicka_over_time_master",
        "potion_stamina_over_time_adept",
        "potion_stamina_over_time_expert",
        "potion_stamina_over_time_master",
    ):
        item = alchemy_items[item_id]
        groups = item.get("groups", [item.get("group")])
        assert groups == ["Restore"], f"{item_id} is not exclusive to Restore"
        assert item["valueText"].endswith("for 10 seconds")
    assert alchemy_items["potion_health_over_time_adept"]["valueText"] == (
        "10 points per second for 10 seconds"
    )
    assert alchemy_items["potion_health_over_time_expert"]["valueText"] == (
        "20 points per second for 10 seconds"
    )
    assert alchemy_items["potion_health_over_time_master"]["valueText"] == (
        "40 points per second for 10 seconds"
    )
    for actor_value in ("magicka", "stamina"):
        for tier, magnitude in (("adept", 10), ("expert", 20), ("master", 40)):
            assert alchemy_items[f"potion_{actor_value}_over_time_{tier}"]["valueText"] == (
                f"{magnitude} points per second for 10 seconds"
            )
    assert {
        item_id
        for item_id, item in alchemy_items.items()
        if item.get("preserveFullMenuName")
    } == {
        "potion_fortify_destruction_expert",
        "potion_fortify_destruction_master",
        "potion_fortify_carry_weight_expert",
        "potion_fortify_carry_weight_master",
    }
    assert len(alchemy_items) == 63
    enchants = {entry["id"]: entry for entry in config["enchantments"]}
    expected_magnitudes = {
        "silver_blessing": (25, 50),
        "berserker_soul": (20, 40),
        "honed_metal": (15, 30),
        "unburden": (15, 30),
        "reinforcement": (20, 40),
        "elemental_burst": (25, 50),
    }
    for enchantment_id, (expert, master) in expected_magnitudes.items():
        definition = enchants[enchantment_id]
        assert definition["minimumTier"] == "Expert"
        assert definition["expertMagnitude"] == expert
        assert definition["masterMagnitude"] == master
        assert definition["expertPrice"] == 2000
        assert definition["masterPrice"] == 5000
        assert definition["expertPrice"] + config["pricing"]["grandSoulServiceCost"] == 3000
        assert definition["masterPrice"] + config["pricing"]["grandSoulServiceCost"] == 6000
    expected_hits = {
        ("silver_blessing", "expertChargeCost"): 150.0,
        ("silver_blessing", "masterChargeCost"): 100.0,
        ("berserker_soul", "expertChargeCost"): 100.0,
        ("berserker_soul", "masterChargeCost"): 70.0,
        ("honed_metal", "expertChargeCost"): 100.0,
        ("honed_metal", "masterChargeCost"): 70.0,
    }
    for (enchantment_id, key), target in expected_hits.items():
        actual = 3000.0 / float(enchants[enchantment_id][key])
        assert abs(actual - target) / target <= 0.05
    print("Direct-hit weapon effects, exclusive 10-second Restore potions, Long Bow, Wolf items, opt-in Skyforge records, final prices, balance, and charge targets: OK")


if __name__ == "__main__":
    main()
