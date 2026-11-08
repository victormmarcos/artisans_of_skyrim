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

#include "NPCEnchantingServices/InventoryService.h"

#include "ArtisansOfSkyrim/ServiceDiscount.h"
#include "ArtisansOfSkyrim/TransactionFeedback.h"

#include "NPCEnchantingServices/FormResolver.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <compare>
#include <cstring>

namespace
{
    struct IntendedEffect
    {
        RE::EffectSetting* baseEffect{ nullptr };
        RE::Effect::EffectItem effectItem;
        float cost{ 0.0F };
    };

    [[nodiscard]] std::vector<IntendedEffect> CaptureIntendedEffects(
        const RE::BSTArray<RE::Effect>& a_effects)
    {
        std::vector<IntendedEffect> result;
        result.reserve(a_effects.size());
        for (const auto& effect : a_effects) {
            result.push_back({
                effect.baseEffect,
                effect.effectItem,
                effect.cost
            });
        }
        return result;
    }

    [[nodiscard]] bool RestoreIntendedEffectValues(
        RE::EnchantmentItem* a_created,
        std::span<const IntendedEffect> a_intended)
    {
        if (!a_created ||
            a_created->effects.size() != a_intended.size()) {
            SKSE::log::error(
                "Created enchantment effect count differs from the "
                "validated table effect count ({} versus {}).",
                a_created ? a_created->effects.size() : 0,
                a_intended.size());
            return false;
        }

        for (std::size_t index = 0; index < a_intended.size(); ++index) {
            const auto effect = a_created->effects[
                static_cast<std::uint32_t>(index)];
            if (!effect || !effect->baseEffect ||
                effect->baseEffect != a_intended[index].baseEffect) {
                SKSE::log::error(
                    "Created enchantment effect {} does not match the "
                    "validated source effect.",
                    index);
                return false;
            }
        }

        for (std::size_t index = 0; index < a_intended.size(); ++index) {
            const auto effect = a_created->effects[
                static_cast<std::uint32_t>(index)];
            const auto previousMagnitude =
                effect->effectItem.magnitude;
            const auto previousDuration =
                effect->effectItem.duration;
            const auto previousCost = effect->cost;

            effect->effectItem = a_intended[index].effectItem;
            effect->cost = a_intended[index].cost;

            if (previousMagnitude != effect->effectItem.magnitude ||
                previousDuration != effect->effectItem.duration ||
                previousCost != effect->cost) {
                SKSE::log::info(
                    "Normalized created effect {} from magnitude "
                    "{:.3f}/duration {}/cost {:.3f} to table magnitude "
                    "{:.3f}/duration {}/cost {:.3f}.",
                    index,
                    previousMagnitude,
                    previousDuration,
                    previousCost,
                    effect->effectItem.magnitude,
                    effect->effectItem.duration,
                    effect->cost);
            }
        }
        return true;
    }

    [[nodiscard]] bool UsesPost629ExtraDataLayout()
    {
        return REL::Module::IsAE() &&
            REL::Module::get().version().compare(
                SKSE::RUNTIME_SSE_1_6_629) !=
                std::strong_ordering::less;
    }

    [[nodiscard]] RE::ExtraDataList* CreateEmptyExtraDataList(
        const RE::TESObjectREFR& a_vtableSource)
    {
        // Do not call RE::ExtraDataList's constructor directly here.
        // Some CommonLibSSE-NG builds declare it but do not provide a linkable
        // definition, which causes LNK2019/LNK1120 during DLL linking.
        //
        // Allocate the runtime-sized storage and, for the post-1.6.629 layout,
        // copy the vtable pointer from a known live reference extra list.
        constexpr std::size_t legacySize = 0x18;
        constexpr std::size_t post629Size = 0x20;

        const auto post629 = UsesPost629ExtraDataLayout();
        auto* storage = RE::calloc(
            1,
            post629 ? post629Size : legacySize);
        if (!storage) {
            return nullptr;
        }

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
            a_player->AddObjectToContainer(
                a_gold,
                nullptr,
                a_price,
                nullptr);
        }
    }

    [[nodiscard]] std::string NormalizeCustomName(std::string_view a_name)
    {
        std::string result;
        result.reserve(std::min<std::size_t>(a_name.size(), 255));
        for (const auto raw : a_name) {
            const auto value = static_cast<unsigned char>(raw);
            if (value >= 0x20 && value != 0x7F) {
                result.push_back(raw);
            }
        }
        while (!result.empty() &&
            std::isspace(static_cast<unsigned char>(result.front()))) {
            result.erase(result.begin());
        }
        while (!result.empty() &&
            std::isspace(static_cast<unsigned char>(result.back()))) {
            result.pop_back();
        }
        if (result.size() > 255) {
            auto cut = std::size_t{ 255 };
            while (cut > 0 &&
                (static_cast<unsigned char>(result[cut]) & 0xC0) == 0x80) {
                --cut;
            }
            result.resize(cut);
        }
        return result;
    }

}

