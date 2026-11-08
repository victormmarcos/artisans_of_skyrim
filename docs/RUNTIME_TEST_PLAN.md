# Runtime Test Plan

Use a disposable save and test both equipment-origin profiles, both Smithing
delivery profiles, and the three optional additions (Custom Skyforge
Equipment, Custom Enchantments, Custom Potions) on and off. Only
`ArtisansOfSkyrim.esp` should be enabled as a service plugin.

## Startup and dialogue

1. Confirm the log reports Smithing, Enchanting, and Alchemy initialized.
2. Confirm `AOS_DialogueQuest` starts on a new game and after loading a save.
3. Test every supported artisan and faction/VoiceType fallback, plus one artisan
   whose voice type has no vanilla line (silent fallback): the vanilla
   response and matching subtitle should play once before the Service Menu when
   unlocked. When locked, the service question must not appear. Repeat after
   completing the requirement and reopening the conversation.
4. Confirm Eorlund says and displays `Gods be praised.`.
5. Confirm Feran Sadri says only neutral `Take a look.` before Enchanting and
   never mentions potions or Alchemy.
6. Load a clean save created before Artisans was installed and remain in game
   for at least two minutes. Confirm there is no delayed startup crash with
   Precision, Floating Damage, or other installed combat plugins.
7. Before loading any save, remain at the main menu for at least one minute.
   Confirm `DataLoaded` completes and the game stays open; player-dependent
   Artisans runtime hooks must not activate until a save or new game loads.
8. Complete one immediate craft, improvement, timed Smithing order,
   enchantment, and Alchemy purchase. Each successful confirmation must play
   Skyrim's coin-spend sound exactly once. Cancel and deliberately fail one
   transaction; neither path may play the sound.

## Smithing and equipment compatibility

1. Verify Adept, Expert, Master, lore locks, Craft, Improve, prices, discounts,
   and resale behavior. Prices end in 0 or 5.
2. In the official-only profile, verify Skyrim, Dawnguard, HearthFires, and
   Dragonborn items remain eligible while Creation Club and third-party items
   are excluded.
3. Verify Leather Helmet, Dragonplate Helmet, and Dragonscale Helmet appear for
   eligible smiths even though they use Hair + Circlet biped slots.
4. Verify actual circlets never appear in either immediate or timed crafting.
5. In the mod-added profile, test a protective helmet using Head, Hair, Circlet,
   or a combination of those slots. It should work when its material and tier are
   recognized; a mod-added circlet must remain blocked.
6. With Custom Skyforge Equipment installed, use Eorlund to order each supported Steel
   weapon, Steel Shield, and Steel
   Plate piece. Confirm the final menu entry, price, saved order, and delivered
   object all use the Skyforge substitute under the normal Steel/Steel Plate
   group. Confirm Imperial Sword, Imperial Bow, Hunting Bow, and Long Bow appear
   as Adept entries; Hunting Bow is under Steel and Long Bow is under Iron.
7. Confirm Skyforge buttons use compact complete labels such as `Skyforge Sword`
   and `Skyforge Plate Armor` instead of ellipses, while confirmation screens
   retain the full item name.
8. Equip the four Skyforge Steel Plate armor pieces after acquiring Matching
   Set and verify the perk bonus activates. Confirm the mod does not grant the
   perk automatically and the Steel shield is not counted as a fifth Plate
   piece.
9. After completing `Proving Honor`, confirm only Eorlund offers the unchanged
   vanilla Wolf Armor, Wolf Boots, Wolf Gauntlets and Wolf Helmet in a separate
   `Wolf` category, with and without Custom Skyforge Equipment. Confirm no Wolf
   shield was added.
10. Confirm rings, necklaces, circlets, and staves never appear in improvement.
11. Without Custom Skyforge Equipment, confirm Eorlund delivers ordinary Steel and
    Steel Plate equipment with vanilla stats and no custom Skyforge substitute.
