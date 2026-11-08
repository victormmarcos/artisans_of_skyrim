# QA Notes

## Unification

- Only `ArtisansOfSkyrim.esp` is active as the final plugin.
- Only `ArtisansOfSkyrim.dll` is loaded as the SKSE DLL.
- `ArtisansOfSkyrim.toml` is the main runtime config; installer option files,
  patch files, `ArtisansOfSkyrim_Custom.toml` and the MCM are layered on top.
- The old standalone dialogue ESPs are not masters.
- `AOS_DialogueQuest` starts on new game and loaded saves.
- `ArtisansOfSkyrim.seq` exists and is current.

## Vanilla-voiced initial responses

- All 111 INFOs (106 voiced, 5 silent fallbacks) pass
  `tools/ValidateArtisansOfSkyrimDialogue.py`.
- Voiced INFOs share reviewed vanilla responses through DNAM; every INFO uses
  an end fragment.
- Actor-specific entries precede faction/VoiceType fallbacks; the silent
  fallbacks come last, for voice types without a vanilla line.
- Locked artisans have no service dialogue; unlocked artisans have the normal
  response and menu. Backend validation still rejects stale or direct requests.
- Check Eorlund's complete catalog with both mouse and controller. The log should
  reach data prepared, values serialized and published successfully in that order.
- Feran Sadri uses `Take a look.` for Enchanting; no alchemy-specific line is
  assigned to him.
- The staged runtime tree contains no redistributed voice or lip-sync files.
- The unchanged SEQ still names the same dialogue quest.
- The release DLL was rebuilt from a clean dependency tree, contains no personal
  workspace path, and passes all six C++ domain/menu/rules tests.
- Changes to shared configuration record layouts require a clean rebuild so
  `Plugin.cpp` and every service translation unit use the same in-memory layout.

## Smithing

- A normal valid blacksmith offers Adept. Blacksmiths added by other mods use
  the configured default tier (MCM: Mod-Added Artisans) and are logged once.
- Balimund, Moth gro-Bagol, Oengul War-Anvil, Gharol, and Glover Mallory offer Expert.
- Eorlund Gray-Mane, Gunmar, and Baldor Iron-Shaper offer Master when lore locks allow.
- Crafting and Improvement remain separate flows.
- Crafting = fee for the item size + item value x 1.25; Improvement = fee for
  the quality and item size + item value x 0.20. Final prices are rounded up
  to multiples of 5.
- Discount never exceeds 20%.
- Service pricing does not create easy resale profit.
- Mod forge recipes that convert an item into another, and mod recipes for
  unique items, are ignored. Improving a unique item requires a Master.
- `[[items.tiers]]` gives equipment with an unknown material a tier; the log
  lists the forge items left out because of an unknown material.
- An item named by form (`[[items.tiers]]` or `allow`) skips the equipment-origin
  preset (official only) and the unique, conversion,
  enchanted-by-default, Ancient Nord and `defaultBlock` exclusions; `block`,
  value/playable/category/quest-item rules and lore gates still apply. Named
  enchanted items are logged.
- A named item with `price` is offered without a forge recipe or with value 0;
  arrows and bolts also need `count`. The price is never below the resale value.
  Named items that still can't be offered are logged with the reason.
- Improve shows one row per item; arrows show their real price.
- The ten approved official equipment records keep their full purchase-menu
  labels; Steel Imperial Gauntlets are grouped under Steel. Other long names,
  including mod-added equipment, remain compact.
- Chitin Light and Heavy use their exact Dragonborn material keywords before
  text fallbacks, so the heavy gauntlets' `PerkFistsSteelPlate` keyword cannot
  move them into the Steel Plate purchase group.

## Enchanting

- NPCs outside the configured list do not offer service.
- Farengar and Calcelmo offer Adept.
- Sergius Turrianus offers Expert to College members, including the Arch-Mage;
  Sybille Stentor offers Expert after her existing quest gate.
- Neloth and Feran Sadri offer Master.
- Enchantment effect selection buttons use compact labels without magnitude/duration text.
- Final confirmation shows selected item, effect details, and final price.
- Final confirmation offers Rename and stores an accepted custom name through
  Skyrim's vanilla `ExtraTextDisplayData` inventory record.
- Confirming service charges once.
- Item receives the selected enchantment(s).
- A configured vanilla enchantment may contain multiple internal effects and
  still counts as one selection; Frost Damage is the regression case.
- Utility contains Carry Weight, Waterbreathing, Unburden, and Reinforcement;
  Muffle remains Stealth.
