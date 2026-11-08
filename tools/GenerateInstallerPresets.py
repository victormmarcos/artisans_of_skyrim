#!/usr/bin/env python3
"""Generate and validate the FOMOD configuration presets and option files.

Four presets cover equipment origin x Smithing delivery. Custom content is
chosen with three independent checkboxes; each installs a small option file
that turns on one [features] key on top of the preset.
"""

from __future__ import annotations

import re
import shutil
import tomllib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "config" / "ArtisansOfSkyrim.toml"
OUTPUT_ROOT = ROOT / "installer" / "presets"
OPTIONS_ROOT = ROOT / "installer" / "options"

# name: (includeModAddedEquipment, orders.enabled)
PRESETS = {
    "official_instant": (False, False),
    "official_delayed": (False, True),
    "mod_added_instant": (True, False),
    "mod_added_delayed": (True, True),
}

# folder name: (file name, [features] key)
OPTIONS = {
    "skyforge_equipment": ("ArtisansOfSkyrim_SkyforgeEquipment.toml", "enableSkyforgeEquipment"),
    "custom_enchantments": ("ArtisansOfSkyrim_CustomEnchantments.toml", "enableCustomEnchantments"),
    "custom_potions": ("ArtisansOfSkyrim_CustomPotions.toml", "enableCustomPotions"),
}

EXPECTED_ORDER_HOURS = {
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


def replace_section_boolean(
    text: str,
    section: str,
    key: str,
    value: bool,
) -> str:
    lines = text.splitlines(keepends=True)
    current = ""
    replaced = 0
    pattern = re.compile(rf"^(\s*{re.escape(key)}\s*=\s*)(true|false)(\s*)$", re.I)
    for index, raw in enumerate(lines):
        line = raw.rstrip("\r\n")
        ending = raw[len(line) :]
        section_match = re.match(r"^\s*\[([^]]+)]\s*$", line)
        if section_match:
            current = section_match.group(1)
            continue
        if current != section:
            continue
        match = pattern.match(line)
        if match:
            lines[index] = (
                match.group(1)
                + ("true" if value else "false")
                + match.group(3)
                + ending
            )
            replaced += 1
    if replaced != 1:
        raise ValueError(f"Expected one [{section}] {key}, replaced {replaced}")
    return "".join(lines)


def changed_lines(base: str, candidate: str) -> list[tuple[str, str]]:
    base_lines = base.splitlines()
    candidate_lines = candidate.splitlines()
    if len(base_lines) != len(candidate_lines):
        raise ValueError("Preset changed the configuration line count")
    return [
        (before, after)
        for before, after in zip(base_lines, candidate_lines, strict=True)
        if before != after
    ]


def option_file(key: str) -> str:
    return (
        "# Installed by the Artisans of Skyrim installer checkbox for this option.\n"
        "# Delete this file, or reinstall without the option, to turn it off.\n"
        "\n"
        "[features]\n"
        f"{key} = true\n"
    )


def main() -> None:
    template = TEMPLATE.read_text(encoding="utf-8")
    template_config = tomllib.loads(template)
    for _folder, (_file_name, key) in OPTIONS.items():
        assert template_config["features"][key] is False, (
            f"{key} must default to false; the installer option turns it on"
        )

    for name, (include_mod_added, delayed_orders) in PRESETS.items():
        generated = replace_section_boolean(
            template,
            "rules",
            "includeModAddedEquipment",
            include_mod_added,
        )
        generated = replace_section_boolean(
            generated,
            "orders",
            "enabled",
            delayed_orders,
        )

        parsed = tomllib.loads(generated)
        assert parsed["rules"]["includeModAddedEquipment"] is include_mod_added
        assert parsed["orders"]["enabled"] is delayed_orders
        assert parsed["forms"]["potemaFirstQuestEditorID"] == "MS06Start"
        for key, hours in EXPECTED_ORDER_HOURS.items():
            assert parsed["orders"]["smithing"][key] == hours
            assert hours % 24.0 == 0.0

        changes = changed_lines(template, generated)
        allowed_prefixes = {
            "includeModAddedEquipment =",
            "enabled =",
        }
        for before, after in changes:
            if not any(
                before.strip().startswith(prefix)
                and after.strip().startswith(prefix)
                for prefix in allowed_prefixes
            ):
                raise ValueError(
                    f"Preset {name} changed an unauthorized line: {before!r} -> {after!r}"
                )

        target = (
            OUTPUT_ROOT
            / name
            / "SKSE"
            / "Plugins"
            / "ArtisansOfSkyrim.toml"
        )
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(generated, encoding="utf-8", newline="\n")
        print(
            f"Generated {name}: "
            f"includeModAddedEquipment={str(include_mod_added).lower()}, "
            f"orders.enabled={str(delayed_orders).lower()}"
        )

    # Remove presets from the earlier eight-combination layout.
    for stale in OUTPUT_ROOT.iterdir():
        if stale.is_dir() and stale.name not in PRESETS:
            shutil.rmtree(stale)
            print(f"Removed obsolete preset {stale.name}")

    for folder, (file_name, key) in OPTIONS.items():
        target = OPTIONS_ROOT / folder / "SKSE" / "Plugins" / file_name
        target.parent.mkdir(parents=True, exist_ok=True)
        text = option_file(key)
        assert tomllib.loads(text) == {"features": {key: True}}
        target.write_text(text, encoding="utf-8", newline="\n")
        print(f"Generated option {folder}: {file_name}")

    if len(list(OUTPUT_ROOT.rglob("ArtisansOfSkyrim.toml"))) != len(PRESETS):
        raise ValueError("Installer must contain exactly four generated presets")
    print("Validated four FOMOD presets and three option files from one configuration")


if __name__ == "__main__":
    main()
