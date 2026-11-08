# Changelog

## 1.8

### License and repository (2026-11-07)
- The mod is GPL-3.0 (required by CommonLibSSE-NG). The download now includes
  `LICENSE.txt` and the source files carry the license notice. Game files are
  unchanged.
- Fixed `tools/GenerateArtisansOfSkyrimEsp.py`: a header comment in
  `config/ArtisansOfSkyrim.toml` that mentions `[[alchemy.items]]` was read as
  an item block and erased most of the configuration on regeneration.
- Repository cleanup, no change to game files: the version is set only in
  `CMakeLists.txt`; build, release gate and packaging docs merged into
  `docs/BUILD.md`; release notes in `docs/RELEASE_NOTES.md`; menu notes in
  `docs/SERVICE_MENU.md`; source-archive scripts removed (the repository is
  the source); `ValidateV15Records.py` renamed `ValidateGameplayRecords.py`
  and `SanitizeReleaseBinary.py` renamed `CheckReleaseBinary.py`; the release
  validator no longer checks exact description and log wording.

### Service Menu
- Redesigned in the style of Skyrim's own menus: framed panels with Journal-style
  corners, plain dividers, larger list text, titles in capitals and values in
  sentence case.
- Category tabs replace the filter mode (Left/Right, D-pad or click). A search
  field filters by name. Clicking a column title sorts by it; clicking again
  reverses the order. F/Y/Triangle opens a filters list with sort order and the
  Material or Effect filter.
- Mouse: hovering selects a row and clicking the selected row uses it. The wheel
  scrolls anywhere on the menu and wins over hover selection until the mouse moves.
- Key icons follow the game: black key caps, black face buttons with white
  letters, PlayStation symbols, and shoulder buttons. Footer prompts start at the
  left edge and are aligned with their labels.
- The 3D preview is placed in the preview area on every screen size, including
  ultrawide. Hold the mouse button over the item and drag to rotate it, or use the
  right stick; both use the game's own inventory rotation.
- Improve lists one row per item (duplicate rows such as Bloodskal Blade are gone)
  and arrows show their correct prices. "Gauntlets/Bracers" is now "Gauntlets".

### Prices and settings
- Simpler prices. Crafting: fee for the item size + item value x 1.25.
  Improvement: fee for the quality and item size + item value x 0.20. Final prices
  are rounded up to multiples of 5.
- Alchemy resale protection uses the potion's real value in game, so potions
  changed by other mods can never be resold at a profit.
- Optional MCM (MCM Helper): crafting and improvement fees and multipliers,
  quality targets, Grand Soul cost, a price per enchantment and rank, a price per
  potion (never below the safe minimum), and the default tier of mod-added
  blacksmiths and alchemists. Pages for custom effects appear only when installed.
- Personal settings in `ArtisansOfSkyrim_Custom.toml`, which mod updates never
  overwrite. Item lists in `[items]` add to the mod's own lists.
- `[services]` switches turn Smithing, Enchanting or Alchemy off.

### Compatibility
- Patch files: `Data/SKSE/Plugins/ArtisansOfSkyrim/*.toml` add enchantments,
  potions, artisan NPCs, item lists and item tiers. Each entry is checked; a
  wrong entry is skipped with a log line. See `README - Patches.md`.
- `[[items.tiers]]` sets the blacksmith tier for an item or for every item with a
  material keyword, for equipment the mod does not recognize. The log lists the
  forge items left out because of an unknown material, as lines ready to copy.
- An item named in a patch (`[[items.tiers]]` with `form`) skips the mod's own
  exclusions: unique items, conversion recipes, unknown materials, items
  enchanted by default and the mod's default blocks (`[items] defaultBlock`).
  `block` always wins; safety rules and lore gates stay. A named item from
  another mod is offered even with "Skyrim and Official DLC Only", which now
  only limits what is found automatically.
- A named item can also set `price` (and `count` for arrows and bolts): items
  with no forge recipe or worth 0 gold are then offered directly. The price never
  goes below the resale value. Jewelry, circlets and staves are never offered.
- Patch enchantments now require `group` and, for every priced rank, its
  magnitude and value text; patch potions require `itemType`, `effect` and
  `valueText`. Incomplete entries are skipped with the missing field in the log.
- The log reports named items that still can't be offered, and why. Improvement
  summaries are written once per change, with the names of unique items.
