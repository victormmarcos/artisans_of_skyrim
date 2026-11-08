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

#include "NPCSmithServices/InventoryService.h"
#include "ArtisansOfSkyrim/Localization.h"

#include "NPCSmithServices/EquipmentOrigin.h"
#include "NPCSmithServices/FormResolver.h"
#include "NPCSmithServices/OrderService.h"
#include "NPCSmithServices/RecipeIndex.h"

#include "ArtisansOfSkyrim/ServiceDiscount.h"
#include "ArtisansOfSkyrim/TransactionFeedback.h"

#include <algorithm>
#include <array>
#include <utility>

namespace
{
    [[nodiscard]] bool UsesPost629ExtraDataLayout()
    {
        return REL::Module::IsAE() &&
            REL::Module::get().version().compare(SKSE::RUNTIME_SSE_1_6_629) !=
                std::strong_ordering::less;
    }

    [[nodiscard]] RE::ExtraDataList* CreateEmptyExtraDataList(
        const RE::TESObjectREFR& a_vtableSource)
    {
        constexpr std::size_t legacySize = 0x18;
        constexpr std::size_t post629Size = 0x20;

        const auto post629 = UsesPost629ExtraDataLayout();
        auto* storage = RE::calloc(1, post629 ? post629Size : legacySize);
        if (!storage) {
            return nullptr;
        }

        // BaseExtraList gained a vtable in Skyrim 1.6.629. Copy the engine's
        // vtable pointer from a live reference while keeping all data fields
        // and the lock zero-initialized.
        if (post629) {
            std::memcpy(
                storage,
                std::addressof(a_vtableSource.extraList),
                sizeof(void*));
        }

        return static_cast<RE::ExtraDataList*>(storage);
    }

    void DestroyUnownedExtraDataList(RE::ExtraDataList* a_extraList)
    {
        if (!a_extraList) {
            return;
        }

        auto* base = reinterpret_cast<RE::BaseExtraList*>(a_extraList);
        while (base->GetData()) {
            auto* extra = base->GetData();
            base->GetData() = extra->next;
            delete extra;
        }
        RE::free(base->GetPresence());
        base->GetPresence() = nullptr;
        RE::free(a_extraList);
    }

    [[nodiscard]] double CurrentHealth(const RE::ExtraDataList* a_extraList)
    {
        if (const auto health = a_extraList ? a_extraList->GetByType<RE::ExtraHealth>() : nullptr) {
            return health->health;
        }
        return 1.0;
    }

    [[nodiscard]] bool IsWorn(const RE::ExtraDataList* a_extraList)
    {
        return a_extraList &&
            (a_extraList->HasType<RE::ExtraWorn>() ||
             a_extraList->HasType<RE::ExtraWornLeft>());
    }

    [[nodiscard]] bool TryRemoveGold(
        RE::PlayerCharacter* a_player,
        RE::TESBoundObject* a_gold,
        std::int32_t a_price)
    {
        if (!a_player || !a_gold) {
            return false;
        }
        if (a_price <= 0) {
            return true;
        }
        if (a_player->GetItemCount(a_gold) < a_price) {
            return false;
        }

        const auto before = a_player->GetItemCount(a_gold);
        a_player->RemoveItem(
            a_gold,
            a_price,
            RE::ITEM_REMOVE_REASON::kRemove,
            nullptr,
            nullptr);
        const auto after = a_player->GetItemCount(a_gold);
        return before - after == a_price;
    }

    void RefundGold(
        RE::PlayerCharacter* a_player,
        RE::TESBoundObject* a_gold,
        std::int32_t a_price)
    {
        if (a_player && a_gold && a_price > 0) {
            a_player->AddObjectToContainer(a_gold, nullptr, a_price, nullptr);
        }
    }

    [[nodiscard]] bool IsCompleted(RE::TESQuest* a_quest)
    {
        return a_quest && a_quest->IsCompleted();
    }

    [[nodiscard]] bool IsDragonGear(const NPCSS::Config& a_config, const RE::TESBoundObject* a_object)
    {
        return NPCSS::FormResolver::HasTextToken(a_object, a_config.dragonGearTokens);
    }

