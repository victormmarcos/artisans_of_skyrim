#!/usr/bin/env python3
"""Validate the public FOMOD archive before release."""

from __future__ import annotations

import argparse
import re
import tomllib
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

import project_version
from ValidateRuntimeAssets import validate_dll, validate_esp, validate_swf

from ValidateArtisansOfSkyrimAlchemyEsp import (
    one_field,
    parse_fields,
    walk_records,
    ztext,
)


VERSION = project_version.TEXT
CORE = {
    "Core/Interface/ArtisansOfSkyrim/ServiceMenu.swf",
    "Core/Interface/ArtisansOfSkyrim/mcm_logo.dds",
    "Core/ArtisansOfSkyrim.esp",
    "Core/SEQ/ArtisansOfSkyrim.seq",
    "Core/SKSE/Plugins/ArtisansOfSkyrim.dll",
    "Core/SKSE/Plugins/ArtisansOfSkyrim.toml",
    "Core/Scripts/ArtisansOfSkyrim.pex",
    "Core/Scripts/AOS_TIF_SmithCraft.pex",
    "Core/Scripts/AOS_TIF_SmithImprove.pex",
    "Core/Scripts/AOS_TIF_Enchant.pex",
    "Core/Scripts/AOS_TIF_Alchemy.pex",
    "Core/Scripts/AOS_TIF_CollectOrders.pex",
    "Core/Scripts/AOS_MCM.pex",
    "Core/MCM/Config/ArtisansOfSkyrim/settings.ini",
    "Core/MCM/Config/ArtisansOfSkyrim/config.json",
    "Core/Interface/Translations/ArtisansOfSkyrim_ENGLISH.txt",
    "Core/Interface/Translations/ArtisansOfSkyrim_FRENCH.txt",
    "Core/Interface/Translations/ArtisansOfSkyrim_GERMAN.txt",
    "Core/Interface/Translations/ArtisansOfSkyrim_ITALIAN.txt",
    "Core/Interface/Translations/ArtisansOfSkyrim_SPANISH.txt",
    "Core/Interface/Translations/ArtisansOfSkyrim_POLISH.txt",
    "Core/Interface/Translations/ArtisansOfSkyrim_RUSSIAN.txt",
    "Core/Interface/Translations/ArtisansOfSkyrim_JAPANESE.txt",
    "Core/Interface/Translations/ArtisansOfSkyrim_CHINESE.txt",
}
MCM_VARIANTS = ("enchantments", "potions", "enchantments_potions")
# folder: (file, [features] key)
OPTIONS = {
    "skyforge_equipment": ("ArtisansOfSkyrim_SkyforgeEquipment.toml", "enableSkyforgeEquipment"),
    "custom_enchantments": ("ArtisansOfSkyrim_CustomEnchantments.toml", "enableCustomEnchantments"),
    "custom_potions": ("ArtisansOfSkyrim_CustomPotions.toml", "enableCustomPotions"),
}
PRESETS = {
    "official_instant": (False, False),
    "official_delayed": (False, True),
    "mod_added_instant": (True, False),
    "mod_added_delayed": (True, True),
}
COURIER_LOCATIONS = {
    "Eorlund Gray-Mane": "Skyforge in Whiterun",
    "Gunmar": "Fort Dawnguard",
    "Baldor Iron-Shaper": "forge in Skaal Village",
    "Balimund": "forge in Riften",
    "Moth gro-Bagol": "forge in Markarth",
    "Oengul War-Anvil": "forge in Windhelm",
    "Gharol": "forge in Dushnikh Yal",
    "Glover Mallory": "forge in Raven Rock",
    "Ulfberth War-Bear": "forge in Whiterun",
    "Beirand": "forge in Solitude",
    "Ghorza gra-Bagol": "forge in Markarth",
    "Tacitus Sallustius": "forge in Markarth",
    "Asbjorn Fire-Tamer": "forge in Riften",
    "Heimvar": "forge in Solitude",
    "Hermir Strong-Heart": "forge in Windhelm",
}
ORDER_HOURS = {
    "daggerHours": 24.0,
    "swordHours": 48.0,
    "warAxeHours": 48.0,
    "maceHours": 48.0,
    "bootsHours": 48.0,
    "gauntletsHours": 48.0,
    "bowHours": 48.0,
    "shieldHours": 48.0,
    "helmetHours": 48.0,
    "greatswordHours": 72.0,
    "battleaxeHours": 72.0,
    "warhammerHours": 72.0,
    "lightBodyArmorHours": 48.0,
    "heavyBodyArmorHours": 72.0,
    "crossbowHours": 72.0,
}
PROFILE_ROOT = re.compile(rb"[A-Za-z]:[\\/]Users[\\/][^\\/\x00]+", re.I)


