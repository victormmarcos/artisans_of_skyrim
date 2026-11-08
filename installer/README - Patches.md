# Making a patch for Artisans of Skyrim

Got an armor mod, a mod with new enchantments or potions, or a new town with a
blacksmith, and want the artisans to work with it? You can do that with a small
text file. No ESP, no xEdit, no scripts. Every section below has templates:
copy the one you need, swap in your values, done.

## First: you might not need a patch at all

Artisans already picks up a lot on its own:

- Weapons and armor from other mods, as long as they use a material the game
  knows (Iron, Steel, Leather, Elven, Glass, Ebony, Daedric, Dragon...) and
  have a forge recipe. Just tick "Include Mod-Added Equipment" in the
  installer.
- Forge recipes from other mods, including ones that make vanilla items
  craftable.
- Blacksmiths and alchemists from other mods that use the vanilla
  blacksmith/alchemist factions.

So the way to go is:

1. **Play with your mods** and see what shows up.
2. **Open the log** to see what was left out and why:
   `Documents/My Games/Skyrim Special Edition/SKSE/ArtisansOfSkyrim.log`
   (GOG: `Skyrim Special Edition GOG`). Search for `Unknown material`,
   `Patch item`, `Mod-added blacksmith` and `Mod-added alchemist`. Most of these
   lines are already written so you can copy them into a patch.
3. **Only then make a patch**, for what's missing.

New enchantments and potions are the exception: those never come in on their
own, because each one needs a price. They always need a patch.

**Patches work with either installer choice.** "Skyrim and Official DLC Only"
vs "Include Mod-Added Equipment" only decides which equipment Artisans picks up
by itself. Everything you put in a patch (items, enchantments, potions, NPCs)
works with both.

## Where the patch goes

```
Data/SKSE/Plugins/ArtisansOfSkyrim/<AnyName>.toml
```

- Any name works, as long as it ends in `.toml`. Ideally put it in a mod of
  its own.
- One file can mix everything: items, enchantments, potions and NPCs.
- A patch only **adds** things. It never removes or changes what the mod or
  another patch already has.
- Every entry is checked on its own. If one is wrong or incomplete, only that
  one is skipped, and the log tells you exactly what's missing. The rest of the
  file still works.
- Lines starting with `#` are just notes.

### Writing forms

Forms are written as `"Plugin.esp|FormID"`, using the FormID **inside its own
plugin** (without the load order number in front), the way xEdit shows it:

```
"Skyrim.esm|0010FB77"
"SomeArmorMod.esp|000800"
"SomeLightMod.esl|000802"
```

Use the exact plugin file name. A wrong name or ID shows up in the log as
`Could not resolve`. A patch for a mod you don't have just does nothing.

### Prices and the MCM

Things added by patches don't show up in the MCM (the MCM is made when the mod
is built, patches are read when the game starts). You set the price in the
patch. Want to change it later just for your game? Edit the patch, or use
`Data/SKSE/Plugins/ArtisansOfSkyrim_Custom.toml` (see each section below).

---

## 1. Weapons, armor, arrows and bolts

**The one rule: if you name an item in your patch, Artisans trusts you and
offers it.** You can even add an item the mod would never offer on its own. But
when the game doesn't give the mod what it needs, you have to give it.

### What you have to fill in

| Field | When | What it is |
|---|---|---|
| `form` | always | the item |
| `tier` | always | blacksmith rank needed: `Adept`, `Expert` or `Master` |
| `price` | if the item has **no forge recipe** or is **worth 0 gold** | the price (for arrows/bolts, of the whole batch) |
| `count` | arrows and bolts **without a forge recipe** | how many come in a batch |

### Templates

**An item with a forge recipe** (the usual case: the mod didn't know its
material, or treats it as unique). The price follows the normal formula, a fee
for the item size + its value:

```toml
[[items.tiers]]
form = "SomeArmorMod.esp|000800"
tier = "Expert"
```

**An item with no forge recipe** (or worth 0 gold). The blacksmith makes it
anyway; you only pay gold for the service, so the recipe was never about
ingredients:

```toml
[[items.tiers]]
form = "SomeArmorMod.esp|000801"
tier = "Expert"
price = 2500
```

**Arrows or bolts with no forge recipe:**

```toml
[[items.tiers]]
form = "SomeArrowMod.esp|000900"
tier = "Adept"
price = 150     # for the whole batch
count = 24      # how many in a batch
```

**A fixed price for an item that has a recipe** (instead of the formula): same
as above, just add `price`.

**A whole set at once, by material keyword:**

```toml
[[items.tiers]]
keyword = "ArmorMaterialSomeMod"
tier = "Adept"
```

A keyword only sets the rank. It doesn't name the items, so it can't add items
without a recipe and the exclusions below still apply (a keyword can catch
dozens of items from several mods, so it stays on the safe side). Name the
pieces you want to fully unlock.

