# Vanilla Voice Response Audit

`source-data/dialogue/vanilla_voice_responses.csv` is the authoritative mapping
for the response following each service or order-collection question.

Representative reviewed sources:

| Service | Source | Subtitle | Use |
|---|---|---|---|
| Smithing | `Skyrim.esm\|000A9636` | Hmm... Blades, helmets. Pretty much anything to suit your needs. | Supported blacksmith voices |
| Smithing | `Skyrim.esm\|000B80C8` | Gods be praised. | Eorlund/MaleNordCommander |
| Smithing | `Dawnguard.esm\|0000F820` | What do you need? Blades, armor, smithing materials? | Gunmar |
| Enchanting | `Skyrim.esm\|0007F6BC` | Take a look. | Neutral supported voices, including Feran |
| Enchanting | `Dragonborn.esm\|00017E70` | Hmm. I might have a few staffs, if you're interested in that sort of thing. | Neloth |
| Alchemy | `Dawnguard.esm\|0000E7A9` | Potions my friend, potions! Possibly some spells. Maybe some other things I've forgotten about. Actually I'm not sure. Let's see, shall we? | Florentius |
| Alchemy | `Dragonborn.esm\|00036A5D` | I probably have some potions around here that might interest you. | Elynea |

Selection follows the artisan's role in this mod. A profession-specific line is
used only when its wording fits; otherwise the mapping uses neutral commerce
text. Feran Sadri is the regression case: although his base-game commerce can
mention Alchemy, his Artisans role is Enchanting, so only `Take a look.` is used.

Eorlund's former mapping was rejected because the observed audio and generated
subtitle did not agree. Both Smithing topics now share
`Skyrim.esm|000B80C8`, whose English subtitle and FUZ are `Gods be praised.`.

The generated plugin contains 79 voiced service INFO records and 27 voiced
collection INFO records, plus 5 silent fallbacks for voice types without a
vanilla line (for example voice types added by other mods). Access globals hide
the service questions while locked. Every distinct SharedInfo/VoiceType combination was resolved in local
official masters and Strings and matched to a FUZ in the official voice archive.
No voice or lip-sync asset is copied into the package.

Run `tools/ValidateVanillaVoiceAssets.py` and
`tools/ValidateArtisansOfSkyrimDialogue.py` after each dialogue regeneration.
