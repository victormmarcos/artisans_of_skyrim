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

#include "NPCSmithServices/SmithClassifier.h"
#include "ArtisansOfSkyrim/Localization.h"

#include "NPCSmithServices/FormResolver.h"

#include <array>
#include <string_view>

namespace
{
    enum class WorkshopKind
    {
        kProxy,
        kShared,
        kSuccessor
    };

    struct WorkshopRule
    {
        WorkshopKind kind;
        RE::FormID primaryBase;
        RE::FormID primaryReference;
        std::string_view primaryName;
        RE::FormID secondaryBase;
        RE::FormID secondaryReference;
        std::string_view secondaryName;
        std::string_view courierLocation;
    };

    // Local FormIDs from Skyrim.esm. These named references are persistent,
    // so their death state can be checked even when the player is speaking to
    // the other member of the workshop.
    constexpr std::array<WorkshopRule, 6> kWorkshopRules{
        WorkshopRule{
            WorkshopKind::kProxy,
            0x00013BB9,
            0x0001A67C,
            "Adrianne Avenicci",
            0x00013B9F,
            0x000D15B0,
            "Ulfberth War-Bear",
            "forge in Whiterun" },
        WorkshopRule{
            WorkshopKind::kShared,
            0x0001361E,
            0x0001A6BE,
            "Rustleif",
            0x0001361F,
            0x0001A6BF,
            "Seren",
            "forge in Dawnstar" },
        WorkshopRule{
            WorkshopKind::kSuccessor,
            0x0001339A,
            0x00019906,
            "Ghorza gra-Bagol",
            0x0001402D,
            0x00019907,
            "Tacitus Sallustius",
            "forge in Markarth" },
        WorkshopRule{
            WorkshopKind::kSuccessor,
            0x0001334C,
            0x00019DF1,
            "Balimund",
            0x00019DF2,
            0x00019DF3,
            "Asbjorn Fire-Tamer",
            "forge in Riften" },
        WorkshopRule{
            WorkshopKind::kSuccessor,
            0x00013261,
            0x000198D8,
            "Beirand",
            0x00013286,
            0x000198C4,
            "Heimvar",
            "forge in Solitude" },
        WorkshopRule{
            WorkshopKind::kSuccessor,
            0x00014142,
            0x0001B135,
            "Oengul War-Anvil",
            0x0001412D,
            0x0001B136,
            "Hermir Strong-Heart",
            "forge in Windhelm" }
    };

    [[nodiscard]] const WorkshopRule* WorkshopForBase(
        RE::FormID a_baseFormID)
    {
        const auto found = std::ranges::find_if(
            kWorkshopRules,
            [a_baseFormID](const WorkshopRule& a_rule) {
                return a_rule.primaryBase == a_baseFormID ||
                    a_rule.secondaryBase == a_baseFormID;
            });
        return found != kWorkshopRules.end() ?
            std::addressof(*found) :
            nullptr;
    }

    [[nodiscard]] const WorkshopRule* WorkshopForKey(
        RE::FormID a_workshopKey)
    {
        const auto found = std::ranges::find_if(
            kWorkshopRules,
            [a_workshopKey](const WorkshopRule& a_rule) {
                if (a_rule.kind == WorkshopKind::kSuccessor) {
                    return a_rule.primaryBase == a_workshopKey ||
                        a_rule.secondaryBase == a_workshopKey;
                }
                return a_rule.primaryBase == a_workshopKey;
            });
        return found != kWorkshopRules.end() ?
            std::addressof(*found) :
            nullptr;
    }

    [[nodiscard]] RE::Actor* ActorReference(RE::FormID a_referenceFormID)
    {
        return RE::TESForm::LookupByID<RE::Actor>(a_referenceFormID);
    }

    [[nodiscard]] bool IsDead(RE::FormID a_referenceFormID)
    {
        const auto actor = ActorReference(a_referenceFormID);
        return actor && actor->IsDead();
    }

    [[nodiscard]] bool QuestCompleted(RE::TESQuest* a_quest)
    {
        return a_quest && a_quest->IsCompleted();
    }

    [[nodiscard]] bool IsNamedSettlement(const RE::BGSLocation* a_location)
    {
        if (!a_location) {
            return false;
        }
        static constexpr std::array<std::string_view, 4> keywords{
            "LocTypeCity",
            "LocTypeTown",
            "LocTypeSettlement",
            "LocTypeOrcStronghold"
        };
        return std::ranges::any_of(keywords, [&](std::string_view a_keyword) {
            return a_location->HasKeywordString(a_keyword);
        });
    }

