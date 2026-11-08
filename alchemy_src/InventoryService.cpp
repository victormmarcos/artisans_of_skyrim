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

#include "NPCAlchemyServices/InventoryService.h"
#include "ArtisansOfSkyrim/Localization.h"

#include "ArtisansOfSkyrim/ServiceDiscount.h"
#include "ArtisansOfSkyrim/TransactionFeedback.h"

#include <algorithm>
#include <array>

namespace
{
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

    [[nodiscard]] bool ValidateAndLogAlchemyForm(
        RE::TESBoundObject* a_object,
        std::string_view a_definitionID)
    {
        const auto alchemy = a_object ? a_object->As<RE::AlchemyItem>() : nullptr;
        if (!alchemy) {
            SKSE::log::error(
                "[Alchemy] '{}' resolved {:08X} as a non-ALCH form.",
                a_definitionID,
                a_object ? a_object->GetFormID() : 0);
            return false;
        }
        if (alchemy->effects.empty()) {
            SKSE::log::error(
                "[Alchemy] '{}' ALCH {:08X} has no effects.",
                a_definitionID,
                alchemy->GetFormID());
            return false;
        }

        SKSE::log::info(
            "[Alchemy] Validating '{}' ALCH {:08X}: {} effect(s).",
            a_definitionID,
            alchemy->GetFormID(),
            alchemy->effects.size());
        std::size_t index = 0;
        for (const auto effect : alchemy->effects) {
            if (!effect || !effect->baseEffect) {
                SKSE::log::error(
                    "[Alchemy] '{}' effect {} has no resolved MGEF.",
                    a_definitionID,
                    index);
                return false;
            }
            SKSE::log::info(
                "[Alchemy] Effect {} MGEF {:08X}, magnitude {}, duration {}, area {}.",
                index,
                effect->baseEffect->GetFormID(),
                effect->effectItem.magnitude,
                effect->effectItem.duration,
                effect->effectItem.area);
            ++index;
        }
        return true;
    }
}

namespace NPCAS
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

    std::vector<const ResolvedAlchemyItem*> InventoryService::ListItems(
        const AlchemistContext& a_alchemist) const
    {
        if (!config_ || !a_alchemist.servicesUnlocked) {
            return {};
        }
        return AlchemyCatalog::GetSingleton().List(a_alchemist.tier);
    }

    std::int32_t InventoryService::Price(
        const ResolvedAlchemyItem& a_item) const
    {
        if (!config_ || !a_item.definition) {
            return 0;
        }
        // Resale protection uses the item's real value in game: an entry may
        // point at a vanilla or patch record worth more than its TOML value.
        const auto& definition = *a_item.definition;
        const auto resaleValue = a_item.item ?
            std::max(definition.alchValue, a_item.item->GetGoldValue()) :
            definition.alchValue;
        const auto batchPrice = std::max(
            definition.batchPrice,
            MinimumSafeBatchPrice(
                resaleValue,
                definition.batchSize,
                config_->barterPricing.maxDiscount));
        return ApplyPriceDiscount(batchPrice, PlayerServiceDiscount());
    }

    TransactionResult InventoryService::Purchase(
        const AlchemistContext& a_alchemist,
        const ResolvedAlchemyItem& a_item) const
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!config_ || !player || !a_item.definition || !a_item.item) {
            return { false, "The alchemy service is unavailable." };
        }

        const auto actor = a_alchemist.actor.get();
        const auto current = actor ?
            AlchemistClassifier::GetSingleton().Classify(actor.get()) :
            std::nullopt;
        if (!current || !current->servicesUnlocked ||
            current->baseFormID != a_alchemist.baseFormID ||
            current->tier != a_alchemist.tier) {
            return { false, "The alchemy service is unavailable." };
        }

        const auto available = ListItems(*current);
        const auto stillAvailable = std::ranges::any_of(
            available,
            [&](const ResolvedAlchemyItem* a_candidate) {
                return a_candidate && a_candidate->definition &&
                    a_candidate->definition->id == a_item.definition->id;
            });
        if (!stillAvailable) {
            return { false, "That batch is not available from this alchemist." };
        }
        if (!ValidateAndLogAlchemyForm(a_item.item, a_item.definition->id)) {
            return { false, "That alchemy item has an invalid effect record." };
        }

        const auto price = Price(a_item);
        const auto gold =
            RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        if (!gold || price <= 0) {
            return { false, "The alchemy service is unavailable." };
        }
        if (player->GetItemCount(gold) < price) {
            return { false, "You do not have enough gold." };
        }
        if (!TryRemoveGold(player, gold, price)) {
            return { false, "The alchemy service could not take payment." };
        }

        const auto count = std::max(a_item.definition->batchSize, 1);
        const auto before = player->GetItemCount(a_item.item);
        SKSE::log::info(
            "[Alchemy] Calling AddObjectToContainer for '{}' ({:08X}) x{}.",
            a_item.definition->id,
            a_item.item->GetFormID(),
            count);
        player->AddObjectToContainer(a_item.item, nullptr, count, nullptr);
        SKSE::log::info(
            "[Alchemy] AddObjectToContainer returned for '{}' ({:08X}).",
            a_item.definition->id,
            a_item.item->GetFormID());
        const auto after = player->GetItemCount(a_item.item);
        if (after - before < count) {
            RefundGold(player, gold, price);
            return { false, "The alchemy batch could not be delivered." };
        }

        AOS::PlayPurchaseSound();
        SKSE::log::info(
            "Purchased alchemy batch '{}' x{} for {} gold.",
            a_item.definition->id,
            count,
            price);
        return {
            true,
            AOS::Text::Format("Purchased {} {}.", count,
                AOS::Text::Localize(a_item.definition->displayName))
        };
    }

    double InventoryService::PlayerServiceDiscount() const
    {
        return config_ ?
            AOS::PlayerServiceDiscount(config_->barterPricing) :
            0.0;
    }
}
