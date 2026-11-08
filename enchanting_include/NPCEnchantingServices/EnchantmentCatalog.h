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

#include "RE/Skyrim.h"

#include <span>
#include <vector>

namespace NPCES
{
    struct ResolvedEnchantment
    {
        const EnchantmentDefinition* definition{ nullptr };
        RE::EnchantmentItem* source{ nullptr };
    };

    class EnchantmentCatalog
    {
    public:
        static EnchantmentCatalog& GetSingleton();

        void Build(const Config& a_config);
        [[nodiscard]] std::vector<const ResolvedEnchantment*> ListValid(
            RE::TESBoundObject* a_object,
            ItemCategory a_category,
            ServiceTier a_tier) const;
        [[nodiscard]] bool IsValid(
            RE::TESBoundObject* a_object,
            ItemCategory a_category,
            ServiceTier a_tier,
            const ResolvedEnchantment& a_enchantment) const;
        [[nodiscard]] bool BuildEffects(
            std::span<const ResolvedEnchantment* const> a_enchantments,
            ServiceTier a_tier,
            RE::BSTArray<RE::Effect>& a_effects) const;
        [[nodiscard]] const TierValue& ValueFor(
            const ResolvedEnchantment& a_enchantment,
            ServiceTier a_tier) const;

    private:
        std::vector<ResolvedEnchantment> enchantments_;
    };
}
