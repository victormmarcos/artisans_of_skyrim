# ESP Generation Notes

`tools/GenerateArtisansOfSkyrimEsp.py` generates
`dist/Data/ArtisansOfSkyrim.esp` from the reviewed source tables.

## Masters

- `Skyrim.esm`
- `Dawnguard.esm`
- `Dragonborn.esm`

The retired standalone dialogue plugins are not masters.

## Generated records

| Signature | Count |
|---|---:|
| TES4 | 1 |
| QUST | 1 |
| DLBR | 5 |
| DIAL | 5 |
| INFO | 106 |
| GLOB | 5 |
| ALCH | 63 |
| BOOK | 1,024 |
| KYWD | 3 |
| MGEF | 10 |
| ENCH | 6 |
| PERK | 8 |
| FLST | 2 |
| WEAP | 7 |
| ARMO | 5 |
| COBJ | 12 |

Main local FormIDs:

- `000800`: `AOS_DialogueQuest`
- `000805`-`000808`: the four service topics
- `00084F`: `AOS_HasOrdersForCurrentSpeaker`
- `000850`: `AOS_CollectOrdersBranch`
- `000851`: `AOS_CollectOrdersTopic`
- `0008F0`: `AOS_SmithServicesForCurrentSpeaker`
- `0008F1`-`0008F3`: Smithing, Enchanting, Alchemy access globals
- `000930`-`000933`: retired experimental silent INFO IDs; not reused
- `000940`-`000943`: silent fallback service INFOs without a VoiceType condition
- `000A00`-`000A35`: the original 54 `AOS_ALCH_*` records
- `000A36`-`000A3E`: the nine v1.5 exclusive `AOS_ALCH_*` records
- `000B00`-`000EFF`: the 1,024 unique courier notice BOOK records
- `000F00` and later: v1.5 keywords, Magic Effects, enchantments, perks,
  FormLists, Skyforge equipment, and tempering recipes

The 111 INFO records contain 79 voiced service responses, 27 voiced collection
responses, and 5 silent fallbacks (four services and collection) for voice types
without a vanilla line. Voiced responses share reviewed vanilla subtitle/audio through DNAM; no audio is
copied. Service INFOs require the corresponding access global to equal one.
Smithing also requires AOS_SmithServicesForCurrentSpeaker; collection additionally
requires AOS_HasOrdersForCurrentSpeaker. Runtime publishes zero when unsupported
or locked and one only when available. The fallbacks follow the voiced INFOs, use
the same access conditions and fragments, and carry no audio. There are no begin
fragments.

The voiced service and collection TopicInfo end fragments invoke their corresponding
Papyrus native only after the vanilla response. Feran Sadri uses the neutral
Enchanting response `Take a look.`. Eorlund uses
`Skyrim.esm|000B80C8`, `Gods be praised.`.

Every generated ALCH retains its manual value and `No Auto-Calc`; only poisons
also carry the `Poison` flag. Historical FormIDs and the full courier BOOK range
remain stable. Each Over Time family uses one direct Peak Value Modifier and a
private associated keyword. The engine applies only the strongest active tier
within a family instead of adding magnitudes; Health, Magicka, and Stamina use
separate keywords.
All nine Over Time records retain their stable FormIDs, last 10 seconds, and use
manual values of 30/60/120 at Adept/Expert/Master. The ALCH validator audits all
63 records against the strict maximum-discount resale rule.

Run after regeneration:

```powershell
python tools/ValidateArtisansOfSkyrimAlchemyEsp.py
python tools/ValidateArtisansOfSkyrimDialogue.py
python tools/ValidateGameplayRecords.py
```

The ESP and dialogue validators require no game installation. The voice-asset
validator additionally requires explicit paths to the local masters, Strings
directory, and voice archive. CK/xEdit inspection and in-game testing remain
the final runtime checks.

## SEQ

`dist/Data/SEQ/ArtisansOfSkyrim.seq` lists the start-game-enabled dialogue quest.
It stays valid while the quest FormID is unchanged. `GenerateArtisansOfSkyrimEsp.py`
rewrites it on a full run; never ship SEQ files for the retired standalone
dialogue plugins.