**Block, or mark as unique:**

```toml
[items]
block = ["SomeArmorMod.esp|000802"]    # never offered, no matter what
special = ["SomeArmorMod.esp|000803"]  # unique: only a Master improves it
```

(`allow = [...]` also exists for older patches; it names an item like
`[[items.tiers]]` does, but keeps the rank the mod finds.)

**Worried about exploits?** Don't be. If your price is lower than what the item
(or batch) sells for, the mod raises it on its own, just to the minimum needed,
so nobody can buy cheap and resell for profit.

### What naming an item unlocks

A named item skips the mod's own exclusions:

- materials the mod doesn't know (guild armors, lots of armor mods);
- items the mod treats as unique;
- recipes that turn one item into another (conversions);
- items that come **enchanted by default**, like the vanilla Nightingale set.
  They're sold with their enchantment, and since the game counts the
  enchantment in the item's value, the price goes up to match. The log notes
  every enchanted item you named, so nothing slips through by accident;
- the Ancient Nord weapon exclusion and the few items the mod blocks by itself;
- the installer choice "Skyrim and Official DLC Only". That choice only limits
  what Artisans finds **on its own**; an item from another mod that you name
  is offered either way.

### What naming an item doesn't change

- **Only weapons, armor, shields, arrows and bolts.** Rings, necklaces (anything
  with a jewelry keyword), circlets (anything with "circlet" in its name or
  keywords), staves and hand-to-hand items are never offered by blacksmiths.
- **The item has to be playable.**
- **Your blocks.** `block` always wins, even over an item you named.
- **Lore gates.** Stalhrim stays with Baldor, Dawnguard gear with Gunmar, the
  Skyforge specials with Eorlund. You can change their rank, not who makes them.
- **Improving** still needs the item in your inventory, not equipped and not a
  quest item. Enchanted items need an Expert or Master, unique ones a Master.

If a named item still doesn't show up, the log has a `Patch item ...` line
telling you why.

---

## 2. Enchantments

Enchantments from other mods never come in on their own. Each one is an entry
pointing to the mod's enchantment record (ENCH).

### What you have to fill in

| Field | What it is |
|---|---|
| `id` | a unique name for the entry, your choice (lowercase, no spaces) |
| `displayName` | the name in the menu |
| `type` | `Weapon` or `Apparel` |
| `group` | the tab it goes in (see the list below) |
| `sourceForm` | the enchantment record (ENCH) from the mod |
| `<rank>Price` | price for that rank: `adeptPrice`, `expertPrice`, `masterPrice`. At least one above 0 |
| `<rank>Magnitude` | strength at that rank, for every rank you gave a price |
| `<rank>Value` | the text shown in the menu for that rank (`"20 points"`, `"25%"`) |

Optional: `minimumTier` (the lowest enchanter rank that offers it, default
`Adept`). Only give prices to the ranks that should offer it.

**Groups (tabs) the mod uses:** `Attribute`, `Combat`, `Magic`, `Resistance`,
`Stealth`, `Utility`, `Absorb`, `Control`, `Damage`. Use one of these so it sits
with similar effects, or a new name to get a tab of its own.

### Templates

**Apparel enchantment, all three ranks:**

```toml
[[enchantments]]
id = "somemod_fortify_mining"
displayName = "Fortify Mining"
type = "Apparel"
group = "Utility"
minimumTier = "Adept"
sourceForm = "SomeMod.esp|000900"
adeptPrice = 600
adeptMagnitude = 10
adeptValue = "10 points"
expertPrice = 1200
expertMagnitude = 20
expertValue = "20 points"
masterPrice = 2400
masterMagnitude = 30
masterValue = "30 points"
```

**Weapon enchantment, Expert and Master only:**

```toml
[[enchantments]]
id = "somemod_poison_damage"
displayName = "Poison Damage"
type = "Weapon"
group = "Damage"
minimumTier = "Expert"
sourceForm = "SomeMod.esp|000901"
expertPrice = 1500
expertMagnitude = 15
expertValue = "15 points"
masterPrice = 3000
masterMagnitude = 25
masterValue = "25 points"
```

The Grand Soul cost is added on top of your price. Enchanters themselves are
hand-picked by the mod (for lore reasons), so patches can't add new enchanters.

To change a price later, in `ArtisansOfSkyrim_Custom.toml`:

```toml
[enchantmentPrices.somemod_fortify_mining]
master = 2500
```

---

## 3. Potions and poisons

Same deal: each potion or poison is an entry. The alchemist sells a batch of
the potion record (ALCH) you point to.

### What you have to fill in

| Field | What it is |
|---|---|
| `id` | a unique name for the entry, your choice (lowercase, no spaces) |
| `displayName` | the name in the menu |
| `itemType` | `Potion` or `Poison` |
| `effect` | the effect name shown in the menu |
| `valueText` | the strength shown in the menu (`"20% for 60 seconds"`) |
| `groups` | the filter(s) it shows under (see the list below) |
| `itemForm` | the potion or poison record (ALCH) from the mod, with at least one effect |
| `batchPrice` | price of the whole batch, above 0 |

