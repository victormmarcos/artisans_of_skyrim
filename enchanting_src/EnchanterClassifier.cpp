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

#include "NPCEnchantingServices/EnchanterClassifier.h"

#include "NPCEnchantingServices/FormResolver.h"

namespace NPCES
{
    EnchanterClassifier& EnchanterClassifier::GetSingleton()
    {
        static EnchanterClassifier singleton;
        return singleton;
    }

    void EnchanterClassifier::Configure(const Config& a_config)
    {
        config_ = std::addressof(a_config);
    }

    std::optional<EnchanterContext> EnchanterClassifier::Classify(
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

        const auto& resolver = FormResolver::GetSingleton();
        const auto formID = base->GetFormID();

        EnchanterContext result;
        result.actor = a_actor->GetHandle();
        result.baseFormID = formID;
        result.displayName = a_actor->GetDisplayFullName();
        if (result.displayName.empty()) {
            result.displayName = "Enchanter";
        }

        if (formID == resolver.FarengarFormID() ||
            formID == resolver.CalcelmoFormID()) {
            result.tier = ServiceTier::kAdept;
        } else if (formID == resolver.SergiusFormID()) {
            result.tier = ServiceTier::kExpert;
        } else if (formID == resolver.SybilleFormID()) {
            result.tier = ServiceTier::kExpert;
        } else if (
            formID == resolver.NelothFormID() ||
            formID == resolver.FeranSadriFormID()) {
            result.tier = ServiceTier::kMaster;
        } else {
            return std::nullopt;
        }

        const auto player = RE::PlayerCharacter::GetSingleton();
        if (formID == resolver.SergiusFormID()) {
            // The Arch-Mage may only be in the Arch-Mage faction (e.g. with
            // College quest mods), so either faction counts as membership.
            static const auto archMageFaction = [] {
                const auto dataHandler = RE::TESDataHandler::GetSingleton();
                return dataHandler ?
                    dataHandler->LookupForm<RE::TESFaction>(0x103372, "Skyrim.esm") :
                    nullptr;
            }();
            result.servicesUnlocked = player &&
                ((resolver.CollegeFaction() && player->IsInFaction(resolver.CollegeFaction())) ||
                 (archMageFaction && player->IsInFaction(archMageFaction)));
            if (!result.servicesUnlocked) {
                result.lockedReason =
                    "Sergius only offers enchanting services to members "
                    "of the College.";
            }
        } else if (formID == resolver.SybilleFormID()) {
            result.servicesUnlocked =
                resolver.PotemaFirstQuest() &&
                resolver.PotemaFirstQuest()->IsCompleted();
            if (!result.servicesUnlocked) {
                result.lockedReason =
                    "Sybille is not willing to offer this service yet.";
            }
        } else if (formID == resolver.NelothFormID()) {
            result.servicesUnlocked =
                resolver.NelothDwemerQuest() &&
                resolver.NelothDwemerQuest()->IsCompleted();
            if (!result.servicesUnlocked) {
                result.lockedReason =
                    "Neloth will not offer enchanting services until "
                    "you have proven useful to him.";
            }
        } else if (formID == resolver.FeranSadriFormID()) {
            const bool volkihar =
                player && resolver.VolkiharFaction() &&
                player->IsInFaction(resolver.VolkiharFaction());
            const bool dawnguard =
                player && resolver.DawnguardFaction() &&
                player->IsInFaction(resolver.DawnguardFaction());
            result.servicesUnlocked = volkihar && !dawnguard;
            if (!result.servicesUnlocked) {
                result.lockedReason =
                    "Feran only offers this service to trusted allies "
                    "of Clan Volkihar.";
            }
        }

        return result;
    }
}