    [[nodiscard]] bool IsDawnguardGear(const NPCSS::Config& a_config, const RE::TESBoundObject* a_object)
    {
        return NPCSS::FormResolver::HasTextToken(a_object, a_config.dawnguardGearTokens);
    }

    [[nodiscard]] bool IsEnhancedCrossbowGear(const NPCSS::Config& a_config, const RE::TESBoundObject* a_object)
    {
        return NPCSS::FormResolver::HasTextToken(a_object, a_config.enhancedCrossbowTokens);
    }

    [[nodiscard]] bool IsSolstheimGear(const NPCSS::Config& a_config, const RE::TESBoundObject* a_object)
    {
        return NPCSS::FormResolver::HasTextToken(a_object, a_config.solstheimGearTokens);
    }
    [[nodiscard]] std::int32_t DiscountedServicePrice(
        std::int32_t a_price,
        const NPCSS::Config& a_config)
    {
        return NPCSS::ApplyPriceDiscount(
            a_price,
            AOS::PlayerServiceDiscount(a_config.barterPricing));
    }

}

namespace NPCSS
{
    InventoryService& InventoryService::GetSingleton()
    {
        static InventoryService singleton;
        return singleton;
    }

    void InventoryService::Configure(const Config& a_config)
    {
        config_ = std::addressof(a_config);
    }

    std::vector<ImprovementItem> InventoryService::ListImprovementItems(
        const SmithContext& a_smith,
        TemperQuality a_targetQuality) const
    {
        std::vector<ImprovementItem> result;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!config_ || !player || !a_smith.servicesUnlocked ||
            !CanOfferQuality(a_smith.tier, a_targetQuality)) {
            return result;
        }

        const auto targetHealth = TargetHealth(a_targetQuality, config_->temperTargets);
        std::size_t blockedCount = 0;
        std::unordered_map<std::string, std::size_t> blockedReasons;
        std::vector<std::string> notes;
        std::vector<std::string> masterOnly;  // names, for checking unique-item detection
        const auto inventory = player->GetInventory([](RE::TESBoundObject& a_object) {
            return a_object.Is(RE::FormType::Weapon) || a_object.Is(RE::FormType::Armor);
        });