Optional: `minimumTier` (default `Adept`; each alchemist sells **only** their own
rank, so pick the rank that should sell it), `batchSize` (default 5) and
`alchValue` (value of one potion).

**Groups (filters) the mod uses:**
- potions: `Restore`, `Regeneration`, `Fortify Ability`, `Resistance`, `Utility`;
- poisons: `Attribute Damage`, `Control`.

An item can be in more than one, like `["Restore", "Regeneration"]`.

### Templates

**Potion:**

```toml
[[alchemy.items]]
id = "somemod_potion_mining"
displayName = "Potion of Mining"
itemType = "Potion"
effect = "Fortify Mining"
valueText = "20% for 60 seconds"
groups = ["Fortify Ability"]
minimumTier = "Adept"
batchPrice = 400
itemForm = "SomeMod.esp|000A10"
```

**Poison:**

```toml
[[alchemy.items]]
id = "somemod_poison_weakness"
displayName = "Poison of Weakness"
itemType = "Poison"
effect = "Weakness to Poison"
valueText = "30% for 30 seconds"
groups = ["Attribute Damage"]
minimumTier = "Expert"
batchPrice = 600
itemForm = "SomeMod.esp|000A11"
```

Don't worry about potions being resold for profit: if the batch could sell for
more than it costs, the price goes up on its own (the mod checks the potion's
real value in game). Fortify Enchanting, Smithing and Barter are never sold.

To change a price later, in `ArtisansOfSkyrim_Custom.toml`:

```toml
[alchemyPrices]
somemod_potion_mining = 350
```

---

## 4. Blacksmiths and alchemists

If a mod's blacksmith or alchemist uses the vanilla factions, Artisans already
finds them. They get the default rank you pick in the MCM ("Mod-Added
Artisans"), and the log lists each one as a line ready to copy.

With a patch you can:

- give one of them their own rank;
- turn **any** NPC into a blacksmith or alchemist, even one with no faction at
  all (a hunter, a guild member, a vanilla NPC the mod doesn't list yet).

**Artisans' own artisans are protected.** The masters and experts the mod sets
up itself, the ones unlocked by quests (Eorlund, Gunmar, Baldor, Florentius...)
and the successors (like Asbjorn taking over from Balimund) can't be changed by
a patch, so nobody can demote Eorlund and break his Skyforge work by accident.
An entry for one of them is ignored however its FormID is written, and the log
says so. Enchanters are hand-picked and can't be added.

### What you have to fill in

| Field | What it is |
|---|---|
| `form` | the NPC (their base record, ActorBase/NPC_ in xEdit) |
| `tier` | `Adept`, `Expert` or `Master` |
| `name` | optional, just for you and the log |

### Templates

**Blacksmith:**

```toml
[[npcs.overrides]]
name = "Some Smith"
form = "SomeTownMod.esp|000D62"
tier = "Expert"
courierLocation = "forge in Some Town"   # optional, used in courier letters
```

**Alchemist:**

```toml
[[alchemy.npcs]]
name = "Some Alchemist"
form = "SomeTownMod.esp|000D63"
tier = "Master"
```

**Alchemist who only serves after a quest:**

```toml
[[alchemy.npcs]]
name = "Some Alchemist"
form = "SomeTownMod.esp|000D64"
tier = "Expert"
requiredQuest = "SomeTownMod.esp|000E10"
lockedReason = "Some Alchemist will help you once you finish her quest."
```

---

## A complete example

```toml
# Some Guild - Artisans of Skyrim patch

# The guild armor has its own material keyword and no forge recipe.
[[items.tiers]]
form = "SomeGuildMod.esp|000801"   # cuirass
tier = "Expert"
price = 1800

[[items.tiers]]
form = "SomeGuildMod.esp|000802"   # boots
tier = "Expert"
price = 900

# The guild's own blacksmith and alchemist.
[[npcs.overrides]]
name = "Guild Smith"
form = "SomeGuildMod.esp|000D62"
tier = "Expert"

[[alchemy.npcs]]
name = "Guild Alchemist"
form = "SomeGuildMod.esp|000D63"
tier = "Adept"

# A potion the guild mod adds.
[[alchemy.items]]
id = "someguild_shadow_draught"
displayName = "Shadow Draught"
itemType = "Potion"
effect = "Fortify Sneak"
valueText = "25% for 60 seconds"
groups = ["Fortify Ability"]
minimumTier = "Adept"
batchPrice = 350
itemForm = "SomeGuildMod.esp|000A20"
```

Made a patch for a popular mod? Share it! Post it on Nexus and tell me on the
mod page, I'll gladly link it.
