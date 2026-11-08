# Artisans of Skyrim 1.8

Version 1.8 redesigns the Service Menu, simplifies prices, adds an optional MCM,
and opens the mod to translations and compatibility patches without plugin
edits. Gameplay records, FormIDs and saved orders are unchanged.

## Interface

The Service Menu follows Skyrim's own menus: framed panels with Journal-style
corners, plain dividers, category tabs, column titles that sort when clicked, a
search field, and a filters list (F/Y/Triangle) with sort order and the Material
or Effect filter. List text is larger; titles are in capitals and values in
sentence case. Key prompts use the game's look: black key caps on keyboard,
black face buttons with white letters on Xbox, symbols on PlayStation.

Mouse: hovering selects a row and clicking the selected row uses it. The wheel
scrolls from anywhere on the menu and keeps priority over hover selection until
the mouse moves. The 3D preview sits in the preview area on every screen size and
rotates with a mouse drag or the right stick through the game's own handler.

The menu shows the same information as 1.7. Details are in
[Service Menu](SERVICE_MENU.md).

## Prices

| Service | Price |
|---|---|
| Crafting | fee for the item size + item value x 1.25 |
| Improvement | fee for the quality and item size + item value x 0.20 |
| Enchanting | price per effect and rank + Grand Soul cost per effect |
| Alchemy | price per batch, never below the safe resale minimum |

Final prices are rounded up to multiples of 5 after the Speech/Barter discount
(at most 20%). Alchemy compares the batch price with the potion's real value in
game, so potions changed by other mods cannot be resold at a profit.

## Settings

Settings are applied in this order; the last one wins:

1. The preset chosen in the installer (`ArtisansOfSkyrim.toml`).
2. The installer options (`ArtisansOfSkyrim_SkyforgeEquipment.toml`,
   `..._CustomEnchantments.toml`, `..._CustomPotions.toml`).
3. Patch files in `Data/SKSE/Plugins/ArtisansOfSkyrim/` (add only).
4. `Data/SKSE/Plugins/ArtisansOfSkyrim_Custom.toml` (personal settings).
5. The MCM, when MCM Helper is installed.

The MCM has pages for Smithing, Enchanting, Alchemy, the optional custom effects
and Mod-Added Artisans. Discount rules stay in the TOML files.

## Compatibility

- Patch files add enchantments, potions, artisan NPCs, item lists and item tiers
  (`[[items.tiers]]`, per item or per material keyword). See
  `README - Patches.md` in the archive. An item named in a patch by its form is
  offered even when the mod would exclude it (unique, conversion recipe,
  enchanted by default, default block); `block`, safety rules and lore gates
  still apply. With `price` (and `count` for arrows and bolts) a named item is
  offered even without a forge recipe. Patch enchantments and potions must give
  every field the menu shows.
- Blacksmiths and alchemists from other mods are detected by faction and use the
  default tier chosen in the MCM. The log lists them, and the forge items with an
  unknown material, as lines ready to copy into a patch.
- Mod forge recipes that convert one item into another, and mod recipes for
  unique items, are ignored. Improving unique items requires a Master.
- Every player-facing text, including the MCM, is in
  `Interface/Translations/ArtisansOfSkyrim_<LANGUAGE>.txt`. See
  `README - Translations.md`.
- Artisans with voice types from other mods get silent fallback dialogue lines
  and open the same services. The dialogue has 111 INFO records.

## Runtime compatibility

Unchanged from 1.7: one x64 DLL with exact SKSE runtime identifiers for Steam
1.5.97, 1.6.317, 1.6.318, 1.6.323, 1.6.342, 1.6.353, 1.6.629, 1.6.640, 1.6.1130,
1.6.1170, 1.7.99 and 1.7.104, and GOG 1.6.659 and 1.6.1179.
Use SKSE and Address Library matching the exact game version and store. MCM
Helper is optional; SkyUI is not required.

## Upgrade

Install the complete 1.8 package over 1.7 and choose the installer options
again. Existing orders, exclusive items and custom names keep working.
If an earlier 1.8 test build was installed with the MCM, delete
`Data/MCM/Settings/ArtisansOfSkyrim.ini` (in MO2 usually in `overwrite`) so the
MCM starts from the current defaults.

## Validation status

Completed offline: Release x64 build without warnings, six C++ test suites,
dialogue/alchemy/record validators, runtime metadata, SWF contract, generated
MCM, presets and translations, and package validation. In-game tests were run
on the author's installation during development; run the
[runtime test plan](RUNTIME_TEST_PLAN.md) on other runtimes before claiming them
as tested.
