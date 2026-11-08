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

#include "NPCSmithServices/Domain.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace NPCSS
{
    enum class CraftingOrderCategory : std::uint8_t
    {
        kArrow,
        kBolt,
        kDagger,
        kSword,
        kWarAxe,
        kMace,
        kBoots,
        kGauntlets,
        kBow,
        kShield,
        kHelmet,
        kGreatsword,
        kBattleaxe,
        kWarhammer,
        kLightBodyArmor,
        kHeavyBodyArmor,
        kCrossbow
    };

    enum class CraftingOrderStatus : std::uint8_t
    {
        kPending,
        kReady,
        kCollected,
        kLost,
        kInvalid
    };

    enum class ArmorWeightClass : std::uint8_t
    {
        kNone,
        kLight,
        kHeavy
    };

    struct OrderTimelineEntry
    {
        std::uint32_t smithFormID{ 0 };
        CraftingOrderStatus status{ CraftingOrderStatus::kPending };
        double readyGameHour{ 0.0 };
    };

    struct OrderSchedulePreview
    {
        bool accepted{ false };
        std::size_t activeOrders{ 0 };
        double startGameHour{ 0.0 };
        double readyGameHour{ 0.0 };
    };

    [[nodiscard]] std::string_view ToString(
        CraftingOrderCategory a_category) noexcept;
    [[nodiscard]] std::string_view ToString(
        CraftingOrderStatus a_status) noexcept;
    [[nodiscard]] std::string CraftingItemTypeLabel(
        CraftingOrderCategory a_category,
        ArmorWeightClass a_armorWeight = ArmorWeightClass::kNone);
    [[nodiscard]] bool IsInstantOrderCategory(
        CraftingOrderCategory a_category) noexcept;
    [[nodiscard]] ServiceComplexity ComplexityForCategory(
        CraftingOrderCategory a_category) noexcept;
    [[nodiscard]] bool IsSupportedHelmet(
        bool a_hasHelmetKeyword,
        bool a_isProtectiveArmor,
        bool a_usesHeadSlot,
        bool a_usesHairSlot,
        bool a_usesCircletSlot,
        bool a_isCircletIdentity) noexcept;
    [[nodiscard]] OrderSchedulePreview PreviewOrderSchedule(
        std::uint32_t a_smithFormID,
        double a_nowGameHour,
        double a_durationHours,
        std::size_t a_maximumActiveOrders,
        std::span<const OrderTimelineEntry> a_orders) noexcept;
    [[nodiscard]] CraftingOrderStatus UpdatedOrderStatus(
        CraftingOrderStatus a_status,
        double a_readyGameHour,
        double a_nowGameHour,
        bool a_smithDead) noexcept;
}
