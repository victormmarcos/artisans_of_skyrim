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

#include "NPCAlchemyServices/AlchemyCatalog.h"

#include "NPCAlchemyServices/FormResolver.h"

#include <algorithm>

namespace NPCAS
{
    AlchemyCatalog& AlchemyCatalog::GetSingleton()
    {
        static AlchemyCatalog singleton;
        return singleton;
    }

    void AlchemyCatalog::Build(const Config& a_config)
    {
        items_.clear();
        items_.reserve(a_config.items.size());

        const auto& resolver = FormResolver::GetSingleton();
        for (const auto& definition : a_config.items) {
            if (definition.exclusive &&
                !a_config.enableCustomPotions) {
                continue;
            }
            const auto form = resolver.Resolve(definition.itemForm);
            const auto alchemy = form ? form->As<RE::AlchemyItem>() : nullptr;
            if (!alchemy) {
                SKSE::log::warn(
                    "Alchemy item '{}' did not resolve '{}' as an ALCH form.",
                    definition.id,
                    definition.itemForm);
                continue;
            }
            if (alchemy->effects.empty() ||
                std::ranges::any_of(alchemy->effects, [](const RE::Effect* a_effect) {
                    return !a_effect || !a_effect->baseEffect;
                })) {
                SKSE::log::error(
                    "Alchemy item '{}' has an empty or unresolved effect chain; "
                    "it was excluded before the service menu was built.",
                    definition.id);
                continue;
            }
            if (alchemy->GetGoldValue() > definition.alchValue) {
                SKSE::log::info(
                    "Alchemy item '{}': in-game value {} exceeds configured {}; "
                    "its batch price is kept above the resale-safe minimum.",
                    definition.id,
                    alchemy->GetGoldValue(),
                    definition.alchValue);
            }
            items_.push_back({ std::addressof(definition), alchemy });
        }

        std::ranges::sort(
            items_,
            [](const ResolvedAlchemyItem& a_lhs, const ResolvedAlchemyItem& a_rhs) {
                const auto lhsGroup = a_lhs.definition->groups.empty() ?
                    std::string_view{} : std::string_view{ a_lhs.definition->groups.front() };
                const auto rhsGroup = a_rhs.definition->groups.empty() ?
                    std::string_view{} : std::string_view{ a_rhs.definition->groups.front() };
                if (lhsGroup != rhsGroup) {
                    return lhsGroup < rhsGroup;
                }
                return a_lhs.definition->displayName <
                    a_rhs.definition->displayName;
            });
        SKSE::log::info(
            "Alchemy catalog built with {} validated item(s) from {} definition(s).",
            items_.size(),
            a_config.items.size());
    }

    std::vector<const ResolvedAlchemyItem*> AlchemyCatalog::List(
        ServiceTier a_tier) const
    {
        std::vector<const ResolvedAlchemyItem*> result;
        for (const auto& item : items_) {
            if (item.definition &&
                (item.definition->exclusive ?
                     a_tier == item.definition->minimumTier :
                     TierAllows(a_tier, item.definition->minimumTier))) {
                result.push_back(std::addressof(item));
            }
        }
        return result;
    }

    const ResolvedAlchemyItem* AlchemyCatalog::Find(
        std::string_view a_id) const
    {
        for (const auto& item : items_) {
            if (item.definition && item.definition->id == a_id) {
                return std::addressof(item);
            }
        }
        return nullptr;
    }
}
