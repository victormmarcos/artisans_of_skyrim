#!/usr/bin/env python3
"""Generate the translation files of Artisans of Skyrim.

Every text the player sees is written in English in the sources. This script
collects those texts and writes Skyrim translation files (UTF-16 LE, one
"$KEY<TAB>Text" per line):

  dist/Data/Interface/Translations/ArtisansOfSkyrim_ENGLISH.txt
      English text for every key (copied to every Skyrim language, so nothing
      shows raw keys). Translators replace ArtisansOfSkyrim_<LANGUAGE>.txt with
      a copy of the English file and translate the text after the tab
      (keep the key before the tab and every {} unchanged). Missing keys
      stay in English.
  include/ArtisansOfSkyrim/InterfaceTexts.h
      Fixed menu texts (from T("...") in interface/*.as) the DLL sends to the menu.

Where texts come from:
  * DLL display texts: string literals in the service sources (see SOURCES).
    Keys are "$AOS_" + the English text; "{}" marks a number or a name.
  * Menu texts: T("...") in interface/*.as.
  * Names and effects from config/ArtisansOfSkyrim.toml.
  * MCM texts: tools/GenerateMcmConfig.py ("$AOS_MCM_..." keys).

Run with --check to verify the committed files are current.
"""

from __future__ import annotations

import argparse
import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import GenerateMcmConfig  # noqa: E402

PREFIX = "$AOS_"
CONFIG_PATH = ROOT / "config" / "ArtisansOfSkyrim.toml"
ENGLISH_OUT = ROOT / "dist" / "Data" / "Interface" / "Translations" / "ArtisansOfSkyrim_ENGLISH.txt"
# Skyrim game languages. Each gets the English text so the MCM never shows raw
# keys; a translation replaces the file of its language.
GAME_LANGUAGES = ("ENGLISH", "FRENCH", "GERMAN", "ITALIAN", "SPANISH", "POLISH", "RUSSIAN", "JAPANESE", "CHINESE")
HEADER_OUT = ROOT / "include" / "ArtisansOfSkyrim" / "InterfaceTexts.h"
# Sources whose string literals reach the player.
SOURCES = [
    "src/ServiceMenu.cpp",
    "smithing_src/Domain.cpp",
    "smithing_src/OrderDomain.cpp",
    "smithing_src/MenuController.cpp",
    "smithing_src/InventoryService.cpp",
    "smithing_src/OrderService.cpp",
    "smithing_src/SmithClassifier.cpp",
    "enchanting_src/Domain.cpp",
    "enchanting_src/MenuController.cpp",
    "enchanting_src/InventoryService.cpp",
    "enchanting_src/EnchanterClassifier.cpp",
    "alchemy_src/Domain.cpp",
    "alchemy_src/MenuController.cpp",
    "alchemy_src/InventoryService.cpp",
    "alchemy_src/AlchemistClassifier.cpp",
]

# Statements that never show text: logs, errors, form lookups, keyword lists...
SKIP_STATEMENT = re.compile(
    r"log::|throw |runtime_error|LookupForm|GetFormEditorID|HasKeyword|Keyword|"
    r"std::array<std::string|std::format\(\s*\"[a-z]|\.esm|\.esp|GetSingleton\(\)\.Is|PlaySound|"
    r"Dispatch\(|command ==|CreateString|SetMember|\"uiStage\"|layout ==|page\.layout|page\.key|row\.key"
)
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')

# Texts built in code that the literal scan cannot see.
EXTRA = [
    # Armor piece types: "Light " / "Heavy " + type.
    *(f"{weight} {piece}" for weight in ("Light", "Heavy") for piece in ("Helmet", "Boots", "Gauntlets", "Shield")),
    # FormatDurationHours.
    "ready now", "{} hour", "{} hours", "{} day", "{} days",
    "{} day, {} hour", "{} day, {} hours", "{} days, {} hour", "{} days, {} hours",
    # Enchantment values.
    "{} sec", "{} / {} sec",
    # Messages split into lines.
    "A blacksmith handling your orders has died.",
    "The following orders were permanently lost:",
    "No refund is available.",
    "Smithing Orders",
    "You have no orders with this blacksmith.",
    "Still Pending",
]

# Literals the scan picks up that are not display text.
IGNORE = {
    "Smithing Order Notice",
    # Name/keyword matching tables and comparisons.
    "nord hero", "ancient nord", "steel plate", "Dawnguard", "Fort Dawnguard", "Light", "Heavy",
    # Courier fallback table: NPC names and their forges (names come from the game).
    "Adrianne Avenicci", "Ulfberth War-Bear", "Rustleif", "Seren", "Ghorza gra-Bagol",
    "Tacitus Sallustius", "Balimund", "Asbjorn Fire-Tamer", "Beirand", "Heimvar",
    "Oengul War-Anvil", "Hermir Strong-Heart",
    # Log lines built with std::format.
    "Unique items (Master only):", "Improvement blocked by '{}': {}",
    "Improvement list for {} ({}): {} eligible, {} blocked",
}


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def is_display_text(text: str) -> bool:
    if not re.search(r"[A-Za-z]{2}", text) or text in IGNORE:
        return False
    if "\\" in text or "%" in text or "{:" in text or text.startswith("$") or "<" in text:
        return False
    if text.startswith("forge in ") and "{}" not in text:
        return False
    if " " not in text:
        # Single words: capitalized words (Iron, Ready); not identifiers.
        if not re.fullmatch(r"[A-Z][a-z]+(?:[-/][A-Za-z][a-z]+)*", text):
            return False
    return True