12. Ask Eorlund to improve several weapons and armor pieces, including a custom
    named or enchanted instance. Confirm selection and purchase do not crash,
    only the chosen instance changes, and item weight/carry weight do not rise.
13. With Gunmar, confirm compact buttons distinguish Light/Heavy equipment and
    Red/Gray/Brown torso variants without ellipses; confirmation uses full names.
14. Confirm the ten configured official menu-label exceptions display in full:
    Bonemold Pauldron Armor; all five Improved Bonemold pieces; Nordic Carved
    Gauntlets; both Steel Armor variants; and Steel Imperial Gauntlets. Confirm
    Steel Imperial Gauntlets are under Steel and unrelated long names, including
    mod-added equipment, still use the normal compact label.

## Timed Smithing orders and courier

1. In immediate mode, verify prior instant behavior is unchanged, collection
   dialogue is absent, no courier appears, and the log contains no installed
   courier-letter Book Menu hook or temporal order-event registration.
2. In timed mode, verify arrows and bolts remain immediate and consume no slot.
3. Confirm whole-day durations match the TOML and queues are FIFO per workshop.
4. Place five items with one smith. Earlier items may become collectible, but no
   courier should be requested until the fifth and final pending item is ready.
5. Place one item in an empty queue and confirm its completion requests a courier.
6. Confirm different smiths maintain independent five-slot queues.
7. Wait, sleep, and fast travel across completion times; readiness must update.
   After a notice is queued, confirm the log reports that its BOOK was added
   directly to the WICourier container and gives the resulting pending count.
8. Confirm the letter title is `Letter from <Artisan>` and its handwritten body
   says the orders are ready, gives the correct pickup location, and ends with
   only the artisan's name. It must not list individual items.
9. Confirm Eorlund's letter says `Skyforge in Whiterun`, Gunmar's says
   `Fort Dawnguard`, and other configured smiths use `forge in/at <location>`.
10. Collect ready items before courier arrival. Collection must succeed and the
    letter must not unlock or gate anything.
11. Reach a completed queue by speaking to the smith before another readiness
    update. Collect everything and confirm no empty letter is queued.
12. Save/load with Pending, Ready, and delivered notices; queue state and old
    letter titles/contents must remain stable. Read letters remain in the Books
    inventory after reading and collection.
13. Kill a smith with active work. The work is not delivered or refunded. One
    centered MessageBox must list every lost item and quantity, and those records
    must not appear again after save/load.
14. Place orders through Adrianne and Ulfberth. Confirm both draw from one shared
    five-slot queue and letters name Adrianne. After Adrianne dies, Ulfberth must
    lose Craft, Improve, and Collect in both immediate and delayed profiles.
15. Place orders through Rustleif and Seren. Confirm they share one five-slot
    queue and that either survivor can continue servicing and collecting it.
16. Kill Ghorza, Balimund, Beirand, and Oengul in separate test saves. Tacitus,
    Asbjorn, Heimvar, and Hermir respectively must gain Adept Craft and Improve
    only after the original smith dies. The old queue is lost without refund;
    each successor begins with an empty queue.
17. Remove a plugin that owns an ordered item, then load the save. The missing
    order must disappear without an error, refund message, or future save record.
18. Switch a save with delayed orders to an immediate preset, save, then return
    to delayed. Old orders must not return.

## Enchanting

1. Verify tier access, eligible items, one/two-effect rules, details, price,
   payment, and delivery.
2. Select one effect and open Craft; then repeat with two effects at a Master.
   Each row must show its own price and the confirmation must itemize both.
3. Enchant an item that already has an instance name and confirm that name is
   preserved.
4. Enchant one item from a plain stack and verify exactly one item receives the
   enchantment while the rest remain intact.
5. With Farengar, enchant a Dwarven Mace with Frost Damage. Confirm the service
   accepts the effect and applies both of its internal vanilla components while
   treating it as one selected enchantment.