    [[nodiscard]] std::string CourierLocationFor(
        RE::Actor* a_actor,
        RE::FormID a_baseFormID,
        const NPCSS::FormResolver& a_resolver)
    {
        if (const auto found =
                a_resolver.CourierLocations().find(a_baseFormID);
            found != a_resolver.CourierLocations().end()) {
            return found->second;
        }

        for (auto* location = a_actor ? a_actor->GetCurrentLocation() : nullptr;
             location;
             location = location->parentLoc) {
            const auto name = location->GetFullName();
            if (name && name[0] != '\0' && IsNamedSettlement(location)) {
                return AOS::Text::Format("forge in {}", std::string_view{ name });
            }
        }
        if (const auto cell = a_actor ? a_actor->GetParentCell() : nullptr) {
            const auto name = cell->GetFullName();
            if (name && name[0] != '\0') {
                return AOS::Text::Format("forge at {}", std::string_view{ name });
            }
        }
        return "forge where the order was placed";
    }
}

namespace NPCSS
{
    SmithClassifier& SmithClassifier::GetSingleton()
    {
        static SmithClassifier singleton;
        return singleton;
    }

    void SmithClassifier::Configure(const Config& a_config)
    {
        config_ = std::addressof(a_config);
    }

    RE::FormID SmithClassifier::WorkshopKeyForBase(
        RE::FormID a_baseFormID) const noexcept
    {
        const auto rule = WorkshopForBase(a_baseFormID);
        if (!rule) {
            return a_baseFormID;
        }
        if (rule->kind == WorkshopKind::kSuccessor &&
            a_baseFormID == rule->secondaryBase) {
            return rule->secondaryBase;
        }
        return rule->primaryBase;
    }

    bool SmithClassifier::OrderOwnerIsDead(
        RE::FormID a_workshopKey,
        RE::FormID a_originalSmithReference) const
    {
        const auto rule = WorkshopForKey(a_workshopKey);
        if (!rule) {
            const auto actor = ActorReference(a_originalSmithReference);
            return actor && actor->IsDead();
        }
        switch (rule->kind) {
        case WorkshopKind::kProxy:
            return IsDead(rule->primaryReference);
        case WorkshopKind::kShared:
            return IsDead(rule->primaryReference) &&
                IsDead(rule->secondaryReference);
        case WorkshopKind::kSuccessor:
            return a_workshopKey == rule->secondaryBase ?
                IsDead(rule->secondaryReference) :
                IsDead(rule->primaryReference);
        }
        return false;
    }

    std::optional<WorkshopArtisan> SmithClassifier::ArtisanForWorkshop(
        RE::FormID a_workshopKey,
        RE::FormID a_preferredSmithReference) const
    {
        RE::FormID artisanReference = a_preferredSmithReference;
        RE::FormID artisanBase = a_workshopKey;
        std::string_view fallbackName = "Blacksmith";
        std::string courierLocation;

        if (const auto rule = WorkshopForKey(a_workshopKey)) {
            courierLocation = rule->courierLocation;
            if (rule->kind == WorkshopKind::kProxy) {
                artisanReference = rule->primaryReference;
                artisanBase = rule->primaryBase;
                fallbackName = rule->primaryName;
            } else if (rule->kind == WorkshopKind::kShared) {
                const bool preferredIsMember =
                    a_preferredSmithReference == rule->primaryReference ||
                    a_preferredSmithReference == rule->secondaryReference;
                if (!preferredIsMember || IsDead(a_preferredSmithReference)) {
                    artisanReference = !IsDead(rule->primaryReference) ?
                        rule->primaryReference :
                        rule->secondaryReference;
                }
                const bool primary =
                    artisanReference == rule->primaryReference;
                artisanBase = primary ?
                    rule->primaryBase :
                    rule->secondaryBase;
                fallbackName = primary ?
                    rule->primaryName :
                    rule->secondaryName;
            } else if (a_workshopKey == rule->secondaryBase) {
                artisanReference = rule->secondaryReference;
                artisanBase = rule->secondaryBase;
                fallbackName = rule->secondaryName;
            } else {
                artisanReference = rule->primaryReference;
                artisanBase = rule->primaryBase;
                fallbackName = rule->primaryName;
            }
        }

        const auto actor = ActorReference(artisanReference);
        if (!actor || actor->IsDead()) {
            return std::nullopt;
        }
        std::string displayName = actor->GetDisplayFullName();
        if (displayName.empty()) {
            displayName = std::string{ fallbackName };
        }
        if (courierLocation.empty()) {
            courierLocation = CourierLocationFor(
                actor,
                artisanBase,
                FormResolver::GetSingleton());
        }
        return WorkshopArtisan{
            artisanReference,
            artisanBase,
            std::move(displayName),
            std::move(courierLocation)
        };
    }

