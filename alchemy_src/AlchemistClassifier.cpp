// Artisans of Skyrim
// Copyright (C) 2026 victormmarcos
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "NPCAlchemyServices/AlchemistClassifier.h"

#include "NPCSmithServices/Domain.h"

#include <mutex>
#include <unordered_set>

#include "NPCAlchemyServices/FormResolver.h"

namespace NPCAS
{
    AlchemistClassifier& AlchemistClassifier::GetSingleton()
    {
        static AlchemistClassifier singleton;
        return singleton;
    }

    void AlchemistClassifier::Configure(const Config& a_config)
    {
        config_ = std::addressof(a_config);
    }

    std::optional<AlchemistContext> AlchemistClassifier::Classify(
        RE::Actor* a_actor) const
    {
        if (!config_ || !config_->serviceEnabled || !a_actor || a_actor->IsPlayerRef() ||
            a_actor->IsDead()) {
            return std::nullopt;
        }

        const auto base = a_actor->GetActorBase();
        if (!base) {
            return std::nullopt;
        }

        AlchemistContext result;
        result.actor = a_actor->GetHandle();
        result.baseFormID = base->GetFormID();
        result.displayName = a_actor->GetDisplayFullName();
        if (result.displayName.empty()) {
            result.displayName = "Alchemist";
        }

        const auto& resolver = FormResolver::GetSingleton();
        if (const auto service =
                resolver.ServiceForNPC(result.baseFormID, result.displayName)) {
            result.tier = service->tier;
            result.servicesUnlocked = !service->questRequired ||
                (service->requiredQuest && service->requiredQuest->IsCompleted());
            if (!result.servicesUnlocked) {
                result.lockedReason = service->lockedReason.empty() ?
                    "This alchemist is not ready to offer this service." :
                    service->lockedReason;
            }
            return result;
        }

        const auto faction = resolver.AlchemyFaction();
        if (faction && a_actor->IsInFaction(faction)) {
            const auto origin = base->GetFile(0);
            const bool modAdded = origin && !NPCSS::IsOfficialEquipmentPlugin(origin->GetFilename());
            result.tier = modAdded ? config_->modAddedAlchemistTier : ServiceTier::kAdept;
            if (modAdded) {
                static std::mutex loggedMutex;
                static std::unordered_set<RE::FormID> logged;
                const std::scoped_lock lock{ loggedMutex };
                if (logged.insert(result.baseFormID).second) {
                    // Ready to paste into ArtisansOfSkyrim_Custom.toml for a per-NPC tier.
                    SKSE::log::info(
                        "Mod-added alchemist detected ({}): [[alchemy.npcs]] name = \"{}\" form = \"{}|{:06X}\" tier = \"{}\"",
                        ToString(result.tier),
                        result.displayName,
                        origin->GetFilename(),
                        base->GetLocalFormID(),
                        ToString(result.tier));
                }
            }
            return result;
        }

        return std::nullopt;
    }
}