6. Confirm only a Master accepts two separately selected enchantments.
7. Confirm Utility contains Carry Weight, Waterbreathing, Unburden, and
   Reinforcement while Muffle remains under Stealth.
8. Apply Silver Blessing at Expert and Master, confirm 25%/50%, full charge is
   3,000, final prices are exactly 3,000/6,000 gold, and record approximately
   150/100 charged hits within 5% tolerance. Before using it, confirm the
   inventory displays its effect description and charge bar.
9. Apply Berserker Soul and Honed Metal at both tiers and record approximately
   100/70 charged hits within 5% tolerance.
10. Compare three identical weapons one at a time on suitable targets. Verify
    Honed Metal gives 15%/30% more physical damage and Berserker Soul bypasses
    20%/40% of actual armor. After switching to the unenchanted weapon, confirm
    its next hit immediately returns to baseline.
11. Dual wield one custom weapon and one identical unenchanted weapon. Test
    right-hand and left-hand attacks separately in both hand arrangements. Only
    the hit made by the custom charged instance may receive the effect. Repeat
    with two different custom effects and confirm neither hand lends its effect.
12. Save with Honed Metal equipped, reload that save repeatedly, and confirm its
    15%/30% multiplier is present exactly once for its own attacks after every
    load. Equip an unenchanted copy and confirm its hits remain at baseline.
13. Exhaust a custom weapon's charge and confirm its effect no longer affects its
    attacks. Recharge it without changing weapons and confirm the next hit uses
    the effect again.
14. Verify Silver Blessing changes damage by 25%/50% against targets carrying
    Skyrim's native undead/ghost classifications and against werewolf/werebear
    races, but not ordinary living actors or Dremora. Do not assume that an NPC
    with vampire-themed dialogue or abilities carries an undead keyword.
15. Verify Elemental Burst accepts only rings and necklaces, not circlets; test
    ring/necklace combinations for 50%, 75%, and 100% Fire/Frost/Shock power.
16. Verify Expert/Master Unburden supplies weightless worn armor and 15%/30%
    sprint and power-attack savings. While sprinting, apply a separate Stamina
    damage effect; Unburden must refund only its share of vanilla sprint drain,
    never any of the external damage. Verify Reinforcement reduces stagger by
    20%/40%.
17. Open the disenchant category with exclusive and ordinary enchanted items in
    inventory, with and without the Custom Enchantments option. Every exclusive
    instance must be absent on the first visible list update, without changing
    categories; ordinary vanilla-only items must retain Skyrim's normal
    eligibility. Verify each visible row's name, icon, effect, type, weight, and
    value before and after visiting another category; no column may shift to a
    neighboring item. If another menu plugin exposes a protected item, final
    confirmation must still be blocked.
18. Without Custom Enchantments, confirm the six new service choices are hidden;
    without Custom Skyforge Equipment, custom Skyforge substitutions are absent. Previously created exclusive
    equipment must still function and remain protected from disenchanting.

## Books and letters

1. Open several ordinary vanilla books, journals, and letters before and after
   receiving a smithing notice; none may crash the game.
2. Open each received smithing notice and confirm its own title/body remain
   correct after saving, loading, and opening ordinary books again.
3. Confirm the generated ESP contains 1,024 unique notice BOOKs in local FormID
   range `000B00..000EFF`. If the pool is exhausted in a synthetic test, manual
   collection must remain available.

## Alchemy

1. Verify tier access, dead-Nurelion handling, five-flask batches, price,
   discounts, and resale behavior.
2. Confirm effects, magnitudes, durations, manual values, `No Auto-Calc`, poison
   flags, and Cure Disease match the generated catalog.
3. Confirm Health, Magicka, and Stamina Over Time appear
   only in Restore at the exact NPC tier.
