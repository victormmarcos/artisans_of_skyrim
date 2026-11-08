# Alchemy Implementation Notes

Alchemy is implemented as an NPC service, not a vanilla barter menu and not a simulated alchemy-lab craft.

## Runtime Model

- Valid configured Expert/Master alchemists use `alchemy.npcs` in `ArtisansOfSkyrim.toml`.
- Other valid apothecaries can provide Adept through `JobApothecaryFaction`.
  Those added by other mods use the configured default tier (MCM: Mod-Added
  Artisans); patch files can add `[[alchemy.npcs]]` entries.
- Dead actors are rejected by the classifier, so Nurelion naturally stops offering service after death.
- The player pays one service price per batch.
- Each batch delivers 5 pre-authored ALCH items.
- Player Alchemy skill and perks do not modify magnitude or duration.

## Catalog

Authoring sources:

- `source-data/alchemy/Artisans_of_Skyrim_unified.xlsx` (reference workbook)
- `source-data/alchemy/alchemy_catalog.csv`
- `source-data/alchemy/alchemy_records.csv`

`alchemy_records.csv` is the authoring/audit table. `dist/Data/ArtisansOfSkyrim.esp` contains one generated ALCH record per row, and runtime config uses `itemForm = "ArtisansOfSkyrim.esp|000Axx"` plugin-local FormIDs. `source-data/alchemy/alchemy_formids.csv` records the generated mapping.

## Economy

`batchPrice` is the service price for 5 flasks. `alchValue` / `ALCHValuePerFlask` is the inventory resale value of one flask.

Every generated ALCH record stores `ALCHValuePerFlask` in `ENIT\Value` and has
the `No Auto-Calc` flag. Poison records also retain the `Poison` flag. This keeps
Skyrim from recalculating resale value from the effects and prevents ordered
batches from becoming a profitable resale loop.

The strict resale safety check is:

```text
minimum_paid = ceil(batchPrice * (1 - maxDiscount))
resale_total = ALCHValuePerFlask * batchSize
resale_total < minimum_paid
```

The service applies the discount, rounds up, then rounds up to a multiple of 5
(`ApplyPriceDiscount`). At purchase the resale value is the higher of
`ALCHValuePerFlask` and the potion's value in game, so a potion changed by another
mod is still checked; the price is raised to `MinimumSafeBatchPrice` when needed.
MCM price sliders never go below that safe minimum.

The validator reports the current value and maximum safe value for every one of
the 63 records. Over Time values are 30/60/120 for Adept/Expert/Master.

Strong effects use the lower side of the value range, especially Invisibility, Paralysis, Damage Health + Lingering Damage Health, Resist Magic, Slow, Frenzy, and Fear.

## Balance Rules

- Batch size is always 5.
- Discount cap is always 20%.
- Fortify Enchanting, Fortify Smithing, and Fortify Barter are excluded.
- Cure Disease exists once as an Adept pure-effect potion. Alchemy catalogs are
  exact-tier lists, so higher-ranked alchemists do not repeat lower-tier entries.
- Invisibility, Waterbreathing, and Paralysis are pure-effect entries only.
- Fortify Carry Weight lasts 120 seconds.
- Human Flesh and Human Heart dependent recipes are excluded.
- No 3-effect or 4-effect recipes are included.
- Health, Magicka, and Stamina Over Time restore 10/20/40 points per second for
  10 seconds and use independent exclusivity keywords.

## Validation

Before release, validate every generated ALCH record in `ArtisansOfSkyrim.esp`:

- EDID matches `AOS_ALCH_*`.
- FULL matches the table display name.
- ENIT `No Auto-Calc` is present on every record.
- ENIT `Poison` is present only on poison records.
- ENIT Value matches `ALCHValuePerFlask`.
- DATA Weight matches the `Weight` column, currently `0.5`.
- Effects, magnitude, and duration match `alchemy_records.csv`.
- Model and icon are inherited from a consistent potion/poison template or filled from `ModelPath` / `IconPath`.
- The generated ESP passes `tools/ValidateArtisansOfSkyrimAlchemyEsp.py`.
- Catalog construction confirms each configured form is an ALCH with at least
  one fully resolved base effect before it can appear in the service menu.