def code_texts() -> list[str]:
    found: list[str] = []
    for relative in SOURCES:
        source = strip_comments((ROOT / relative).read_text(encoding="utf-8"))
        for statement in re.split(r"[;{}]\s*\n", source):
            if SKIP_STATEMENT.search(statement):
                continue
            # Adjacent literals form one C++ string.
            joined = re.sub(r'"\s*\n\s*"', "", statement)
            for match in LITERAL.finditer(joined):
                value = match.group(1)
                for part in value.split("\\n"):
                    part = part.strip()
                    if is_display_text(part):
                        found.append(part)
    return found


def interface_texts() -> list[str]:
    found: list[str] = []
    for path in sorted((ROOT / "interface").glob("*.as")):
        found += re.findall(r'\b[TU]\("((?:[^"\\]|\\.)*)"\)', path.read_text(encoding="utf-8"))
    return found


def config_texts(config: dict) -> list[str]:
    found: list[str] = []
    for item in config.get("enchantments", []):
        if item.get("blocked"):
            continue
        name = item.get("displayName", "")
        found += [name, GenerateMcmConfig.short_enchantment_name(name), item.get("group", "")]
        found += [str(item[f"{tier}Value"]) for tier in GenerateMcmConfig.TIERS if f"{tier}Value" in item]
    for item in config.get("alchemy", {}).get("items", []):
        found += [item.get("displayName", ""), GenerateMcmConfig.short_alchemy_name(item.get("displayName", "")),
                  item.get("effect", ""), item.get("valueText", "")]
        found += list(item.get("groups", []))
    for npc in config.get("alchemy", {}).get("npcs", []):
        found.append(npc.get("lockedReason", ""))
    return [text for text in found if re.search(r"[A-Za-z]{2}", text)]


def unique(values) -> list[str]:
    seen: dict[str, None] = {}
    for value in values:
        if value and value not in seen:
            seen[value] = None
    return list(seen)


def translation_file(entries: list[tuple[str, str]]) -> bytes:
    lines = [f"{key}\t{value}" for key, value in entries]
    for line in lines:
        if len(line) >= 500:
            raise ValueError(f"Translation line too long for Skyrim (max 511): {line[:60]}...")
    return ("﻿" + "\r\n".join(lines) + "\r\n").encode("utf-16-le")


def header_file(texts: list[str]) -> str:
    def quote(text: str) -> str:
        return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'
    rows = "\n".join(f"        {quote(text)}," for text in texts)
    return (
        "// Artisans of Skyrim\n"
        "// Copyright (C) 2026 victormmarcos\n"
        "//\n"
        "// This program is free software: you can redistribute it and/or modify\n"
        "// it under the terms of the GNU General Public License as published by\n"
        "// the Free Software Foundation, either version 3 of the License, or\n"
        "// (at your option) any later version.\n"
        "//\n"
        "// This program is distributed in the hope that it will be useful,\n"
        "// but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
        "// GNU General Public License for more details.\n"
        "//\n"
        "// You should have received a copy of the GNU General Public License\n"
        "// along with this program. If not, see <https://www.gnu.org/licenses/>.\n\n"
        "#pragma once\n\n"
        "// Generated by tools/GenerateTranslations.py from T(\"...\") in interface/*.as.\n"
        "// The menu receives these texts translated (data.labels).\n\n"
        "#include <array>\n#include <string_view>\n\n"
        "namespace AOS::Text\n{\n"
        f"    inline constexpr std::array<std::string_view, {len(texts)}> kInterfaceTexts{{\n"
        f"{rows}\n"
        "    };\n}\n"
    )


def build() -> tuple[dict[Path, bytes], list[str]]:
    with CONFIG_PATH.open("rb") as handle:
        config = tomllib.load(handle)
    menu = unique(interface_texts())
    texts = unique(code_texts() + EXTRA + menu + config_texts(config))
    mcm = GenerateMcmConfig.translations(config)

    english = translation_file([(PREFIX + text, text) for text in texts] + mcm)
    outputs: dict[Path, bytes] = {
        ENGLISH_OUT.with_name(f"ArtisansOfSkyrim_{language}.txt"): english for language in GAME_LANGUAGES
    }
    outputs[HEADER_OUT] = header_file(menu).encode("utf-8")
    report = [f"{len(texts)} texts, {len(mcm)} MCM texts"]
    return outputs, report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if outputs are stale")
    parser.add_argument("--list", action="store_true", help="print the English texts")
    args = parser.parse_args()
    outputs, report = build()
    if args.list:
        english = outputs[ENGLISH_OUT].decode("utf-16-le").lstrip("﻿")
        print(english)
        return 0
    for line in report:
        print(line)
    if args.check:
        stale = [str(path.relative_to(ROOT)) for path, data in outputs.items()
                 if not path.exists() or path.read_bytes() != data]
        if stale:
            print("Stale translation files: " + ", ".join(stale), file=sys.stderr)
            return 1
        print("Translation files are current.")
        return 0
    for path, data in outputs.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print(f"Generated {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
