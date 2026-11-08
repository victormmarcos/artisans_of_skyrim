#!/usr/bin/env python3
"""Generate the optional MCM Helper menu from the authoritative configuration.

Outputs:
  dist/Data/MCM/Config/ArtisansOfSkyrim/settings.ini   default values
  dist/Data/MCM/Config/ArtisansOfSkyrim/config.json    pages without custom content
  installer/mcm/<variant>/MCM/Config/.../config.json   adds Custom Enchantments
                                                       and/or Custom Potions pages

Every slider id encodes its TOML path, which the DLL reads back:
  "fpricing__crafting__craftMultiplier:Smithing" -> [pricing.crafting] craftMultiplier
  "ienchantmentPrices__fortify_health__master:Enchanting"
  "ialchemyPrices__potion_restore_health_adept:Alchemy"

Texts are written as translation keys ("$AOS_MCM_..."); the English texts
go to the translation file made by GenerateTranslations.py.

Run with --check to verify the committed files are current.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import math
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = ROOT / "config" / "ArtisansOfSkyrim.toml"
MOD_NAME = "ArtisansOfSkyrim"
SETTINGS_PATH = ROOT / "dist" / "Data" / "MCM" / "Config" / MOD_NAME / "settings.ini"
# variant: (Custom Enchantments page, Custom Potions page). "base" ships in Core.
MCM_VARIANTS = {
    "base": (False, False),
    "enchantments": (True, False),
    "potions": (False, True),
    "enchantments_potions": (True, True),
}
MCM_ROOT = ROOT / "installer" / "mcm"

TIERS = ("adept", "expert", "master")
# Discounts are capped at 20% by the DLL regardless of configuration.
DISCOUNT_CAP = 0.20

ENCHANT_PRICE_RANGE = (100, 20000, 100)
ALCHEMY_PRICE_MAX = 10000
ALCHEMY_PRICE_STEP = 50
GOLD_FLOOR_RANGE = (50.0, 10000.0, 50.0)

# Landing page image, relative to Data/Interface. x/y place it in the MCM panel.
LOGO = {"source": "ArtisansOfSkyrim/mcm_logo.dds", "x": 120, "y": 90}

# ---------------------------------------------------------------------------
# Every text shown in the MCM. Edit here, then run:
#     python tools/GenerateMcmConfig.py
# Item names (enchantments, potions) and their effect descriptions come from
# config/ArtisansOfSkyrim.toml (displayName / valueText).
# Placeholders in {braces} are filled in by the generator.
# ---------------------------------------------------------------------------
TEXT = {
    "modName": "Artisans of Skyrim",
    "pages": {
        "smithing": "Smithing",
        "enchanting": "Enchanting",
        "customEnchantments": "Custom Enchantments",
        "alchemy": "Alchemy",
        "customPotions": "Custom Potions",
        "modAdded": "Mod-Added Artisans",
    },
    # Section titles for the three artisan ranks.
    "tiers": {"adept": "Adept", "expert": "Expert", "master": "Master"},
    # Smithing item sizes, as used by the fees.
    "sizes": {
        "small": "Complexity - Small",
        "medium": "Complexity - Medium",
        "large": "Complexity - Large",
    },
    "sizeItems": {
        "small": "Daggers.",
        "medium": "One-handed weapons, bows, shields, light armor and armor pieces.",
        "large": "Two-handed weapons, crossbows and heavy body armor.",
    },
    "qualities": {
        "fine": "Fine",
        "superior": "Superior",
        "exquisite": "Exquisite",
        "flawless": "Flawless",
        "epic": "Epic",
        "legendary": "Legendary",
    },
    "smithing": {
        # Crafting price = fee for the item size + item value x multiplier.
        "craftingHeader": "Crafting Price",
        "multiplier": "Value Multiplier",
        "multiplierHelp": "Price = fee for the item size + item value times this.",
        "fee": "Fee: {size}",
        "feeHelp": "Fixed part of the price, added to the item value part. {items}",
        "ammunitionFee": "Fee: Ammunition",
        "ammunitionFeeHelp": "Arrows and bolts without a fixed batch price.",
        # Improvement price = the higher of (item value x percent) and the
        # minimum price for the quality and size (right column).
        # Improvement price = fee for the quality and size (right column)
        # + item value x multiplier.
        "valueHeader": "Improvement Price",
        "valueMultiplier": "Value Multiplier",
        "valueMultiplierHelp": "Price = fee for the quality and size + item value times this.",
        "targetsHeader": "Quality Targets",
        "targetHelp": "Damage or armor multiplier after improvement.",
        "minimumHeader": "Improvement Fee: {quality}",
        "minimumHelp": "Fixed part of the price to improve to {quality}. {items}",
        "modTierHeader": "Blacksmiths",
        "modTier": "Blacksmith Tier",
        "modTierHelp": "Tier for blacksmiths from other mods. Per-NPC tiers are listed in the log.",
    },
    "enchanting": {
        "soulHeader": "Grand Soul",
        "soul": "Grand Soul Cost",
        "soulHelp": "Added for every effect, including both effects of a dual enchantment.",
        "priceHelp": "{tier}: {value}. Before the Grand Soul cost.",
    },
    "alchemy": {
        "priceHelp": "{tier}: {value}. Batch of {size}, minimum {minimum}.",
        "modTierHeader": "Alchemists",
        "modTier": "Alchemist Tier",
        "modTierHelp": "Tier for alchemists from other mods. Per-NPC tiers are listed in the log.",
    },
}


# Display texts of the MCM documents.
TEXT_FIELDS = ("text", "help", "pageDisplayName", "displayName")


def mcm_key(english: str) -> str:
    words = re.sub(r"[^A-Za-z0-9]+", " ", english).title().replace(" ", "")[:32]
    digest = hashlib.sha1(english.encode("utf-8")).hexdigest()[:6]
    return f"$AOS_MCM_{words}_{digest}"


def collect_texts(document: object, found: dict[str, str]) -> None:
    if isinstance(document, dict):
        for key, value in document.items():
            if key in TEXT_FIELDS and isinstance(value, str) and value:
                found.setdefault(mcm_key(value), value)
            elif key == "options" and isinstance(value, list):
                for option in value:
                    found.setdefault(mcm_key(option), option)
            else:
                collect_texts(value, found)
    elif isinstance(document, list):
        for value in document:
            collect_texts(value, found)


def keyed(document: object) -> object:
    """Copy of an MCM document with its display texts replaced by keys."""
    if isinstance(document, dict):
        result = {}
        for key, value in document.items():
            if key in TEXT_FIELDS and isinstance(value, str) and value:
                result[key] = mcm_key(value)
            elif key == "options" and isinstance(value, list):
                result[key] = [mcm_key(option) for option in value]
            else:
                result[key] = keyed(value)
        return result
    if isinstance(document, list):
        return [keyed(value) for value in document]
    return document


def translations(config: dict) -> list[tuple[str, str]]:
    """(key, English text) for every MCM text."""
    found: dict[str, str] = {}
    for document in build_documents(config).values():
        collect_texts(document, found)
    return list(found.items())


def short_enchantment_name(name: str) -> str:
    # "Fortify Destruction & Magicka Regen" -> "Fortify Destruction + Regen"
    return name.replace(" & Magicka Regen", " + Regen")


def short_alchemy_name(name: str) -> str:
    return name.removeprefix("Potion of ").removeprefix("Poison of ")


@dataclass(frozen=True)
class Stepper:
    """Choice from a fixed list, stored as its index (MCM Helper "stepper")."""
    section: str
    path: tuple[str, ...]
    text: str
    help: str
    default: int
    options: tuple[str, ...]

    @property
    def key(self) -> str:
        return "i" + "__".join(self.path)

    @property
    def setting_id(self) -> str:
        return f"{self.key}:{self.section}"

    def validate(self) -> None:
        if not 0 <= self.default < len(self.options):
            raise ValueError(f"{self.setting_id}: default {self.default} is not an option")

    def json(self) -> dict[str, object]:
        return {
            "id": self.setting_id,
            "text": self.text,
            "type": "stepper",
            "help": self.help,
            "valueOptions": {"options": list(self.options), "sourceType": "ModSettingInt"},
        }

    def ini_value(self) -> str:
        return str(self.default)


def tier_stepper(section: str, path: tuple[str, ...], text: dict, current: str) -> Stepper:
    options = tuple(TEXT["tiers"][tier] for tier in TIERS)
    names = [tier.capitalize() for tier in TIERS]
    stepper = Stepper(
        section, path, text["modTier"], text["modTierHelp"],
        names.index(current.capitalize()) if current.capitalize() in names else 0,
        options,
    )
    stepper.validate()
    return stepper


@dataclass(frozen=True)
class Slider:
    section: str
    path: tuple[str, ...]
    text: str
    help: str
    default: float | int
    minimum: float | int
    maximum: float | int
    step: float | int
    format_string: str
    integer: bool = False

    @property
    def key(self) -> str:
        return ("i" if self.integer else "f") + "__".join(self.path)

    @property
    def setting_id(self) -> str:
        return f"{self.key}:{self.section}"

    def validate(self) -> None:
        if not self.minimum <= self.default <= self.maximum:
            raise ValueError(
                f"{self.setting_id}: default {self.default} is outside "
                f"{self.minimum}..{self.maximum}"
            )
        steps = (self.default - self.minimum) / self.step
        if abs(steps - round(steps)) > 1e-6:
            raise ValueError(
                f"{self.setting_id}: default {self.default} is not on the "
                f"{self.step} grid starting at {self.minimum}"
            )

    def json(self) -> dict[str, object]:
        return {
            "id": self.setting_id,
            "text": self.text,
            "type": "slider",
            "help": self.help,
            "valueOptions": {
                "min": self.minimum,
                "max": self.maximum,
                "step": self.step,
                "formatString": self.format_string,
                "sourceType": "ModSettingInt" if self.integer else "ModSettingFloat",
            },
        }

    def ini_value(self) -> str:
        if self.integer:
            return str(int(self.default))
        return f"{float(self.default):.2f}"


def header(text: str) -> dict[str, object]:
    return {"type": "header", "text": text}


EMPTY: dict[str, object] = {"type": "empty"}
# SkyUI pages hold 128 options; with "leftToRight" they fill both columns.
SKYUI_PAGE_LIMIT = 128


def two_columns(left: list[dict], right: list[dict]) -> list[dict]:
    """Interleave two independent columns for the leftToRight fill mode."""
    content: list[dict] = []
    for index in range(max(len(left), len(right))):
        content.append(left[index] if index < len(left) else EMPTY)
        content.append(right[index] if index < len(right) else EMPTY)
    return content


def split_section(title: str, rows: list[dict]) -> list[dict]:
    """Title over both columns, then the items split top-to-bottom per column."""
    if not rows:
        return []
    half = math.ceil(len(rows) / 2)
    return [header(title), header(title)] + two_columns(rows[:half], rows[half:])


def smithing_page(config: dict) -> tuple[list[dict], list[Slider]]:
    text = TEXT["smithing"]
    sizes, items, qualities = TEXT["sizes"], TEXT["sizeItems"], TEXT["qualities"]
    pricing = config["pricing"]
    crafting = pricing["crafting"]
    improvement = pricing["improvement"]
    fees = pricing["improvementFees"]
    targets = config["tempering"]["targets"]
    sliders: list[Slider] = []

    def add(column: list[dict], slider: Slider) -> None:
        slider.validate()
        sliders.append(slider)
        column.append(slider.json())

    left: list[dict] = [header(text["craftingHeader"])]
    add(left, Slider(
        "Smithing", ("pricing", "crafting", "craftMultiplier"),
        text["multiplier"], text["multiplierHelp"],
        float(crafting["craftMultiplier"]), 0.25, 5.0, 0.05, "{2}x",
    ))
    for size in ("small", "medium", "large"):
        add(left, Slider(
            "Smithing", ("pricing", "crafting", f"{size}ServiceFee"),
            text["fee"].format(size=sizes[size]),
            text["feeHelp"].format(items=items[size]),
            float(crafting[f"{size}ServiceFee"]), *GOLD_FLOOR_RANGE, "{0}",
        ))
    add(left, Slider(
        "Smithing", ("pricing", "crafting", "ammunitionServiceFee"),
        text["ammunitionFee"], text["ammunitionFeeHelp"],
        float(crafting["ammunitionServiceFee"]), 50.0, 2000.0, 50.0, "{0}",
    ))

    left.append(header(text["valueHeader"]))
    add(left, Slider(
        "Smithing", ("pricing", "improvement", "valueMultiplier"),
        text["valueMultiplier"], text["valueMultiplierHelp"],
        float(improvement["valueMultiplier"]), 0.0, 3.0, 0.05, "{2}x",
    ))

    left.append(header(text["targetsHeader"]))
    bands = {
        "fine": (1.10, 1.19, 0.01),
        "superior": (1.20, 1.29, 0.01),
        "exquisite": (1.30, 1.39, 0.01),
        "flawless": (1.40, 1.49, 0.01),
        "epic": (1.50, 1.59, 0.01),
        "legendary": (1.60, 3.00, 0.05),
    }
    for quality, (minimum, maximum, step) in bands.items():
        add(left, Slider(
            "Smithing", ("tempering", "targets", quality),
            qualities[quality], text["targetHelp"],
            float(targets[quality]), minimum, maximum, step, "{2}",
        ))

    right: list[dict] = []
    for quality in bands:
        right.append(header(text["minimumHeader"].format(quality=qualities[quality])))
        for size in ("small", "medium", "large"):
            key = f"{quality}{size.capitalize()}"
            add(right, Slider(
                "Smithing", ("pricing", "improvementFees", key),
                sizes[size],
                text["minimumHelp"].format(quality=qualities[quality], items=items[size]),
                float(fees[key]), *GOLD_FLOOR_RANGE, "{0}",
            ))
    return two_columns(left, right), sliders


def enchanting_pages(config: dict) -> tuple[list[dict], list[dict], list[Slider]]:
    text = TEXT["enchanting"]
    soul = Slider(
        "Enchanting", ("pricing", "grandSoulServiceCost"),
        text["soul"], text["soulHelp"],
        int(config["pricing"]["grandSoulServiceCost"]), 500, 5000, 50, "{0}",
        integer=True,
    )
    soul.validate()
    sliders = [soul]
    regular_rows: dict[str, list[dict]] = {tier: [] for tier in TIERS}
    custom_rows: dict[str, list[dict]] = {tier: [] for tier in TIERS}

    definitions = sorted(
        (item for item in config["enchantments"] if not item.get("blocked")),
        key=lambda item: item["displayName"],
    )
    for definition in definitions:
        for tier in TIERS:
            price = int(definition.get(f"{tier}Price", 0))
            if price <= 0:
                continue
            slider = Slider(
                "Enchanting",
                ("enchantmentPrices", definition["id"], tier),
                short_enchantment_name(definition["displayName"]),
                text["priceHelp"].format(
                    tier=TEXT["tiers"][tier],
                    value=definition.get(f"{tier}Value", ""),
                ),
                price, *ENCHANT_PRICE_RANGE, "{0}", integer=True,
            )
            slider.validate()
            sliders.append(slider)
            target = custom_rows if definition.get("exclusive") else regular_rows
            target[tier].append(slider.json())

    # Single slider: title on the left only, the right side stays empty.
    regular = [header(text["soulHeader"]), EMPTY, soul.json(), EMPTY]
    custom: list[dict] = []
    for tier in TIERS:
        regular += split_section(TEXT["tiers"][tier], regular_rows[tier])
        custom += split_section(TEXT["tiers"][tier], custom_rows[tier])
    return regular, custom, sliders


def alchemy_pages(config: dict) -> tuple[list[dict], list[dict], list[Slider]]:
    text = TEXT["alchemy"]
    max_discount = min(max(float(config["pricing"]["barter"]["maxDiscount"]), 0.0), DISCOUNT_CAP)
    regular_rows: dict[str, list[dict]] = {tier: [] for tier in TIERS}
    custom_rows: dict[str, list[dict]] = {tier: [] for tier in TIERS}
    sliders: list[Slider] = []

    items = sorted(
        config["alchemy"]["items"],
        key=lambda item: short_alchemy_name(item["displayName"]),
    )
    for item in items:
        tier = item["minimumTier"].lower()
        price = int(item["batchPrice"])
        size = int(item.get("batchSize", 5))
        safe = minimum_safe_batch_price(int(item["alchValue"]), size, max_discount)
        minimum = max(ALCHEMY_PRICE_STEP, math.ceil(safe / ALCHEMY_PRICE_STEP) * ALCHEMY_PRICE_STEP)
        slider = Slider(
            "Alchemy",
            ("alchemyPrices", item["id"]),
            short_alchemy_name(item["displayName"]),
            text["priceHelp"].format(
                tier=TEXT["tiers"][tier],
                value=item.get("valueText", ""),
                size=size,
                minimum=minimum,
            ),
            price, minimum, ALCHEMY_PRICE_MAX, ALCHEMY_PRICE_STEP, "{0}",
            integer=True,
        )
        slider.validate()
        sliders.append(slider)
        (custom_rows if item.get("exclusive") else regular_rows)[tier].append(slider.json())

    regular: list[dict] = []
    custom: list[dict] = []
    for tier in TIERS:
        regular += split_section(TEXT["tiers"][tier], regular_rows[tier])
        custom += split_section(TEXT["tiers"][tier], custom_rows[tier])
    return regular, custom, sliders


def mod_added_page(config: dict) -> tuple[list[dict], list[Stepper]]:
    """Default tiers for blacksmiths and alchemists added by other mods."""
    smith_text, alchemy_text = TEXT["smithing"], TEXT["alchemy"]
    smith = tier_stepper("Smithing", ("rules", "modAddedSmithTier"), smith_text,
                         config["rules"].get("modAddedSmithTier", "Adept"))
    alchemist = tier_stepper("Alchemy", ("alchemy", "rules", "modAddedAlchemistTier"), alchemy_text,
                             config["alchemy"]["rules"].get("modAddedAlchemistTier", "Adept"))
    left = [header(smith_text["modTierHeader"]), smith.json()]
    right = [header(alchemy_text["modTierHeader"]), alchemist.json()]
    return two_columns(left, right), [smith, alchemist]


def is_resale_safe(value: int, price: int, size: int, discount: float) -> bool:
    # Mirror of NPCAS::ApplyPriceDiscount: rounds up, then up to a multiple of 5.
    paid = math.ceil(max(price, 0) * (1.0 - min(max(discount, 0.0), DISCOUNT_CAP)))
    paid = -(-paid // 5) * 5 if paid > 0 else 0
    return max(value, 0) * max(size, 1) < paid


def minimum_safe_batch_price(value: int, size: int, discount: float) -> int:
    """Mirror of NPCAS::MinimumSafeBatchPrice."""
    resale = max(value, 0) * max(size, 1)
    price = max(1, int(resale / (1.0 - min(max(discount, 0.0), DISCOUNT_CAP))))
    while not is_resale_safe(value, price, size, discount):
        price += 1
    return price


def build(config: dict) -> tuple[str, dict[str, str]]:
    settings_ini, _ = build_parts(config)
    configs = {
        variant: json.dumps(keyed(document), indent=2, ensure_ascii=False) + "\n"
        for variant, document in build_documents(config).items()
    }
    return settings_ini, configs


def build_documents(config: dict) -> dict[str, dict]:
    return build_parts(config)[1]


def build_parts(config: dict) -> tuple[str, dict[str, dict]]:
    smithing, smithing_sliders = smithing_page(config)
    enchanting, enchanting_custom, enchanting_sliders = enchanting_pages(config)
    alchemy, alchemy_custom, alchemy_sliders = alchemy_pages(config)
    mod_added, mod_added_steppers = mod_added_page(config)
    sliders = smithing_sliders + enchanting_sliders + alchemy_sliders + mod_added_steppers

    ids = [slider.setting_id for slider in sliders]
    duplicates = {setting_id for setting_id in ids if ids.count(setting_id) > 1}
    if duplicates:
        raise ValueError(f"Duplicate MCM setting ids: {sorted(duplicates)}")

    sections: dict[str, list[Slider]] = {}
    for slider in sliders:
        sections.setdefault(slider.section, []).append(slider)
    ini_lines = [
        "; Default values for the Artisans of Skyrim MCM.",
        "; Generated by tools/GenerateMcmConfig.py from config/ArtisansOfSkyrim.toml.",
    ]
    for section, entries in sections.items():
        ini_lines.append("")
        ini_lines.append(f"[{section}]")
        ini_lines.extend(f"{entry.key} = {entry.ini_value()}" for entry in entries)
    settings_ini = "\n".join(ini_lines) + "\n"

    def page(name: str, content: list[dict]) -> dict[str, object]:
        if len(content) > SKYUI_PAGE_LIMIT:
            raise ValueError(
                f"MCM page {name} has {len(content)} options; SkyUI shows {SKYUI_PAGE_LIMIT}"
            )
        if len(content) % 2:
            raise ValueError(f"MCM page {name} does not fill whole rows")
        return {"pageDisplayName": name, "cursorFillMode": "leftToRight", "content": content}

    configs: dict[str, dict] = {}
    for variant, (with_enchantments, with_potions) in MCM_VARIANTS.items():
        names = TEXT["pages"]
        pages = [page(names["smithing"], smithing), page(names["enchanting"], enchanting)]
        if with_enchantments:
            pages.append(page(names["customEnchantments"], enchanting_custom))
        pages.append(page(names["alchemy"], alchemy))
        if with_potions:
            pages.append(page(names["customPotions"], alchemy_custom))
        pages.append(page(names["modAdded"], mod_added))
        document = {
            "modName": MOD_NAME,
            "displayName": TEXT["modName"],
            "minMcmVersion": 9,
            "pluginRequirements": [f"{MOD_NAME}.esp"],
            # Shown before a page is selected: the logo only, no text.
            "customContent": LOGO,
            "pages": pages,
        }
        configs[variant] = document
    return settings_ini, configs


def variant_path(variant: str) -> Path:
    if variant == "base":
        return SETTINGS_PATH.with_name("config.json")
    return MCM_ROOT / variant / "MCM" / "Config" / MOD_NAME / "config.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if outputs are stale")
    args = parser.parse_args()

    with CONFIG_PATH.open("rb") as handle:
        config = tomllib.load(handle)
    settings_ini, configs = build(config)
    outputs = {SETTINGS_PATH: settings_ini}
    outputs.update({variant_path(variant): text for variant, text in configs.items()})

    if args.check:
        stale = [
            str(path.relative_to(ROOT))
            for path, text in outputs.items()
            if not path.exists() or path.read_text(encoding="utf-8") != text
        ]
        if stale:
            print("Stale MCM files: " + ", ".join(stale), file=sys.stderr)
            return 1
        print("MCM files are current.")
        return 0

    for path, text in outputs.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
        print(f"Generated {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