- Exclusive instances cannot be disenchanted, with or without the Custom
  Enchantments option. They are absent on the first visible disenchant-list update without
  requiring a category change. The native master entry order is preserved, so
  every remaining row must retain its own name, effect, type, weight, and value
  before and after category changes. Ordinary vanilla-only enchanted items
  remain governed by Skyrim's normal learned-effect rules.
- All NPC-created weapon enchantments begin with a full 3,000 Grand Soul charge.
- Exclusive single-enchantment confirmations total exactly 3,000/6,000 gold at
  Expert/Master, including the Grand Soul service.
- Custom weapon calculations use the exact attacking instance, skip a depleted
  weapon, and cannot migrate from the other hand.

## Shared workshops and successors

- Adrianne/Ulfberth and Rustleif/Seren each use one shared five-slot capacity.
- Adrianne's death disables every Ulfberth service; Ulfberth's death does not
  cancel Adrianne's workshop.
- Either surviving member of Rustleif/Seren retains the shared queue.
- Tacitus, Asbjorn, Heimvar, and Hermir appear only after Ghorza, Balimund,
  Beirand, and Oengul respectively die; each begins a fresh Adept queue.
- The same dialogue availability rules apply when timed orders are disabled.

## Timed orders and courier

- The delayed profile has 1,024 unique, persistent courier BOOK forms.
- One brief letter is dispatched only after the current workshop queue has no
  Pending work; its title, location, and plain-name signature survive save/load.
- Dispatch synchronously adds the BOOK to the vanilla courier container and
  makes its pending-item global match the real container count. Initialization
  repairs a mismatch before any new notice is sent.
- Letters do not enumerate items and remain in the Books inventory after reading
  and collection.
- Collection never depends on receiving or reading a letter, including after
  notice-pool exhaustion.
- The immediate profile installs no courier hook or temporal order runtime and
  writes no order/courier serialization records.
- Only Pending and Ready orders persist. Collected and unresolved records are
  removed silently; death losses are listed once and then removed.
- Time estimates use one shared singular/plural formatter.

## Papyrus sources

- `papyrus/Source` is authoritative for all six PSC files.
- CMake copies them into `dist/Data/Scripts/Source` and compares every file.
- Source-package validation requires both trees to be byte-identical.

## Alchemy

- Patch entries for an NPC already on Artisans' own lists are ignored and logged,
  whatever the FormID format (first entry per resolved NPC wins).
- A valid apothecary fallback offers Adept. Alchemists added by other mods use
  the configured default tier (MCM: Mod-Added Artisans) and are logged once.
- Arcadia, Milore Ienth, Elgrim, Frida, and Quintus Navale offer Expert.
- Babette, Bothela, and Nurelion offer Master. Florentius Baenius offers Master
  only after `Bolstering the Ranks` is complete.
- Dead Nurelion does not offer service.
- Each purchase delivers exactly 5 flasks.
- Price is per batch.
- Discount never exceeds 20%.
- Magnitude/duration are fixed ALCH record values.
- Player Alchemy skill/perks do not affect output.
- Every ALCH record has `No Auto-Calc`.
- Only poison ALCH records also have `Poison`.
- Each ALCH `ENIT\Value` matches the manual `ALCHValuePerFlask` source value.
- ALCH values do not allow 20%-discount resale profit. At purchase the check
  uses the higher of the configured value and the potion's value in game.
- Cure Disease appears once as an Adept pure entry.
- Invisibility, Waterbreathing, and Paralysis are pure entries.
- Fortify Enchanting, Fortify Smithing, and Fortify Barter are absent.
- Health, Magicka, and Stamina Over Time appear only in Restore.
- Health Over Time is a direct Peak Value Modifier on Health, not a modifier
  on HealRateMult; its magnitude is applied as points per second.
- All three Over Time effects have three tiered potion records, last 10 seconds,
  and use one private Peak Value Modifier keyword per actor value. Equal doses
  do not add, and the highest active magnitude wins across tiers, matching
  vanilla Fortify Destruction behavior.
- Health, Magicka, and Stamina Over Time each restore 10/20/40 points per
  second and remain independent, so one potion from each family may operate at
  the same time.
- Potion of Fortify Destruction and Potion of Fortify Carry Weight retain their
  full purchase-menu names; unrelated long potion names remain compact.

## v1.5 exclusive effects and Skyforge records

- Elemental Burst affects Fire, Frost, and Shock spell damage only.
- Silver Blessing applies 25%/50% only against undead and werebeasts and does
  not include Dremora.