4. At the purchase confirmation for every new batch, confirm the menu remains
   stable. Buy all nine tiered service batches, wait at least 15 seconds after delivery,
   and confirm no delayed crash occurs.
5. Consume one Health, Magicka, and Stamina Over Time potion at each tier and
   verify the exact 10/20/40 per-second values and 10-second duration for all
   three families.
6. Drink repeated same-tier potions for one actor value before the first expires.
   Confirm restoration never accelerates and the latest equal dose renews the
   effective 10-second window.
7. Drink Expert followed by Master and confirm the Master magnitude takes over.
   Repeat with Master followed by Expert and confirm the Master magnitude remains
   active. Health, Magicka, and Stamina doses must still operate simultaneously
   because they use separate Peak Value Modifier keywords.
8. Without Custom Potions, confirm all three effect families are hidden while
   potions already in inventory retain their engine-driven effects.
9. Before completing `Bolstering the Ranks`, confirm Florentius does not offer
   the Alchemy dialogue. Complete the quest, speak to him again, and confirm he offers the
   Master catalog.
10. Confirm Potion of Fortify Destruction and Potion of Fortify Carry Weight keep
   their full purchase-menu names at Expert and Master. Other long Alchemy names
   must retain the normal compact label.
11. For every one of the 63 potions/poisons, verify the five inventory values
    total strictly less than the service price at the maximum 20% discount.

## FOMOD and packaging

1. Install the four presets with the option checkboxes on and off; inspect the
   resulting TOML, option files and MCM pages.
2. Confirm the installer page says `three new Over Time potion effects`, not a
   total number of potions or batches, and explains the Skyforge behavior.
3. Confirm the archive includes its third-party notice and both READMEs, and the
   manager installs one ESP, DLL, SEQ, bridge PEX, MCM PEX, five fragment PEX
   files, the translation files, exactly one main TOML and the chosen option files.
4. Confirm the installable contains no old standalone plugin, PDB, PDF, log,
   source table, or redistributed voice/lip-sync asset.

## Acceptance matrix

Run on the supported runtimes with matching SKSE and Address Library; the 1.7
matrix below still applies to everything not listed here.

- Menu: category tabs (Left/Right, D-pad, click), search (type, Enter keeps,
  Esc clears), column-title sorting and reversing, filters list (sort, Material,
  Effect). Titles in capitals, values in sentence case, no all-capital values.
- Mouse: hover selects; clicking the selected row uses it; the wheel scrolls from
  anywhere and is not undone by hovering; preview rotates while dragging over the
  3D area. Right stick rotates the preview.
- Key icons: keyboard, Xbox and PlayStation prompts in the footer, card buttons
  and confirmations; prompts start at the left edge and align with their labels.
- 16:9 and ultrawide: the item is centred in the preview area and the card text
  stays inside its frame.
- Prices: craft and improve an item of each size; prices end in 0 or 5. Buy a
  potion batch and confirm it cannot be resold for more than its price.
- MCM (with MCM Helper): change a fee, an enchantment price, a potion price and
  the Mod-Added Artisans tiers; reopen a service and confirm the new values.
  Custom pages appear only with their installer option.
- Patches: a patch file with `[[items.tiers]]`, `[[npcs.overrides]]` and an
  invalid entry; the log reports `added` and `skipped` lines. The log lists
  mod-added artisans and unknown-material items as lines ready to copy.
- Translation: edit one line of `ArtisansOfSkyrim_ENGLISH.txt`; the menu and MCM
  show the new text.
- Dialogue: an artisan with a voice type from another mod shows the service
  questions with a silent line and opens the menu.
- Sergius with the Arch-Mage; Improve on a unique item requires a Master; the
  Wolf Helmet is offered with the Wolf set.

## 1.7 acceptance matrix

Kept for regression testing, updated to the current behavior.

Run each applicable case on Steam 1.5.97, 1.6.640, 1.6.1170, 1.7.104 and
GOG 1.6.659/1.6.1179 with matching SKSE and Address Library. Compare the startup
log with the actual executable/store; do not infer support from DLL metadata.