        for (const auto& [object, inventoryData] : inventory) {
            const auto totalCount = inventoryData.first;
            const auto& entryData = inventoryData.second;
            if (!object || !entryData || totalCount <= 0) {
                continue;
            }
            const bool questObject = entryData->IsQuestObject();

            // One row per physical item. Some entries carry more data records
            // than items (seen with unique weapons); records beyond the real
            // count are ignored so an item is never listed twice. Records with
            // tempering data are kept first.
            std::vector<RE::ExtraDataList*> instances;
            if (entryData->extraLists) {
                for (const auto extraList : *entryData->extraLists) {
                    if (extraList) instances.push_back(extraList);
                }
            }
            std::ranges::stable_sort(instances, [](RE::ExtraDataList* a_lhs, RE::ExtraDataList* a_rhs) {
                return a_lhs->HasType<RE::ExtraHealth>() && !a_rhs->HasType<RE::ExtraHealth>();
            });
            std::int32_t recordedCount = 0;
            for (const auto extraList : instances) recordedCount += std::max(extraList->GetCount(), 1);
            if (recordedCount > totalCount) {
                notes.push_back(std::format(
                    "Improvement list: {:08X} '{}' has {} data record(s) for {} item(s); extra records ignored.",
                    object->GetFormID(),
                    DisplayName(object, nullptr),
                    instances.size(),
                    totalCount));
            }

            std::int32_t describedCount = 0;
            {
                for (const auto extraList : instances) {
                    if (describedCount >= totalCount) {
                        break;
                    }
                    describedCount += std::max(extraList->GetCount(), 1);
                    const auto health = CurrentHealth(extraList);
                    const auto reason = BlockReason(
                        a_smith,
                        object,
                        extraList,
                        health,
                        questObject,
                        targetHealth);
                    if (!reason.empty()) {
                        ++blockedCount;
                        ++blockedReasons[reason];
                        if (reason == "special item requires a Master") masterOnly.push_back(DisplayName(object, extraList));
                        SKSE::log::debug(
                            "Blocked improvement {:08X} '{}': {}",
                            object->GetFormID(),
                            DisplayName(object, extraList),
                            reason);
                        continue;
                    }

                    result.push_back(ImprovementItem{
                        object,
                        extraList,
                        DisplayName(object, extraList),
                        health,
                        targetHealth,
                        QualityFromHealth(health),
                        a_targetQuality,
                        DiscountedServicePrice(
                            ImprovementPrice(
                                object->GetGoldValue(),
                                a_targetQuality,
                                ClassifyServiceComplexity(object),
                                config_->improvementPricing),
                            *config_),
                        false
                    });
                }
            }

            const auto plainCount = std::max(totalCount - describedCount, 0);
            if (plainCount > 0) {
                const auto reason = BlockReason(
                    a_smith,
                    object,
                    nullptr,
                    1.0,
                    questObject,
                    targetHealth);
                if (!reason.empty()) {
                    ++blockedCount;
                    ++blockedReasons[reason];
                    if (reason == "special item requires a Master") masterOnly.push_back(DisplayName(object, nullptr));
                    SKSE::log::debug(
                        "Blocked plain improvement {:08X} '{}': {}",
                        object->GetFormID(),
                        DisplayName(object, nullptr),
                        reason);
                    continue;
                }

                auto name = DisplayName(object, nullptr);
                if (plainCount > 1) {
                    name += " (x" + std::to_string(plainCount) + ')';
                }
                result.push_back(ImprovementItem{
                    object,
                    nullptr,
                    std::move(name),
                    1.0,
                    targetHealth,
                    TemperQuality::kUnimproved,
                    a_targetQuality,
                    DiscountedServicePrice(
                            ImprovementPrice(
                                object->GetGoldValue(),
                                a_targetQuality,
                                ClassifyServiceComplexity(object),
                                config_->improvementPricing),
                            *config_),
                    true
                });
            }
        }

        std::ranges::sort(result, [](const ImprovementItem& a_lhs, const ImprovementItem& a_rhs) {
            if (a_lhs.displayName != a_rhs.displayName) {
                return a_lhs.displayName < a_rhs.displayName;
            }
            return a_lhs.object->GetFormID() < a_rhs.object->GetFormID();
        });