def validate_open(archive: zipfile.ZipFile, label: str) -> None:
    expected = CORE | {
        "fomod/info.xml",
        "fomod/ModuleConfig.xml",
        "THIRD_PARTY_NOTICES.md",
        "LICENSE.txt",
        "README - Translations.md",
        "README - Patches.md",
    } | {
        f"Presets/{name}/SKSE/Plugins/ArtisansOfSkyrim.toml" for name in PRESETS
    } | {
        f"Mcm/{name}/MCM/Config/ArtisansOfSkyrim/config.json" for name in MCM_VARIANTS
    } | {
        f"Options/{folder}/SKSE/Plugins/{file_name}"
        for folder, (file_name, _key) in OPTIONS.items()
    }

    names = {info.filename for info in archive.infolist() if not info.is_dir()}
    assert names == expected, (
        f"Unexpected package entries. Missing={sorted(expected - names)}, "
        f"extra={sorted(names - expected)}"
    )

    assert b"GNU GENERAL PUBLIC LICENSE" in archive.read("LICENSE.txt")[:200], "LICENSE.txt is not the GPL-3.0 text"

    info_root = ET.fromstring(archive.read("fomod/info.xml"))
    assert info_root.findtext("Version") == VERSION
    author = ET.parse(Path(__file__).resolve().parents[1] / "fomod/info.xml").getroot().findtext("Author")
    assert author and info_root.findtext("Author") == author, "Official FOMOD author changed"
    assert info_root.findtext("Website") == "https://www.nexusmods.com/skyrimspecialedition/mods/185212"
    module_root = ET.fromstring(archive.read("fomod/ModuleConfig.xml"))
    assert module_root.findtext("moduleName") == f"Artisans of Skyrim {VERSION}"
    steps = module_root.findall("./installSteps/installStep")
    assert [step.attrib["name"] for step in steps] == [
        "Equipment Compatibility",
        "Smithing Orders",
        "Custom Effects",
    ]
    smith_plugins = steps[1].findall("./optionalFileGroups/group/plugins/plugin")
    assert [plugin.attrib["name"] for plugin in smith_plugins] == [
        "Timed Orders with Courier Notices",
        "Immediate Crafting",
    ]
    assert smith_plugins[0].find("./typeDescriptor/type").attrib["name"] == "Recommended"
    custom_group = steps[2].find("./optionalFileGroups/group")
    assert custom_group is not None and custom_group.attrib["type"] == "SelectAny"
    exclusive_plugins = custom_group.findall("./plugins/plugin")
    assert [plugin.attrib["name"] for plugin in exclusive_plugins] == [
        "Custom Skyforge Equipment",
        "Custom Enchantments",
        "Custom Potions",
    ]
    for plugin, folder in zip(exclusive_plugins, OPTIONS, strict=True):
        source = plugin.find("./files/folder")
        assert source is not None and source.attrib["source"] == f"Options/{folder}"
        assert plugin.find("./typeDescriptor/type").attrib["name"] == "Recommended"

    for folder, (file_name, key) in OPTIONS.items():
        option = tomllib.loads(
            archive.read(f"Options/{folder}/SKSE/Plugins/{file_name}").decode("utf-8")
        )
        assert option == {"features": {key: True}}, f"Unexpected option file {file_name}"

    fallback = tomllib.loads(
        archive.read("Core/SKSE/Plugins/ArtisansOfSkyrim.toml").decode("utf-8")
    )
    assert fallback["rules"]["includeModAddedEquipment"] is False
    assert fallback["orders"]["enabled"] is True
    for _file_name, key in OPTIONS.values():
        assert fallback["features"][key] is False
    assert fallback["forms"]["potemaFirstQuestEditorID"] == "MS06Start"

    for name, (include_mod_added, delayed) in PRESETS.items():
        entry = f"Presets/{name}/SKSE/Plugins/ArtisansOfSkyrim.toml"
        config = tomllib.loads(archive.read(entry).decode("utf-8"))
        assert config["rules"]["includeModAddedEquipment"] is include_mod_added
        assert config["orders"]["enabled"] is delayed
        for _file_name, key in OPTIONS.values():
            assert config["features"][key] is False
        assert config["forms"]["potemaFirstQuestEditorID"] == "MS06Start"
        assert config["pricing"]["grandSoulWeaponCharge"] == 3000
        florentius = next(
            entry
            for entry in config["alchemy"]["npcs"]
            if entry["name"] == "Florentius Baenius"
        )
        assert florentius["tier"] == "Master"
        assert florentius["form"] == "Dawnguard.esm|0000336D"
        assert florentius["requiredQuest"] == "Dawnguard.esm|00013F1F"
        full_smithing_labels = {
            entry["form"]
            for entry in config["smithing"]["menuLabelOverrides"]
            if entry.get("preserveFull")
        }
        assert len(full_smithing_labels) == 10
        assert config["smithing"]["materialLabelOverrides"] == [
            {"form": "Skyrim.esm|000F6F23", "label": "Steel"}
        ]
        alchemy_items = {entry["id"]: entry for entry in config["alchemy"]["items"]}
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
        for key, hours in ORDER_HOURS.items():
            assert config["orders"]["smithing"][key] == hours
        actual_locations = {
            entry["name"]: entry.get("courierLocation")
            for entry in config["npcs"]["overrides"]
        }
        assert actual_locations == COURIER_LOCATIONS

    esp_records = list(walk_records(archive.read("Core/ArtisansOfSkyrim.esp")))
    tes4 = [record for record in esp_records if record.signature == "TES4"]
    assert len(tes4) == 1
    tes4_fields = parse_fields(tes4[0].body)
    assert ztext(one_field(tes4_fields, "CNAM")) == "Artisans of Skyrim"
    assert len([record for record in esp_records if record.signature == "INFO"]) == 111
    assert len([record for record in esp_records if record.signature == "GLOB"]) == 5
    alchemy = [record for record in esp_records if record.signature == "ALCH"]
    assert len(alchemy) == 63
    alchemy_names = [
        ztext(one_field(parse_fields(record.body), "FULL"))
        for record in alchemy
    ]
    assert all(
        name.startswith("Potion of ") or name.startswith("Poison of ")
        for name in alchemy_names
    )
    books = [record for record in esp_records if record.signature == "BOOK"]
    assert len(books) == 1024
    assert [record.form_id & 0xFFFFFF for record in books] == list(
        range(0x0B00, 0x0F00)
    )
    support_counts = {
        "KYWD": 3,
        "MGEF": 10,
        "ENCH": 6,
        "PERK": 8,
        "WEAP": 7,
        "ARMO": 5,
        "COBJ": 12,
        "FLST": 2,
    }
    for signature, expected_count in support_counts.items():
        assert len([record for record in esp_records if record.signature == signature]) == expected_count
    validate_esp(archive.read("Core/ArtisansOfSkyrim.esp"))
    swf_body = validate_swf(archive.read("Core/Interface/ArtisansOfSkyrim/ServiceMenu.swf"))
    assert not PROFILE_ROOT.search(swf_body)
    dll = archive.read("Core/SKSE/Plugins/ArtisansOfSkyrim.dll")
    validate_dll(dll)
    assert dll[:2] == b"MZ", "Release DLL is not a PE image"
    pe_offset = int.from_bytes(dll[0x3C:0x40], "little")
    assert dll[pe_offset : pe_offset + 4] == b"PE\0\0", "Release DLL has no PE signature"
    assert int.from_bytes(dll[pe_offset + 4 : pe_offset + 6], "little") == 0x8664, (
        "Release DLL is not an x64 image"
    )
    assert b"MinHook" in archive.read("THIRD_PARTY_NOTICES.md")

    for name in names:
        data = archive.read(name)
        assert not PROFILE_ROOT.search(data.replace(b"\x00", b"")), f"Private profile root in {name}"

    print(f"Validated {len(expected)} exact entries in {label}")
    print("FOMOD version, recommended profiles, four presets and three options: OK")
    print("ESP author, 111 INFOs, 63 normalized ALCH records, stable support records, five globals, and 1024 unique BOOK records: OK")
    print("Private profile paths: none")


def validate(path: Path) -> None:
    with zipfile.ZipFile(path) as archive:
        validate_open(archive, str(path))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "archive",
        type=Path,
        default=Path("Artisans of Skyrim.zip"),
        nargs="?",
    )
    args = parser.parse_args()
    validate(args.archive)


if __name__ == "__main__":
    main()