- Open Craft, Improve, Enchanting, Alchemy, and Orders through dialogue.
- At a locked Eorlund, Gunmar, Sybille, and Florentius, confirm the supported
  service questions are absent. Complete the same requirements and confirm
  the normal voiced reply and service access. Sybille must show Expert; test dual
  enchantments with Neloth or Feran instead.
- Verify conventional labels, Alchemy `(x5)`, Result Quality, and full Unburden
  text. No list-position counter or received-weight delta should appear.
- Open Eorlund's full craft catalog with mouse and gamepad, cancel/reopen and
  switch to Improve. Confirm no crash while preparing or publishing item details.
  Repeat with Orders enabled and disabled.
- Check the key icons in keyboard, Xbox and PlayStation modes,
  including ordinary confirmation, dual-effect confirmation, Rename, filter
  navigation and both shoulder controls. Icons and labels must stay centered.
- Trigger insufficient gold, changed inventory, and a service failure. Confirm
  the notice is readable, then make a successful transaction and read its result.
- Click rows, action buttons, tabs, filters, options, and confirmation buttons.
  Wheel through long lists/popups repeatedly; cancel and reopen, then switch to
  the D-pad/left stick. No duplicate steps or automatic purchases should occur.
- Gamepad input hides both cursor layers. Real mouse input restores the cursor;
  tiny jitter does not alternate modes. Check after Rename and close/reopen.
- Check the left prompt group stays before screen center in every mode; Gold
  and Carry Weight remain correct on the right. Check Xbox and PlayStation
  platform reporting, including an XInput-emulated PlayStation controller.
- Confirm the preview rotates only while dragging over it or with the right stick.
- Collect ready Orders directly with one Collect action and no confirmation.
- Browse, filter, sort, cancel, go back, close and reopen with mouse, keyboard,
  and gamepad. Rename must not leave gameplay text input enabled.
- Confirm that the native 3D model loads for weapons, armor, jewelry, potions,
  ammunition and equipment added by another mod, and clears when no row matches.
- Test single and dual enchanting, invalid/removed item instances, existing
  improvements, custom names and protected enchantments.
- Test immediate delivery near carry capacity, insufficient gold, repeated
  rapid confirmation, and ordinary purchasing after cancelling a confirmation.
- Test timed orders, instant ammunition, a full workshop queue, and collective
  pickup of several ready items. Save/load while orders are pending.
- Open/close vanilla inventory and crafting before and after the new menu;
  preview state, cursor, pause state and input must be restored.
- Test vanilla UI and SkyUI, widescreen and 16:9, custom fonts and long names.
- Confirm the installer looks correct in MO2 and Vortex, the Website opens the
  correct Nexus page, and every installer combination installs the menu SWF.

## Eorlund opening and poison magnitude follow-up

- Install the entire candidate (DLL, SWF, ESP and the selected configuration preset).
- Reopen Eorlund's unlocked Craft/Improve pages several times. Repeat with Orders
  on/off, Skyforge substitutions on/off, and another Master blacksmith. Confirm
  all UI stages finish, the first model loads, and filters/Price sorting work.
  If a crash persists, retain both the SKSE plugin log and Crash Logger stack.
- Purchase Fear and Frenzy: inventory descriptions must say up to level 30 for
  60 seconds. Test eligible targets below/at/above the cap without immunity or
  resistance changes; descriptions and actual results must agree.
- Purchase both Slow tiers: confirm 50% slowdown for 10/20 seconds. Verify
  Paralysis, Invisibility and Waterbreathing remain duration-only as in vanilla.
- Inspect the longest single/dual Alchemy descriptions: Price stays beside the
  title, Effect stays below, and confirmation still charges the whole five-item batch.

Offline tests do not exercise Skyrim's Scaleform VM, model renderer or combat.