    std::optional<SmithContext> SmithClassifier::Classify(RE::Actor* a_actor) const
    {
        if (!config_ || !config_->serviceEnabled || !a_actor || a_actor->IsPlayerRef() || a_actor->IsDead()) {
            return std::nullopt;
        }

        const auto base = a_actor->GetActorBase();
        if (!base) {
            return std::nullopt;
        }

        const auto& resolver = FormResolver::GetSingleton();
        const auto formID = base->GetFormID();
        const auto workshop = WorkshopForBase(formID);
        if (workshop) {
            if (workshop->kind == WorkshopKind::kProxy &&
                formID == workshop->secondaryBase &&
                IsDead(workshop->primaryReference)) {
                return std::nullopt;
            }
            if (workshop->kind == WorkshopKind::kSuccessor &&
                formID == workshop->secondaryBase &&
                !IsDead(workshop->primaryReference)) {
                return std::nullopt;
            }
        }
        const auto tierOverride = resolver.NpcTiers().find(formID);
        const bool explicitlyConfigured = tierOverride != resolver.NpcTiers().end();
        const bool inBlacksmithFaction =
            resolver.BlacksmithFaction() && a_actor->IsInFaction(resolver.BlacksmithFaction());

        if (!explicitlyConfigured &&
            config_->rules.requireBlacksmithFaction &&
            !inBlacksmithFaction) {
            return std::nullopt;
        }

        SmithContext result;
        result.actor = a_actor->GetHandle();
        result.baseFormID = formID;
        result.workshopKey = WorkshopKeyForBase(formID);
        const auto origin = base->GetFile(0);
        const bool modAdded = origin && !IsOfficialEquipmentPlugin(origin->GetFilename());
        result.tier = explicitlyConfigured ?
            tierOverride->second :
            (modAdded ? config_->rules.modAddedSmithTier : config_->rules.defaultSmithTier);
        if (modAdded && !explicitlyConfigured) {
            const std::scoped_lock lock{ loggedMutex_ };
            if (loggedModArtisans_.insert(formID).second) {
                // Ready to paste into ArtisansOfSkyrim_Custom.toml for a per-NPC tier.
                SKSE::log::info(
                    "Mod-added blacksmith detected ({}): [[npcs.overrides]] name = \"{}\" form = \"{}|{:06X}\" tier = \"{}\"",
                    ToString(result.tier),
                    a_actor->GetDisplayFullName(),
                    origin->GetFilename(),
                    base->GetLocalFormID(),
                    ToString(result.tier));
            }
        }
        result.isEorlund = formID == resolver.EorlundFormID();
        result.isBaldor = formID == resolver.BaldorFormID();
        result.isGunmar = formID == resolver.GunmarFormID();
        result.isGlover = formID == resolver.GloverFormID();
        result.isSolstheimSmith = result.isBaldor || result.isGlover;
        result.displayName = a_actor->GetDisplayFullName();
        if (result.displayName.empty()) {
            result.displayName = "Blacksmith";
        }
        if (const auto artisan = ArtisanForWorkshop(
                result.workshopKey,
                a_actor->GetFormID())) {
            result.artisanReferenceFormID = artisan->referenceFormID;
            result.artisanDisplayName = artisan->displayName;
            result.courierLocation = artisan->courierLocation;
        } else {
            result.artisanReferenceFormID = a_actor->GetFormID();
            result.artisanDisplayName = result.displayName;
            result.courierLocation = CourierLocationFor(
                a_actor,
                formID,
                resolver);
        }

        if (result.isEorlund && config_->rules.eorlundRequiresTrustQuest) {
            const bool trustedByCompanions = QuestCompleted(resolver.EorlundProvingHonorQuest());
            const bool trustedByGrayManes = QuestCompleted(resolver.EorlundMissingInActionQuest());
            result.servicesUnlocked = trustedByCompanions || trustedByGrayManes;
            if (!result.servicesUnlocked) {
                result.lockedReason =
                    "Eorlund does not trust you enough to offer the services of the Skyforge yet.";
            }
        }

        if (result.isGunmar && config_->rules.gunmarRequiresRecruitmentQuest) {
            result.servicesUnlocked = QuestCompleted(resolver.GunmarRecruitmentQuest());
            if (!result.servicesUnlocked) {
                result.lockedReason =
                    "Gunmar is not settled at Fort Dawnguard yet.";
            }
        }

        if (result.isBaldor && config_->rules.baldorRequiresQuest) {
            result.servicesUnlocked = QuestCompleted(resolver.BaldorQuest());
            if (!result.servicesUnlocked) {
                result.lockedReason =
                    "Baldor does not trust you enough to offer the services of his forge yet.";
            }
        }

        return result;
    }
}
