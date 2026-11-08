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

#pragma once

#include "NPCEnchantingServices/Config.h"
#include "NPCEnchantingServices/EnchanterClassifier.h"
#include "NPCEnchantingServices/EnchantmentCatalog.h"

#include "RE/Skyrim.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace NPCES
{
    struct EnchantableItem
    {
        RE::TESBoundObject* object{ nullptr };
        RE::ExtraDataList* extraList{ nullptr };
        std::string displayName;
        std::string currentName;
        ItemCategory category{ ItemCategory::kWeapons };
        bool plainStackItem{ false };
    };

    struct TransactionResult
    {
        bool success{ false };
        std::string message;
    };

    class InventoryService
    {
    public:
        static InventoryService& GetSingleton();

        void Configure(const Config& a_config);
        [[nodiscard]] std::vector<EnchantableItem> ListItems(
            const EnchanterContext& a_enchanter) const;
        [[nodiscard]] std::vector<const ResolvedEnchantment*> ListEnchantments(
            const EnchanterContext& a_enchanter,
            const EnchantableItem& a_item) const;
        [[nodiscard]] std::int32_t Price(
            const EnchanterContext& a_enchanter,
            std::span<const ResolvedEnchantment* const> a_enchantments) const;
        [[nodiscard]] TransactionResult ApplyEnchanting(
            const EnchanterContext& a_enchanter,
            const EnchantableItem& a_item,
            std::span<const ResolvedEnchantment* const> a_enchantments,
            std::string_view a_customName = {}) const;

        [[nodiscard]] static ItemCategory CategoryFor(
            const RE::TESBoundObject* a_object);

    private:
        [[nodiscard]] std::string BlockReason(
            RE::TESBoundObject* a_object,
            RE::ExtraDataList* a_extraList,
            bool a_plainStack,
            bool a_hasAnyExtraLists,
            bool a_isQuestObject) const;
        [[nodiscard]] static bool IsBaseEnchanted(
            const RE::TESBoundObject* a_object);
        [[nodiscard]] static bool IsInstanceEnchanted(
            RE::ExtraDataList* a_extraList);
        [[nodiscard]] static std::string DisplayName(
            RE::TESBoundObject* a_object,
            RE::ExtraDataList* a_extraList);
        [[nodiscard]] double PlayerServiceDiscount() const;

        const Config* config_{ nullptr };
    };
}