- Berserker Soul ignores 20%/40% of armor; Honed Metal adds 15%/30% physical
  weapon damage; Reinforcement reduces stagger by 20%/40%.
- Weapon effects modify the final damage of the exact `HitData::Populate` call
  for the attacking item instance. One function detour covers every caller
  instead of scanning and rewriting individual call sites. Honed Metal and
  Silver Blessing multiply only that hit; Berserker Soul derives its multiplier
  from the target's current Damage Resist and Skyrim's armor scale and cap.
- Only apparel effects use temporary perks, which are purged against the
  player's actual perk state around load boundaries. Weapon effects retain no
  perk or equipped state, preventing repeated-load multiplication and stale
  bonuses after switching.
- There is no periodic weapon inventory scan. Armor effects refresh after load
  and armor equip events; the player update hook performs only the bounded
  Unburden sprint calculation.
- `DataLoaded` performs no player inventory/equipment access and installs no
  player-update hook; those runtime operations begin only after a save or new
  game has loaded.
- Unburden makes worn armor weightless, removes armor movement penalties, and
  reduces sprint and power-attack Stamina costs by 15%/30%.
- Unburden refunds at most 15%/30% of Skyrim's expected sprint drain for the
  frame; simultaneous external Stamina damage cannot increase that refund.
- All new v1.5 effects use Artisans-owned MGEF records; apparel support uses
  Artisans-owned PERK records cloned from exact vanilla structures. Elemental
  Burst and Unburden do not grant vanilla perks.
- Reinforcement may stack multiplicatively with Force Without Effort.
- Eorlund substitutes the approved Steel weapons, Steel Shield, and Steel Plate
  pieces before pricing, order creation, and delivery only when the Custom
  Skyforge Equipment option is installed. All 12 Skyforge results are Artisans-owned forms with approved
  balance and temper recipes; vanilla Skyforge weapons are not overridden.
- The four Skyforge Plate body pieces total 96 base armor and all carry
  `ArmorMaterialSteelPlate`, so Matching Set recognizes them only when the
  player owns that perk. The 32-armor shield remains `ArmorMaterialSteel`.
- Eorlund alone offers the unchanged vanilla Wolf Armor, Wolf Boots, Wolf
  Gauntlets and Wolf Helmet under `Wolf` after `Proving Honor`, with or without
  the Custom Skyforge Equipment option.
- Gunmar's buttons distinguish Dawnguard light/heavy pieces and torso colors;
  confirmations retain the full configured names.
- Inventory eligibility checks never attach a live ExtraDataList to a temporary
  InventoryEntryData owner.
- Long Bow is an explicit Adept entry under Iron; Hunting Bow remains under
  Steel.
- Exclusive Enchanting markers have visible FULL/DNAM fields and never carry
  the Hide in UI flag.
- Every new Alchemy effect begins as a complete clone of its validated vanilla
  duration-restoration base record.
- The MGEF group is serialized before ALCH, and purchase-time validation rejects
  any ALCH whose effect did not resolve before payment.
- Catalog construction rejects a non-ALCH form, an empty effect list, or any
  unresolved base effect before the option is exposed in the menu.
- The original 1,024 courier BOOKs remain exactly at `000B00..000EFF`; every
  new support record is allocated after that range.

## Transaction feedback

- Every successful immediate craft, improvement, timed order, enchantment, and
  alchemy purchase plays Skyrim's `ITMGoldDown` coin-spend sound.
- Cancellation, failed delivery, validation failure, insufficient gold, and
  refunded transactions do not play the sound.

## v1.3.0 Auto-Calc regression checks

- Run `tools/ValidateArtisansOfSkyrimAlchemyEsp.py` against the staged ESP.
- Confirm all 63 ALCH records pass. The original 54 retain FormIDs `000A00`
  through `000A35`, and the new records use `000A36` through `000A3E`.
- In game, compare inventory values before and after changing Alchemy skill/perks.
- Buy a discounted batch, then confirm reselling the five flasks cannot exceed the service cost.

## Historical v1.2.0 corrective regression checks

- Open an ammunition entry and record the displayed final price.
- Confirm the purchase and verify the exact same amount of gold is removed.
- Verify arrows deliver exactly 24 and bolts deliver exactly 10.
- Repeat with no service discount and with the maximum 20% discount.
- Verify Nord Hero weapons remain available from Eorlund after Glory of the Dead.
- Verify ordinary Ancient Nord weapons remain excluded.
