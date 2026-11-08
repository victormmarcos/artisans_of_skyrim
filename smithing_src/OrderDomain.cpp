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

#include "NPCSmithServices/OrderDomain.h"

#include <algorithm>

namespace NPCSS
{
    std::string_view ToString(CraftingOrderCategory a_category) noexcept
    {
        switch (a_category) {
        case CraftingOrderCategory::kArrow: return "Arrow";
        case CraftingOrderCategory::kBolt: return "Bolt";
        case CraftingOrderCategory::kDagger: return "Dagger";
        case CraftingOrderCategory::kSword: return "Sword";
        case CraftingOrderCategory::kWarAxe: return "War Axe";
        case CraftingOrderCategory::kMace: return "Mace";
        case CraftingOrderCategory::kBoots: return "Boots";
        case CraftingOrderCategory::kGauntlets: return "Gauntlets";
        case CraftingOrderCategory::kBow: return "Bow";
        case CraftingOrderCategory::kShield: return "Shield";
        case CraftingOrderCategory::kHelmet: return "Helmet";
        case CraftingOrderCategory::kGreatsword: return "Greatsword";
        case CraftingOrderCategory::kBattleaxe: return "Battleaxe";
        case CraftingOrderCategory::kWarhammer: return "Warhammer";
        case CraftingOrderCategory::kLightBodyArmor: return "Light Body Armor";
        case CraftingOrderCategory::kHeavyBodyArmor: return "Heavy Body Armor";
        case CraftingOrderCategory::kCrossbow: return "Crossbow";
        }
        return "Unknown";
    }

    std::string_view ToString(CraftingOrderStatus a_status) noexcept
    {
        switch (a_status) {
        case CraftingOrderStatus::kPending: return "Pending";
        case CraftingOrderStatus::kReady: return "Ready";
        case CraftingOrderStatus::kCollected: return "Collected";
        case CraftingOrderStatus::kLost: return "Lost";
        case CraftingOrderStatus::kInvalid: return "Invalid";
        }
        return "Unknown";
    }

    std::string CraftingItemTypeLabel(
        CraftingOrderCategory a_category,
        ArmorWeightClass a_armorWeight)
    {
        if (a_category == CraftingOrderCategory::kLightBodyArmor ||
            a_category == CraftingOrderCategory::kHeavyBodyArmor) {
            return std::string{ ToString(a_category) };
        }

        const bool armorPiece =
            a_category == CraftingOrderCategory::kHelmet ||
            a_category == CraftingOrderCategory::kBoots ||
            a_category == CraftingOrderCategory::kGauntlets ||
            a_category == CraftingOrderCategory::kShield;
        if (!armorPiece || a_armorWeight == ArmorWeightClass::kNone) {
            return std::string{ ToString(a_category) };
        }

        const auto prefix = a_armorWeight == ArmorWeightClass::kLight ?
            "Light " :
            "Heavy ";
        std::string label{ prefix };
        label.append(ToString(a_category));
        return label;
    }

    bool IsInstantOrderCategory(CraftingOrderCategory a_category) noexcept
    {
        return a_category == CraftingOrderCategory::kArrow ||
            a_category == CraftingOrderCategory::kBolt;
    }

    ServiceComplexity ComplexityForCategory(
        CraftingOrderCategory a_category) noexcept
    {
        switch (a_category) {
        case CraftingOrderCategory::kArrow:
        case CraftingOrderCategory::kBolt:
            return ServiceComplexity::kAmmunition;
        case CraftingOrderCategory::kDagger:
            return ServiceComplexity::kSmall;
        case CraftingOrderCategory::kGreatsword:
        case CraftingOrderCategory::kBattleaxe:
        case CraftingOrderCategory::kWarhammer:
        case CraftingOrderCategory::kHeavyBodyArmor:
        case CraftingOrderCategory::kCrossbow:
            return ServiceComplexity::kLarge;
        case CraftingOrderCategory::kSword:
        case CraftingOrderCategory::kWarAxe:
        case CraftingOrderCategory::kMace:
        case CraftingOrderCategory::kBoots:
        case CraftingOrderCategory::kGauntlets:
        case CraftingOrderCategory::kBow:
        case CraftingOrderCategory::kShield:
        case CraftingOrderCategory::kHelmet:
        case CraftingOrderCategory::kLightBodyArmor:
            return ServiceComplexity::kMedium;
        }
        return ServiceComplexity::kMedium;
    }

    bool IsSupportedHelmet(
        bool a_hasHelmetKeyword,
        bool a_isProtectiveArmor,
        bool a_usesHeadSlot,
        bool a_usesHairSlot,
        bool a_usesCircletSlot,
        bool a_isCircletIdentity) noexcept
    {
        if (a_isCircletIdentity) {
            return false;
        }
        if (a_hasHelmetKeyword) {
            return true;
        }
        return a_isProtectiveArmor &&
            (a_usesHeadSlot || a_usesHairSlot || a_usesCircletSlot);
    }

    OrderSchedulePreview PreviewOrderSchedule(
        std::uint32_t a_smithFormID,
        double a_nowGameHour,
        double a_durationHours,
        std::size_t a_maximumActiveOrders,
        std::span<const OrderTimelineEntry> a_orders) noexcept
    {
        OrderSchedulePreview result;
        result.startGameHour = std::max(a_nowGameHour, 0.0);
        for (const auto& order : a_orders) {
            if (order.smithFormID != a_smithFormID) {
                continue;
            }
            if (order.status == CraftingOrderStatus::kPending ||
                order.status == CraftingOrderStatus::kReady) {
                ++result.activeOrders;
            }
            if (order.status == CraftingOrderStatus::kPending) {
                result.startGameHour = std::max(
                    result.startGameHour,
                    order.readyGameHour);
            }
        }
        result.accepted =
            result.activeOrders < a_maximumActiveOrders &&
            a_durationHours > 0.0;
        result.readyGameHour = result.startGameHour +
            std::max(a_durationHours, 0.0);
        return result;
    }

    CraftingOrderStatus UpdatedOrderStatus(
        CraftingOrderStatus a_status,
        double a_readyGameHour,
        double a_nowGameHour,
        bool a_smithDead) noexcept
    {
        if (a_status != CraftingOrderStatus::kPending &&
            a_status != CraftingOrderStatus::kReady) {
            return a_status;
        }
        if (a_smithDead) {
            return CraftingOrderStatus::kLost;
        }
        if (a_status == CraftingOrderStatus::kPending &&
            a_nowGameHour >= a_readyGameHour) {
            return CraftingOrderStatus::kReady;
        }
        return a_status;
    }
}