namespace NPCES
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

    std::vector<EnchantableItem> InventoryService::ListItems(
        const EnchanterContext& a_enchanter) const
    {
        std::vector<EnchantableItem> result;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!config_ || !player || !a_enchanter.servicesUnlocked) {
            return result;
        }

        const auto inventory = player->GetInventory(
            [](RE::TESBoundObject& a_object) {
                return a_object.Is(RE::FormType::Weapon) ||
                    a_object.Is(RE::FormType::Armor);
            });

        for (const auto& [object, inventoryData] : inventory) {
            const auto totalCount = inventoryData.first;
            const auto& entryData = inventoryData.second;
            if (!object || !entryData || totalCount <= 0) {
                continue;
            }
            const bool questObject = entryData->IsQuestObject();

            std::int32_t describedCount = 0;
            bool hasAnyExtraLists = false;
            if (entryData->extraLists) {
                for (const auto extraList : *entryData->extraLists) {
                    if (!extraList) {
                        continue;
                    }
                    hasAnyExtraLists = true;
                    describedCount += std::max(extraList->GetCount(), 1);
                    const auto reason = BlockReason(
                        object,
                        extraList,
                        false,
                        true,
                        questObject);
                    if (!reason.empty()) {
                        SKSE::log::debug(
                            "Blocked item {:08X} '{}': {}",
                            object->GetFormID(),
                            DisplayName(object, extraList),
                            reason);
                        continue;
                    }

                    const auto category = CategoryFor(object);
                    if (EnchantmentCatalog::GetSingleton()
                            .ListValid(
                                object,
                                category,
                                a_enchanter.tier)
                            .empty()) {
                        continue;
                    }
                    auto currentName = DisplayName(object, extraList);
                    result.push_back({
                        object,
                        extraList,
                        currentName,
                        std::move(currentName),
                        category,
                        false
                    });
                }
            }

            const auto plainCount = std::max(
                totalCount - describedCount,
                0);
            if (plainCount > 0) {
                const auto reason = BlockReason(
                    object,
                    nullptr,
                    true,
                    hasAnyExtraLists,
                    questObject);
                if (!reason.empty()) {
                    SKSE::log::debug(
                        "Blocked plain item {:08X} '{}': {}",
                        object->GetFormID(),
                        DisplayName(object, nullptr),
                        reason);
                    continue;
                }

                const auto category = CategoryFor(object);
                if (EnchantmentCatalog::GetSingleton()
                        .ListValid(
                            object,
                            category,
                            a_enchanter.tier)
                        .empty()) {
                    continue;
                }

                auto currentName = DisplayName(object, nullptr);
                auto name = currentName;
                if (plainCount > 1) {
                    name += " (x" + std::to_string(plainCount) + ')';
                }
                result.push_back({
                    object,
                    nullptr,
                    std::move(name),
                    std::move(currentName),
                    category,
                    true
                });
            }
        }

        std::ranges::sort(
            result,
            [](const EnchantableItem& a_lhs, const EnchantableItem& a_rhs) {
                if (a_lhs.displayName != a_rhs.displayName) {
                    return a_lhs.displayName < a_rhs.displayName;
                }
                return a_lhs.object->GetFormID() <
                    a_rhs.object->GetFormID();
            });
        return result;
    }

    std::vector<const ResolvedEnchantment*>
    InventoryService::ListEnchantments(
        const EnchanterContext& a_enchanter,
        const EnchantableItem& a_item) const
    {
        return EnchantmentCatalog::GetSingleton().ListValid(
            a_item.object,
            a_item.category,
            a_enchanter.tier);
    }

    std::int32_t InventoryService::Price(
        const EnchanterContext& a_enchanter,
        std::span<const ResolvedEnchantment* const> a_enchantments) const
    {
        if (!config_ || a_enchantments.empty() ||
            a_enchantments.size() > 2) {
            return 0;
        }
        if (a_enchantments.size() == 2 &&
            !CanUseDual(a_enchanter.tier)) {
            return 0;
        }

        const auto& catalog = EnchantmentCatalog::GetSingleton();
        const auto first = a_enchantments[0];
        if (!first) {
            return 0;
        }
        const auto firstPrice =
            catalog.ValueFor(*first, a_enchanter.tier).price;

        const auto discount = PlayerServiceDiscount();
        if (a_enchantments.size() == 1) {
            return ApplyPriceDiscount(
                SinglePrice(firstPrice, config_->pricing),
                discount);
        }

        const auto second = a_enchantments[1];
        if (!second) {
            return 0;
        }
        const auto secondPrice =
            catalog.ValueFor(*second, a_enchanter.tier).price;

        // A dual order is itemized exactly as two independently priced
        // enchantments. Keeping the transaction total equal to the two values
        // shown in the list and confirmation avoids a hidden combined price.
        const auto firstFinal = ApplyPriceDiscount(
            SinglePrice(firstPrice, config_->pricing),
            discount);
        const auto secondFinal = ApplyPriceDiscount(
            SinglePrice(secondPrice, config_->pricing),
            discount);
        return AddPrices(firstFinal, secondFinal);
    }

    TransactionResult InventoryService::ApplyEnchanting(
        const EnchanterContext& a_enchanter,
        const EnchantableItem& a_item,
        std::span<const ResolvedEnchantment* const> a_enchantments,
        std::string_view a_customName) const
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!config_ || !player || !a_item.object ||
            a_enchantments.empty() || a_enchantments.size() > 2) {
            return { false, "The item could not be enchanted." };
        }

        const auto actor = a_enchanter.actor.get();
        const auto current = actor ?
            EnchanterClassifier::GetSingleton().Classify(actor.get()) :
            std::nullopt;
        if (!current || !current->servicesUnlocked ||
            current->baseFormID != a_enchanter.baseFormID ||
            current->tier != a_enchanter.tier) {
            return { false, "The item could not be enchanted." };
        }
        if (a_enchantments.size() == 2 &&
            !CanUseDual(a_enchanter.tier)) {
            return { false, "The item could not be enchanted." };
        }
        if (a_enchantments.size() == 2 &&
            a_enchantments[0] && a_enchantments[1] &&
            a_enchantments[0]->definition->id ==
                a_enchantments[1]->definition->id) {
            return { false, "The item could not be enchanted." };
        }

        const auto stillAvailable = ListItems(*current);
        const auto itemIsCurrent = std::ranges::any_of(
            stillAvailable,
            [&](const EnchantableItem& a_candidate) {
                return a_candidate.object == a_item.object &&
                    a_candidate.extraList == a_item.extraList &&
                    a_candidate.plainStackItem == a_item.plainStackItem;
            });
        if (!itemIsCurrent) {
            return { false, "The item could not be enchanted." };
        }

        const auto& catalog = EnchantmentCatalog::GetSingleton();
        for (const auto enchantment : a_enchantments) {
            if (!enchantment ||
                !catalog.IsValid(
                    a_item.object,
                    a_item.category,
                    a_enchanter.tier,
                    *enchantment)) {
                return { false, "The item could not be enchanted." };
            }
        }

        const auto price = Price(a_enchanter, a_enchantments);
        const auto customName = NormalizeCustomName(a_customName);
        const auto gold =
            RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        if (!gold || price <= 0) {
            return { false, "The item could not be enchanted." };
        }
        if (player->GetItemCount(gold) < price) {
            return { false, "You do not have enough gold." };
        }

        RE::BSTArray<RE::Effect> effects;
        if (!catalog.BuildEffects(
                a_enchantments,
                a_enchanter.tier,
                effects)) {
            return { false, "The item could not be enchanted." };
        }
        const auto intendedEffects = CaptureIntendedEffects(effects);

        const auto manager = RE::BGSCreatedObjectManager::GetSingleton();
        auto* created = manager ?
            (a_item.category == ItemCategory::kWeapons ?
                 manager->AddWeaponEnchantment(effects) :
                 manager->AddArmorEnchantment(effects)) :
            nullptr;
        if (!created) {
            return { false, "The item could not be enchanted." };
        }
        if (!RestoreIntendedEffectValues(created, intendedEffects)) {
            return { false, "The item could not be enchanted." };
        }

        const auto charge =
            a_item.category == ItemCategory::kWeapons ?
                config_->grandSoulWeaponCharge :
                std::uint16_t{ 0 };
        if (a_item.extraList) {
            if (a_item.extraList->GetCount() != 1 ||
                a_item.extraList->HasType<RE::ExtraEnchantment>()) {
                return { false, "The item could not be enchanted." };
            }
            auto* enchantmentData =
                new RE::ExtraEnchantment(created, charge);

            if (!TryRemoveGold(player, gold, price)) {
                delete enchantmentData;
                return { false, "The item could not be enchanted." };
            }

            a_item.extraList->Add(enchantmentData);
            if (a_item.extraList->GetByType<RE::ExtraEnchantment>() !=
                enchantmentData) {
                delete enchantmentData;
                RefundGold(player, gold, price);
                return { false, "The item could not be enchanted." };
            }
            if (!customName.empty()) {
                // This is the same vanilla inventory-name record used by the
                // enchanting table's Rename action.
                a_item.extraList->SetOverrideName(customName.c_str());
            }
            // Some perk overhauls modify created enchantment data while the
            // enchantment is attached to an inventory instance. Run the
            // normalization pass again after attachment so the stored service
            // result remains the NPC tier value rather than the player's
            // Enchanting/perk-scaled value.
            if (!RestoreIntendedEffectValues(created, intendedEffects)) {
                SKSE::log::error(
                    "Post-attach enchantment normalization failed for "
                    "existing inventory instance.");
            }
            player->AddChange(
                RE::TESObjectREFR::ChangeFlags::kItemExtraData);
        } else {
            auto* extraList = CreateEmptyExtraDataList(*player);
            if (!extraList) {
                return { false, "The item could not be enchanted." };
            }
            extraList->Add(new RE::ExtraEnchantment(created, charge));
            if (!customName.empty()) {
                extraList->SetOverrideName(customName.c_str());
            }

            if (!extraList->HasType<RE::ExtraEnchantment>()) {
                DestroyUnownedExtraDataList(extraList);
                return { false, "The item could not be enchanted." };
            }
            if (!TryRemoveGold(player, gold, price)) {
                DestroyUnownedExtraDataList(extraList);
                return { false, "The item could not be enchanted." };
            }

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
                return { false, "The item could not be enchanted." };
            }
            player->AddObjectToContainer(
                a_item.object,
                extraList,
                1,
                nullptr);
            player->AddChange(
                RE::TESObjectREFR::ChangeFlags::kItemExtraData);
            // See the in-place path above: perform a final normalization pass
            // after the new enchanted instance has entered the player's
            // inventory.
            if (!RestoreIntendedEffectValues(created, intendedEffects)) {
                SKSE::log::error(
                    "Post-attach enchantment normalization failed for "
                    "new inventory instance.");
            }
        }

        AOS::PlayPurchaseSound();
        SKSE::log::info(
            "Enchanted {:08X} '{}' with {} effect(s) for {} gold{}.",
            a_item.object->GetFormID(),
            a_item.displayName,
            a_enchantments.size(),
            price,
            customName.empty() ? "" : " and a custom name");
        return { true, "Item enchanted." };
    }

    ItemCategory InventoryService::CategoryFor(
        const RE::TESBoundObject* a_object)
    {
        if (const auto armor =
                a_object ? a_object->As<RE::TESObjectARMO>() : nullptr) {
            if (armor->HasPartOf(
                    RE::BGSBipedObjectForm::BipedObjectSlot::kRing) ||
                armor->HasPartOf(
                    RE::BGSBipedObjectForm::BipedObjectSlot::kAmulet)) {
                return ItemCategory::kJewelry;
            }
            return ItemCategory::kArmor;
        }
        return ItemCategory::kWeapons;
    }

    std::string InventoryService::BlockReason(
        RE::TESBoundObject* a_object,
        RE::ExtraDataList* a_extraList,
        bool a_plainStack,
        bool a_hasAnyExtraLists,
        bool a_isQuestObject) const
    {
        if (!config_ || !a_object) {
            return "invalid item";
        }
        if (!a_object->GetPlayable()) {
            return "non-playable";
        }
        if (!a_object->Is(RE::FormType::Weapon) &&
            !a_object->Is(RE::FormType::Armor)) {
            return "unsupported type";
        }
        if (const auto weapon = a_object->As<RE::TESObjectWEAP>();
            weapon && (weapon->IsStaff() ||
                       weapon->IsHandToHandMelee())) {
            return "unsupported weapon type";
        }
        if (a_plainStack && a_hasAnyExtraLists) {
            return "plain stack is ambiguous beside described instances";
        }
        if (a_extraList && a_extraList->GetCount() != 1) {
            return "stacked instance";
        }
        if (FormResolver::GetSingleton().IsBlockedItem(
                a_object->GetFormID())) {
            return "blocked by configuration";
        }
        if (IsWorn(a_extraList)) {
            return "equipped";
        }
        if (a_isQuestObject) {
            return "quest item";
        }
        if (IsBaseEnchanted(a_object) ||
            IsInstanceEnchanted(a_extraList)) {
            return "already enchanted";
        }
        if (FormResolver::HasKeywordToken(
                a_object,
                config_->artifactKeywordTokens) ||
            FormResolver::HasTextToken(
                a_object,
                config_->specialItemTokens)) {
            return "artifact or unique special item";
        }
        return {};
    }

    bool InventoryService::IsBaseEnchanted(
        const RE::TESBoundObject* a_object)
    {
        const auto enchantable =
            a_object ? a_object->As<RE::TESEnchantableForm>() : nullptr;
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
        return name && name[0] != '\0' ?
            name :
            "<unnamed item>";
    }

    double InventoryService::PlayerServiceDiscount() const
    {
        return config_ ?
            AOS::PlayerServiceDiscount(config_->barterPricing) :
            0.0;
    }
}