        // The list is built once per quality while browsing; log it only when
        // something changed, so the log stays readable.
        std::vector<std::string> lines = std::move(notes);
        lines.push_back(std::format(
            "Improvement list for {} ({}): {} eligible, {} blocked",
            a_smith.displayName,
            ToString(a_smith.tier),
            result.size(),
            blockedCount));
        for (const auto& [reason, count] : blockedReasons) {
            lines.push_back(std::format("Improvement blocked by '{}': {}", reason, count));
        }
        if (!masterOnly.empty()) {
            std::ranges::sort(masterOnly);
            std::string names;
            for (const auto& name : masterOnly) {
                if (!names.empty()) names += ", ";
                names += name;
            }
            lines.push_back("Unique items (Master only): " + names);
        }
        std::string summary;
        for (const auto& line : lines) summary += line + '\n';
        {
            const std::scoped_lock lock{ logMutex_ };
            if (summary != lastImprovementLog_) {
                lastImprovementLog_ = summary;
                for (const auto& line : lines) SKSE::log::info("{}", line);
            }
        }
        return result;
    }

    TransactionResult InventoryService::ApplyImprovement(
        const SmithContext& a_smith,
        const ImprovementItem& a_item) const
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!config_ || !player || !a_item.object ||
            !CanOfferQuality(a_smith.tier, a_item.targetQuality)) {
            return { false, "The service is not available." };
        }

        const auto actor = a_smith.actor.get();
        if (!actor || !a_smith.servicesUnlocked) {
            return { false, "The blacksmith is no longer available." };
        }

        const auto currentInventory = player->GetInventory();
        const auto inventoryEntry = currentInventory.find(a_item.object);
        if (inventoryEntry == currentInventory.end() ||
            inventoryEntry->second.first <= 0 ||
            !inventoryEntry->second.second) {
            return { false, "The inventory changed before the service could be applied." };
        }
        const auto& entryData = inventoryEntry->second.second;
        if (a_item.extraList) {
            const bool stillPresent = entryData->extraLists &&
                std::ranges::find(*entryData->extraLists, a_item.extraList) !=
                    entryData->extraLists->end();
            if (!stillPresent) {
                return { false, "The inventory changed before the service could be applied." };
            }
        } else {
            std::int32_t describedCount = 0;
            if (entryData->extraLists) {
                for (const auto extraList : *entryData->extraLists) {
                    describedCount += extraList ?
                        std::max(extraList->GetCount(), 1) :
                        0;
                }
            }
            if (inventoryEntry->second.first - describedCount <= 0) {
                return { false, "The inventory changed before the service could be applied." };
            }
        }

        const auto currentHealth = CurrentHealth(a_item.extraList);
        const auto targetHealth = TargetHealth(
            a_item.targetQuality,
            config_->temperTargets);
        const auto reason = BlockReason(
            a_smith,
            a_item.object,
            a_item.extraList,
            currentHealth,
            entryData->IsQuestObject(),
            targetHealth);
        if (!reason.empty()) {
            return { false, AOS::Text::Format("This item is no longer eligible: {}", AOS::Text::Localize(reason)) };
        }

        const auto price = DiscountedServicePrice(
            ImprovementPrice(
                a_item.object->GetGoldValue(),
                a_item.targetQuality,
                ClassifyServiceComplexity(a_item.object),
                config_->improvementPricing),
            *config_);
        const auto gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        if (!gold) {
            return { false, "Gold form 0000000F was not found." };
        }
        if (player->GetItemCount(gold) < price) {
            return { false, "You do not have enough septims." };
        }
        if (!TryRemoveGold(player, gold, price)) {
            return { false, "The payment could not be completed." };
        }

        const auto target = static_cast<float>(targetHealth);
        if (a_item.extraList) {
            if (a_item.extraList->GetCount() > 1) {
                RefundGold(player, gold, price);
                return { false, "Stacked item instances are not modified in-place." };
            }
            if (auto health = a_item.extraList->GetByType<RE::ExtraHealth>()) {
                health->health = target;
            } else {
                a_item.extraList->Add(new RE::ExtraHealth(target));
            }
            player->AddChange(RE::TESObjectREFR::ChangeFlags::kItemExtraData);
        } else {
            auto* extraList = CreateEmptyExtraDataList(*player);
            if (!extraList) {
                RefundGold(player, gold, price);
                return { false, "Could not allocate item instance data." };
            }
            extraList->Add(new RE::ExtraHealth(target));

            const auto countBefore = player->GetItemCount(a_item.object);
            player->RemoveItem(
                a_item.object,
                1,
                RE::ITEM_REMOVE_REASON::kRemove,
                nullptr,
                nullptr);
            const auto countAfter = player->GetItemCount(a_item.object);
            if (countBefore - countAfter != 1) {
                DestroyUnownedExtraDataList(extraList);
                RefundGold(player, gold, price);
                return { false, "The inventory changed before the service could be applied." };
            }

            player->AddObjectToContainer(a_item.object, extraList, 1, nullptr);
            player->AddChange(
                RE::TESObjectREFR::ChangeFlags::kItemExtraData);
        }


        AOS::PlayPurchaseSound();
        SKSE::log::info(
            "Applied {} to {:08X} '{}' for {} septims",
            ToString(a_item.targetQuality),
            a_item.object->GetFormID(),
            a_item.displayName,
            price);
        return {
            true,
            AOS::Text::Format("{} improved to {}.", a_item.displayName,
                AOS::Text::Localize(ToString(a_item.targetQuality)))
        };
    }

    TransactionResult InventoryService::ApplyCrafting(
        const SmithContext& a_smith,
        const CraftableItem& a_item) const
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto actor = a_smith.actor.get();
        if (!config_ || !player || !actor || !a_smith.servicesUnlocked ||
            !a_item.object || a_item.outputCount <= 0) {
            return { false, "The crafting service is not available." };
        }

        const auto available = RecipeIndex::GetSingleton().ListCraftable(a_smith);
        const auto current = std::ranges::find_if(
            available,
            [&](const CraftableItem& a_candidate) {
                return a_candidate.object == a_item.object &&
                    a_candidate.recipe == a_item.recipe;
            });
        if (current == available.end() ||
            current->price <= 0 ||
            current->outputCount <= 0) {
            return { false, "This item is no longer available from this blacksmith." };
        }

        if (config_->orders.enabled) {
            const auto category =
                ClassifyCraftingOrderCategory(current->object);
            if (!category) {
                return {
                    false,
                    "This item has no supported crafting-time category."
                };
            }
            if (!IsInstantOrderCategory(*category)) {
                return OrderService::GetSingleton().PlaceOrder(
                    a_smith,
                    *current);
            }
        }

        // Always use the freshly rebuilt candidate. This preserves fixed batch
        // prices for arrows/bolts and revalidates the current discount, output
        // count and progression rules at the moment payment is taken.
        const auto price = current->price;
        const auto outputCount = current->outputCount;
        const auto gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        if (!gold) {
            return { false, "Gold form 0000000F was not found." };
        }
        if (player->GetItemCount(gold) < price) {
            return { false, "You do not have enough septims." };
        }

        if (!TryRemoveGold(player, gold, price)) {
            return { false, "The payment could not be completed." };
        }

        const auto countBefore = player->GetItemCount(current->object);
        player->AddObjectToContainer(
            current->object,
            nullptr,
            outputCount,
            nullptr);
        const auto countAfter = player->GetItemCount(current->object);
        if (countAfter - countBefore < outputCount) {
            RefundGold(player, gold, price);
            return { false, "The crafted item could not be delivered." };
        }

        const auto name = DisplayName(current->object, nullptr);
        AOS::PlayPurchaseSound();
        SKSE::log::info(
            "Crafted {}x {:08X} '{}' for {} septims",
            outputCount,
            current->object->GetFormID(),
            name,
            price);
        return {
            true,
            AOS::Text::Format("{} crafted (x{}).", name, outputCount)
        };
    }

    std::string InventoryService::BlockReason(
        const SmithContext& a_smith,
        RE::TESBoundObject* a_object,
        RE::ExtraDataList* a_extraList,
        double a_currentHealth,
        bool a_isQuestObject,
        double a_targetHealth) const
    {
        if (!config_ || !a_object) {
            return "invalid item";
        }
        if (!a_object->GetPlayable()) {
            return "non-playable";
        }
        if (!a_object->Is(RE::FormType::Weapon) && !a_object->Is(RE::FormType::Armor)) {
            return "unsupported category";
        }
        const auto& recipes = RecipeIndex::GetSingleton();
        if (!recipes.IsAllowedImprovementCategory(a_object)) {
            return "unsupported equipment category";
        }
        if (!IsAllowedEquipmentOrigin(
                a_object,
                config_->rules.includeModAddedEquipment)) {
            return "mod-added equipment is disabled by the installed preset";
        }

        const auto& resolver = FormResolver::GetSingleton();
        if (resolver.IsBlocked(a_object->GetFormID())) {
            return "blocked by configuration";
        }
        if (resolver.IsAncientNordWeapon(a_object) &&
            !resolver.IsNordHero(a_object->GetFormID()) &&
            !resolver.IsAllowed(a_object->GetFormID())) {
            return "Ancient Nord weapons are excluded";
        }
        const bool skyforgeSpecial =
            resolver.IsNordHero(a_object->GetFormID()) ||
            resolver.IsAncientNordArmor(a_object->GetFormID());
        if (skyforgeSpecial && a_smith.tier != SmithTier::kMaster) {
            return "Skyforge special gear requires a Master";
        }
        if (skyforgeSpecial && !a_smith.isEorlund) {
            return "Skyforge special gear requires Eorlund";
        }
        if (skyforgeSpecial && !IsCompleted(resolver.SkyforgeUnlockQuest())) {
            return "Skyforge special gear requires Glory of the Dead";
        }
        if (a_isQuestObject) {
            return "quest item";
        }
        if (IsWorn(a_extraList) && !config_->rules.allowEquippedItems) {
            return "equipped item";
        }
        const bool enchanted = IsBaseEnchanted(a_object) ||
            IsInstanceEnchanted(a_extraList);
        if (enchanted && !config_->rules.allowEnchantedItems) {
            return "enchanted items are disabled";
        }
        if (enchanted &&
            config_->rules.enchantedItemsRequireExpert &&
            a_smith.tier == SmithTier::kAdept) {
            return "enchanted item requires an Expert or Master";
        }
        if (FormResolver::IsStalhrim(a_object) &&
            config_->rules.stalhrimOnlyBaldorForImprovement &&
            !a_smith.isBaldor) {
            return "Stalhrim requires Baldor";
        }

        if (config_->rules.dragonGearRequiresMaster &&
            IsDragonGear(*config_, a_object) &&
            a_smith.tier != SmithTier::kMaster) {
            return "Dragon gear requires a Master";
        }
        if (config_->rules.dragonGearRequiresWayOfVoice &&
            IsDragonGear(*config_, a_object) &&
            !IsCompleted(resolver.WayOfVoiceQuest())) {
            return "Dragon gear requires The Way of the Voice";
        }
        if (config_->rules.dawnguardGearOnlyGunmar &&
            IsDawnguardGear(*config_, a_object) &&
            !a_smith.isGunmar) {
            return "Dawnguard gear requires Gunmar";
        }
        if (config_->rules.enhancedCrossbowsRequireAncientTechnology &&
            IsEnhancedCrossbowGear(*config_, a_object) &&
            !IsCompleted(resolver.AncientTechnologyQuest())) {
            return "enhanced crossbow gear requires Ancient Technology";
        }
        if (config_->rules.solstheimGearOnlySolstheimSmiths &&
            IsSolstheimGear(*config_, a_object) &&
            !a_smith.isSolstheimSmith) {
            return "Solstheim gear requires a Solstheim smith";
        }

        const auto special =
            resolver.IsSpecial(a_object->GetFormID()) ||
            FormResolver::HasKeywordToken(a_object, config_->artifactKeywordTokens) ||
            RecipeIndex::GetSingleton().IsUnique(a_object);
        if (special &&
            a_smith.tier != SmithTier::kMaster &&
            (config_->rules.uniqueItemsRequireMaster || config_->rules.artifactsRequireMaster)) {
            return "special item requires a Master";
        }

        const bool explicitAllow = resolver.IsAllowed(a_object->GetFormID());
        const bool hasRecipe = recipes.HasTemperingRecipe(a_object->GetFormID());
        const bool recognizedMaterial = recipes.HasRecognizedMaterial(a_object);
        const bool unknownFallback =
            config_->rules.allowUnknownMaterialFallbackForExpertOrMaster &&
            a_smith.tier != SmithTier::kAdept;
        if (config_->rules.requireTemperingRecipe &&
            !explicitAllow &&
            !hasRecipe &&
            !(config_->rules.allowRecognizedKeywordFallback && recognizedMaterial) &&
            !unknownFallback) {
            return "no tempering recipe or safe fallback";
        }

        constexpr double epsilon = 0.0005;
        if (a_currentHealth + epsilon >= a_targetHealth) {
            return "already at or above the selected quality";
        }
        return {};
    }

    bool InventoryService::IsBaseEnchanted(const RE::TESBoundObject* a_object)
    {
        const auto enchantable = a_object ? a_object->As<RE::TESEnchantableForm>() : nullptr;
        return enchantable && enchantable->formEnchanting;
    }

    bool InventoryService::IsInstanceEnchanted(
        RE::ExtraDataList* a_extraList)
    {
        return a_extraList &&
            a_extraList->HasType<RE::ExtraEnchantment>();
    }

    std::string InventoryService::DisplayName(
        RE::TESBoundObject* a_object,
        RE::ExtraDataList* a_extraList)
    {
        const char* name = nullptr;
        if (a_extraList) {
            name = a_extraList->GetDisplayName(a_object);
        }
        if (!name || name[0] == '\0') {
            name = a_object ? a_object->GetName() : nullptr;
        }
        return name && name[0] != '\0' ? name : "<unnamed item>";
    }
}
