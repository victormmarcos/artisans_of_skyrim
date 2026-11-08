# Architecture

Artisans of Skyrim is one SKSE/CommonLibSSE-NG plugin with three service
modules. `src/Plugin.cpp` owns startup and lifecycle integration;
`src/PapyrusBridge.cpp` exposes the dialogue entry points; Smithing,
Enchanting, and Alchemy keep their domain logic in separate source trees.

## Runtime flow

1. SKSE loads the DLL and registers serialization, disenchant protection,
   Papyrus natives, and the messaging listener.
2. `DataLoaded` loads the translation file, reads the layered settings (see
   below), resolves game forms, builds the three catalogs, and starts the
   unified dialogue quest. Player inventory and
   equipment are not inspected at this stage.
3. `PostLoadGame` or `NewGame` activates player-dependent custom-effect hooks
   and reconciles persistent Smithing orders.
4. Dialogue exposes only supported, unlocked services. Its end fragment opens
   the Service Menu after the normal voiced response. The service
   validates the speaker, progression gates, inventory, price, and selected
   record again immediately before charging the player.

Player funds, carry weight, item weight, damage/armor and improvement projections
are captured on the game task queue before a page is handed to the UI queue.
`Publish` serializes that snapshot into Scaleform; it performs no player/inventory
queries or actor-statistic calculations. Preview and transaction guards remain.

All ordinary runtime diagnostics use the SKSE log at
`Documents/My Games/Skyrim Special Edition/SKSE/ArtisansOfSkyrim.log`.

## Settings

`src/UserSettings.cpp` builds one TOML table from, in order: the installer
preset, the installer option files, patch files in
`Data/SKSE/Plugins/ArtisansOfSkyrim/` (add-only, validated entry by entry),
`ArtisansOfSkyrim_Custom.toml`, and the MCM Helper values. Each service reads
that table. MCM changes are reported by `AOS_MCM.psc` through the
`ApplyMcmSetting` native; their ids encode the TOML path they change, and price
fields are reloaded in place without rebuilding the catalogs.

## Translation

`src/Localization.cpp` looks every player-facing text up in
`Interface/Translations/ArtisansOfSkyrim_<LANGUAGE>.txt` with the key
`$AOS_<English text>` just before it reaches the player; numbers match `{}`
placeholders. Missing keys keep the English text, and a translation that only
differs in letter case is ignored (Skyrim pools translated strings without
case). `tools/GenerateTranslations.py` writes the English file for every game
language, the MCM keys and `InterfaceTexts.h` (the movie's fixed texts).

## Service interface

`src/ServiceMenu.cpp` registers `ArtisansServiceMenu` at SKSE InputLoaded and
loads `Interface/ArtisansOfSkyrim/ServiceMenu.swf`. The module menu controllers
provide typed pages and rows. Game transactions use the existing inventory and
order services on the game task queue. Page revision tokens reject stale UI
commands, and a pending-command guard prevents duplicate payments.

The AS2 interface handles sorting, filtering, retained selection, mouse actions, and
confirmation presentation. It cannot set prices or create items; commands only
identify an existing native row. Close/load releases native model resources,
and Rename text-input registration is balanced across focus, page, and close.
Item instance extra-data pointers are checked against the current inventory
before native preview and statistics access. Existing service methods also
revalidate ownership, eligibility, price, and available gold before mutation.

`interface/build-manifest.json` binds the distributed SWF to its source
files (`ArtisansMenu.as`, `ArtisansControls.as`, `ArtisansDraw.as`,
`ServiceMovie.as`). `tools/CompileInterface.py` regenerates it with MTASC; runtime-asset
validation detects stale source/output combinations.

## Modules

- `smithing_src/` and `smithing_include/`: recipe discovery, equipment
  filtering, improvement, immediate delivery, persistent orders, shared
  workshops, successors, and courier notices.
- `enchanting_src/` and `enchanting_include/`: effect catalog, item
  eligibility, pricing, enchantment application, and confirmation menus.
- `alchemy_src/` and `include/NPCAlchemyServices/`: alchemist classification,
  fixed potion batches, pricing, and delivery.
- `src/ExclusiveEffects.cpp`: exact-hit weapon damage, event-driven apparel
  refresh, bounded Unburden sprint handling, and instance-level disenchant
  protection.
- `src/UserSettings.cpp`, `src/Localization.cpp`, `src/ServiceDiscount.cpp`:
  layered settings and MCM values, translation lookup, and the shared
  Speech/Barter discount.
- `include/ArtisansOfSkyrim/`: shared menu sorting, price rounding, transaction
  feedback, bridge declarations, and custom-effect interfaces.

## Sources of truth

| Data | Authoritative location | Generated outputs |
|---|---|---|
| Runtime settings | `config/ArtisansOfSkyrim.toml` | Four presets under `installer/presets/` and three option files under `installer/options/` |
| MCM | `config/ArtisansOfSkyrim.toml` and `tools/GenerateMcmConfig.py` | `dist/Data/MCM/Config/` and `installer/mcm/` |
| Player-facing texts | C++, AS2 and MCM sources | `dist/Data/Interface/Translations/` and `include/ArtisansOfSkyrim/InterfaceTexts.h` |
| Papyrus | `papyrus/Source/` | PEX files and synchronized PSC files under `dist/Data/Scripts/` |
| ESP records | `source-data/` plus generator definitions | `dist/Data/ArtisansOfSkyrim.esp`, manifest, resolved forms, and SEQ |
| Native runtime | C++ source and headers | `dist/Data/SKSE/Plugins/ArtisansOfSkyrim.dll` |
| FOMOD | `fomod/`, staged runtime, and presets | `Artisans of Skyrim.zip` |

Generated files are intentionally checked into the public source package so a
release can be inspected without rebuilding. Edit their authoritative inputs,
regenerate them, and run the validators instead of editing generated copies by
hand.

## Custom effects

The six Enchanting effects use private Magic Effect records, with private Perk
records only for the apparel effects derived from native structures. Weapon
effects are calculated directly after Skyrim populates the hit produced by the
exact charged item instance; no weapon perk or equipped-state modifier exists,
so the other hand cannot lend its effect. Apparel is refreshed only after load
or an armor equip event. Silver Blessing follows native undead/ghost keywords and
explicit werewolf/werebear race checks; it does not add classifications to actors.

The installer options Custom Skyforge Equipment, Custom Enchantments and
Custom Potions gate Eorlund's Skyforge substitutions and the visibility of the
custom effects. Form resolution, effect execution, and disenchant protection are
always initialized so items created under another preset remain functional.
The disenchant-list guard runs after Skyrim has populated the current crafting
menu. For instances containing a private Artisans marker, it clears only the
weapon/armor disenchant flags and preserves the master `listEntries` array's
size and order. This keeps Scaleform's parallel row data indexed correctly. A
refresh is requested only when eligibility changed, and a separate confirmation
hook rejects a protected instance if another menu plugin exposes one.

The three Over Time potion families use complete duration-restoration effect
records with direct Health, Magicka, or Stamina actor values. Each is a Peak
Value Modifier associated with a private family keyword, following Skyrim's
Fortify-potion exclusivity model. Equal doses do not add and the strongest
active tier prevails; the three families remain independent.
All nine service potions last 10 seconds. Their manual values are validated so
reselling a five-flask batch is strictly worth less than the minimum discounted
service price.

## Generated plugin stability

The original Alchemy FormIDs and the 1,024 courier BOOK range are reserved for
save compatibility. New support records begin after that range. The generator
and validators enforce record counts, FormID ranges, masters, dialogue links,
Skyforge statistics, and installer/configuration consistency.