- Artisans' own blacksmiths and alchemists can't be changed by patches: an entry
  for one of them is ignored however its FormID is written (the first entry for
  an NPC wins, and the mod's own list is read first). Workshops and successors
  are unchanged.
- Nord Hero Arrows are recognized (Eorlund, like the Nord Hero weapons).
- The log is shorter: per-click menu diagnostics moved to debug level; each page
  shown is one line.
- Blacksmiths and alchemists from other mods (detected by faction) use a
  configurable default tier; the log lists each one as a line ready to copy.
- Translations: every menu, message, courier and MCM text is in
  `Interface/Translations/ArtisansOfSkyrim_<LANGUAGE>.txt`. See
  `README - Translations.md`.
- Service dialogue no longer depends on vanilla voice types: artisans with voice
  types from other mods get silent fallback lines and open the same services.

### Smithing
- Forge recipes added by other mods that convert one item into another are
  ignored, and so are mod recipes for unique items. Improving unique items
  (vanilla or from mods) requires a Master.
- Two-handed weapons without a recognized weapon type keyword are classified as
  two-handed weapons instead of being left out.
- The Wolf Helmet joins the Wolf armor set.

### Enchanting
- Sergius also serves the Arch-Mage.

### Installer
- The Custom Effects choice is split into three checkboxes: Custom Skyforge
  Equipment, Custom Enchantments and Custom Potions. Four presets plus three
  option files replace the eight presets.
- Two new guides at the root of the archive: `README - Translations.md` and
  `README - Patches.md`.

## 1.7

- Replaced the service-list AS2 sort callback with field sorting and deferred model
  requests until the next frame; added UI phase checkpoints for the reported
  Eorlund opening crash. In-engine confirmation remains pending.
- Fixed actual zero-magnitude Fear/Frenzy poisons (level 30, existing 60 seconds)
  and Slow poisons (50%, existing durations). Added fail-closed magnitude checks.
- Alchemy now places Price beside the title, matching Enchanting, with full-width
  Effect text underneath.

- Simplified service dialogue: questions exist only when the artisan is unlocked.
  Removed the experimental silent INFOs, begin fragments, and speaker override.
- Moved inventory/statistic reads out of Scaleform publication into the game task
  queue; the UI consumes a prepared snapshot with the same formulas.
- Standardized circular input indicators across the footer, service actions,
  purchase confirmation, and Rename; shoulder controls use separate circles.
- Restored mouse clicks and deferred wheel navigation; input mode controls cursor visibility.
- Standardized labels, compact left-side prompts, and right-side Gold/Carry Weight.
- Added two-line effect details, Alchemy batch names, Result Quality, and visible notices.
- Removed unused presentation fields, filter-cycle helpers, and manual-preview interaction.
- Consolidated build documentation and retained existing gameplay/configuration data.

- Reworked Master enchanting into one persistent effect list: every effect now
  shows its own description and price, one or two effects may be selected, and
  selected effects remain visible and marked in the list.
- Added an itemized enchanting confirmation with both effect names, both
  individual prices, and an exact additive total; Master enchanting no longer
  forces the player to buy a second effect.
- Added a dedicated Craft command for enchanting (Square on PlayStation, X on
  Xbox, or C on keyboard) while the normal confirm button selects effects.
- Added Rename to the enchanting confirmation (Square on PlayStation, X on
  Xbox, or R on keyboard) using Skyrim's vanilla custom inventory-name record.

- Added a standalone item browser for all three services and workshop collection.
- Added artisan rank, modal filters, service-specific sorting, item model and stat integration, and gold confirmation.
- Consolidated SKSE lifecycle handling into one messaging listener so the menu
  registration cannot prevent Smithing, Enchanting, or Alchemy initialization.
- Fixed GOG store-subtype runtime matching in both metadata and query/load guards.
- Generated the ESP with header 1.70 while preserving existing FormIDs.
- Shortened FOMOD labels, organized option descriptions and added the Nexus Website.
- Added compiled-runtime and SWF artifact validation.
- Refined the service menu with vanilla-style Gold and Carry Weight readouts,
  left-aligned input hints and right-aligned values, larger left-aligned item details, compact purchase
  confirmations, direct workshop collection, and balanced top ornamentation.
- Added order delivery estimates to Smithing item details and replaced compact
  potency labels with the actual inventory descriptions for Alchemy and
  Enchanting effects, including both descriptions for Master dual enchanting.
- Restored Sybille Stentor to Expert after the dual-enchantment test pass;
  her existing quest requirement is unchanged.
- Moved Enchanting effect Price to the upper right beside the title, retaining
  full-width effect text below.
- Master enchanters allow one or two selections in the same persistent list;
  the second effect is optional and Back removes the most recent selection.
- Normalized Skyrim's negative ammunition weight sentinel to the vanilla value
  of zero in item details and immediate-delivery confirmations.


## 1.6.0

- Corrected Sybille Stentor's Potema prerequisite to use `MS06Start`.
- Added item-type labels to both immediate and timed Smithing confirmations;
  armor labels retain Light/Heavy weight classes.
- Preserved the complete `HitData::Populate` ABI, including its left-hand flag,
  through the exact-hit weapon detour.
- Replaced removed legacy message/HUD calls with current CommonLibSSE-NG APIs.
- Replaced fragile Book Menu call-site scanning with a MinHook entry detour;
  normal books still chain through the original function.
- Rebuilt against CommonLibSSE-NG 7.2.0 with explicit known SE/AE runtime
  records and a clean x64 release package.

## 1.5.0 Hotfix

- Corrected Sybille Stentor's Potema prerequisite to use `MS06Start` (The Man Who Cried Wolf) instead of `MS06` (The Wolf Queen Awakened).
- Crafting confirmation now shows `Item type` in both Immediate Crafting and Timed Orders.
- Armor confirmation labels now preserve weight class across all supported armor pieces: Light/Heavy Body Armor, Helmet, Boots, Gauntlets/Bracers, and Shield.
- Timed Orders still show crafting/queue timing; Immediate Crafting shows the item type without timing.
- Kept the serialized crafting-order category enum unchanged for save compatibility.

## 1.5.0

- Added six exclusive Enchanting services: Silver Blessing, Elemental Burst,
  Berserker Soul, Honed Metal, Unburden, and Reinforcement.
- Rebuilt every new Enchanting mechanic as an Artisans-owned effect cloned from
  a native structure, giving the mod full control without modifying vanilla
  effects.
- Added three custom Health, Magicka, and Stamina Over Time effects, each
  offered at all three tiers. All use Restore, last 10 seconds, and restore
  10/20/40 points per second.
- Built Over Time restoration on Skyrim's duration-based food structure as
  direct actor-value modifiers rather than regeneration-rate bonuses. Each
  family uses a private Peak Value Modifier keyword, matching vanilla Fortify
  potion exclusivity: equal doses do not add and the strongest active tier
  prevails. Health, Magicka, and Stamina remain independent.
- Made every exclusive Enchanting marker visible in the inventory with an
  explicit name and description, restoring normal effect text and weapon charge
  presentation.
- Added the Utility Enchanting group for Carry Weight, Waterbreathing,
  Unburden, and Reinforcement while retaining Muffle under Stealth.
- Limited Elemental Burst to Fire, Frost, and Shock spell damage.
- Implemented Unburden as weightless worn armor, removal of armor movement
  penalties, and a 15%/30% reduction to sprint and power-attack Stamina costs.
- Consolidated cloned Conditioning, Unhindered, and power-attack entries into a
  unique Unburden perk and removed the duplicate manual inventory-weight offset.
- Added Expert/Master scaling to every effect: Silver Blessing 25%/50%,
  Berserker Soul 20%/40%, Honed Metal 15%/30%, Reinforcement 20%/40%, and
  Elemental Burst 25%/50%.
- Replaced temporary weapon perks with direct final-hit calculations on the
  exact charged item instance passed to each `HitData::Populate` call. Honed
  Metal and valid Silver Blessing hits multiply that hit's damage; Berserker
  Soul recalculates the portion protected by the target's current armor using
  Skyrim's armor settings. Dual-wield attacks cannot inherit the other hand's
  effect, and no weapon bonus remains after the hit.
- Installs native detours during SKSE plugin load so they can chain before later
  combat plugins. The equipment event sink and first equipped-item refresh stay
  deferred until `PostLoadGame`/`NewGame`. `DataLoaded` performs only
  player-independent catalog/form setup, preventing a startup CTD before the
  main menu when no playable character exists yet.
- Made Silver Blessing affect actors identified by Skyrim's native undead or
  ghost keywords, plus werewolf and werebear races, while excluding Dremora and
  actors without those classifications.
- Made Elemental Burst additive across a ring and necklace, with supported
  combined strengths of 50%, 75%, and 100%.
- Added instance-level disenchant protection for every custom enchantment in
  every preset. The feature flag controls custom services and Eorlund's custom
  Skyforge equipment/stats; existing exclusive items keep runtime support.
- Synchronized disenchant protection with Skyrim's initial enchanting-menu
  update. Protected instances are filtered after the native list is populated
  by clearing only their disenchant filter flags. Their positions in Skyrim's
  master entry array remain intact, preventing item names and row metadata from
  becoming misaligned. The interface is redrawn only when eligibility changes,
  and the final confirmation guard remains as a second line of protection.
- Standardized NPC-created weapon enchantments on a full Grand Soul charge of
  3,000 and added per-effect charge costs for the approved hit targets.
- Corrected exclusive single-enchantment pricing to final totals of 3,000 gold
  at Expert and 6,000 at Master, including the 1,000-gold Grand Soul service.
- Added Eorlund Skyforge substitutions for Steel Dagger, Sword, War Axe,
  Greatsword, Battleaxe, Mace, Warhammer, Steel Shield, and the full Steel Plate
  set. All 12 results now use Artisans-owned forms and tempering recipes, and
  failed replacements safely fall back to the original Steel item. Vanilla
  Skyforge forms are no longer overridden.
- Applied the approved values, weights, damage, and armor ratings to every
  Skyforge substitute. The four body pieces retain Steel Plate Matching Set
  eligibility; the shield correctly remains Steel.
- Added unchanged vanilla Wolf Armor, Wolf Boots, and Wolf Gauntlets to an
  Eorlund-only `Wolf` category after `Proving Honor`.
- Added compact, non-truncated Skyforge button labels while retaining full item
  names in confirmations and inventory.
- Added distinct compact Gunmar menu labels for every Dawnguard light/heavy and
  color variant while retaining full confirmation names.
- Preserved full purchase-menu names for the selected official Steel,
  Bonemold, Improved Bonemold, and Nordic Carved records while keeping the
  normal truncation limit for other equipment, including mod-added items.
- Moved Steel Imperial Gauntlets to the Steel material group and preserved its
  full label. Potion of Fortify Destruction and Potion of Fortify Carry Weight
  also keep their full purchase-menu names.
- Kept Dragonborn's Chitin Heavy Gauntlets in the Chitin purchase group despite
  their auxiliary vanilla `PerkFistsSteelPlate` keyword.
- Removed borrowed ExtraDataList ownership from Smithing and Enchanting item
  checks, preventing inventory corruption and Eorlund improvement crashes.
- Excluded rings, necklaces, circlets, and staves from blacksmith improvement.
- Serialized custom MGEFs before ALCH records and retained runtime ALCH/MGEF
  validation plus delivery logging before payment and inventory insertion.
- Restored Sybille Stentor to Expert after exclusive-effect testing.
- Added Florentius Baenius as a Master Alchemist after completion of
  `Bolstering the Ranks`.
- Added Skyrim's vanilla coin-spend sound after every successful Smithing,
  Enchanting, and Alchemy purchase. Cancelled or failed transactions stay
  silent.
- Removed weapon temporary perks and periodic weapon inventory polling. Custom
  weapon effects are selected and calculated directly per hit and are skipped
  at zero charge, preventing load multiplication and migration between weapons.
- Bounded Unburden's sprint refund to Skyrim's expected vanilla sprint drain, so
  simultaneous external Stamina damage can never be refunded as sprint cost.
- Reduced all nine Over Time durations to 10 seconds and set their manual ALCH
  values to 30/60/120. A strict validator now audits every one of the 63 ALCH
  records against the maximum 20% service discount.
- Added EditorID fallback resolution and explicit runtime logging to Eorlund's
  immediate-delivery substitutions so a valid Skyforge form cannot silently
  degrade to ordinary Steel because of a local-form lookup mismatch.
- Added Imperial Sword, Imperial Bow, Hunting Bow, and Long Bow to the Adept
  Smithing catalog. Long Bow is forced into the Iron menu group.
- Added a Custom Effects FOMOD choice covering the six enchantments, three Over
  Time effects, and Eorlund's custom Skyforge equipment; expanded generated
  presets from four to eight.
- Added Dawnguard and Dragonborn as authorized masters for the unified plugin.
- Expanded the generated plugin from 54 to 63 ALCH records while preserving all
  historical Alchemy FormIDs and all 1,024 courier BOOK FormIDs.
- Added deterministic v1.5 record validation and updated both source workbooks.
- Updated release metadata and packaging to 1.5.0.
- Rebuilt every DLL translation unit after extending the Smithing and Alchemy
  configuration records, preventing a `DataLoaded` return crash caused by stale
  incremental objects using the previous in-memory layout.

## Version 1.4.0

- Added reviewed vanilla-voiced initial responses for Smithing, Enchanting, and
  Alchemy without redistributing voice assets.
- Corrected Eorlund's SharedInfo mapping so audio and subtitle both use
  `Gods be praised.`.
- Kept Feran Sadri's Enchanting response neutral and excluded Alchemy-specific
  commerce wording.
- Added FOMOD choices for official equipment only or eligible mod-added
  equipment, and for immediate crafting or timed orders.
- Added persistent per-smith FIFO orders with five active slots, save/load
  support, smith-death handling, and voiced collection dialogue.
- Rebalanced delayed Smithing to whole days: one day for daggers; two days for
  one-handed weapons, boots, gauntlets, bows, shields, helmets, and light body
  armor; three days for two-handed weapons, heavy body armor, and crossbows.
- Fixed missing Leather, Dragonplate, Dragonscale, and similarly configured
  modded helmets by recognizing `ArmorHelmet` and guarded Hair/Head/Circlet slot
  layouts.
- Added explicit circlet/jewelry blocking so the helmet fallback cannot expose
  actual circlets in immediate or timed crafting.
- Added brief courier notices through Skyrim's vanilla courier quest. One
  notice is sent when the artisan's current queue finishes, stating that the
  orders are ready and giving the pickup location and artisan name.
- Corrected courier dispatch to update the vanilla delivery container and its
  pending-item global synchronously. Startup also reconciles those values so a
  queued VM call can no longer be logged as delivered before it actually runs.
- Kept courier letters informational; receiving or reading one is never required
  to collect ready work.
- Changed courier-letter signatures to the artisan's plain name without a
  preceding Unicode dash, avoiding unsupported-font placeholder squares. Saved
  notices from earlier 1.4 builds are normalized when loaded.
- Added 1,024 unique persistent BOOK records so previously delivered letters
  keep their own titles and contents across saves. Pool exhaustion remains
  informational and never prevents manual collection.
- Shortened courier letters to a stable readiness notice with artisan and pickup
  location; itemized contents are no longer embedded in letters. Letters remain
  in the player's inventory after reading and collection.
- Made immediate crafting structurally independent from the order runtime: no
  courier initialization, Book Menu hook, temporal events, queues, collection
  availability, or order/courier save records are active in that profile.
- Limited persistent order data to Pending and Ready work. Successfully
  collected, invalid, and death-lost records are removed instead of accumulating.
- Changed smith-death warnings to one itemized message with quantities, followed
  by permanent cleanup of the affected records.
- Added silent cleanup for serialized orders whose item or essential source form
  can no longer be restored after a plugin is removed.
- Corrected singular and plural time text through one shared formatter, including
  `ready now` for zero remaining time.
- Made `papyrus/Source` authoritative, synchronized all six distributed PSC
  files during CMake configuration, and added source-package equality checks.
- Corrected book-menu integration so courier letters and ordinary books use the
  game's normal opening path without destabilizing the menu.
- Allowed vanilla enchantments with multiple internal effects, including Frost
  Damage, to count as one enchantment; Master dual enchanting still requires two
  separately selected enchantments.
- Standardized all public Alchemy item names to Skyrim's `Potion of ...` and
  `Poison of ...` convention without changing their EditorIDs or FormIDs.
- Kept recognized-material improvement fallbacks such as Silver equipment even
  without a vanilla tempering recipe. Craftable equipment remains improvable;
  unique items and artifacts still require a Master and are not made craftable.
- Simplified Enchanting so effect selection proceeds directly to confirmation
  while existing custom item names remain unchanged.
- Added shared queues for Adrianne/Ulfberth and Rustleif/Seren, plus fresh Adept
  successor services for Tacitus, Asbjorn, Heimvar, and Hermir after their
  respective masters die.
- Applied the same workshop availability rules to immediate and delayed profiles.
- Changed improvement and collection acknowledgements to neutral vanilla lines,
  with Gunmar retaining sex-aware `Aye lad.` and `Aye lass.` responses.
- Added four generated presets, updated FOMOD copy, and structural validation of
  the courier BOOK pool.
- Updated all release metadata to 1.4.0 and combined the clean source with the
  versionless `Artisans of Skyrim.zip` installer.

## Version 1.3.0

- Corrected all generated potion and poison records to preserve their manually
  balanced values and prevent an unintended resale exploit.
