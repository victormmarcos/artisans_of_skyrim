# Dialogue Plugin Spec

Artisans of Skyrim exposes Smithing craft, Smithing improve, Enchanting,
Alchemy and order collection through the existing dialogue quest.

## Availability

Service questions are shown only when the artisan is supported and unlocked.
The classifier remains authoritative for quests, factions and workshop rules;
no requirements, prices or rank rules are duplicated in the ESP.

The existing activation/crosshair event refresh publishes binary access globals:
`AOS_SmithAccessForCurrentSpeaker`, `AOS_EnchantAccessForCurrentSpeaker`, and
`AOS_AlchemyAccessForCurrentSpeaker`. Zero means unavailable; one means available.
Each service INFO requires its access global to equal one.
`AOS_SmithServicesForCurrentSpeaker` also requires unlocked Smithing. Collection
additionally requires Orders to be enabled and active orders for this artisan.

The experimental silent INFOs at 000930-000933 were removed; those IDs are not
reused. No response-less INFO, begin fragment or MenuTopicManager speaker override
is used. Backend checks still reject stale/direct requests if access has changed.

## Responses and fragments

The 106 voiced INFOs (79 service, 27 collection) share reviewed vanilla responses
through DNAM. The mapping in `source-data/dialogue/vanilla_voice_responses.csv` is
unchanged. No voice or lip-sync files are redistributed. Actor/faction/VoiceType
conditions remain as before, followed by the access requirement.

Five fallback INFOs (000940-000943 and the collection fallback) carry no VoiceType
condition and no voice file. They come after the voiced INFOs, so an artisan whose
voice type has no vanilla line (for example a voice type added by another mod)
still gets the same service questions with a silent subtitle. The ESP has 111
INFO records in total.

All fragments run at the end of the normal response and call the existing natives:
`OpenSmithCraftMenu`, `OpenSmithImproveMenu`, `OpenEnchantMenu`, `OpenAlchemyMenu`,
and `CollectSmithOrders`. The dialogue quest and SEQ IDs remain unchanged.

Test Eorlund both before and after either accepted trust quest. Repeat for Gunmar,
Baldor, Sergius, Sybille, Neloth, Feran and Florentius with their existing gates.
Reopen dialogue after completing a prerequisite. Locked services must be absent;
available services must play their normal voice and open without crashing.
