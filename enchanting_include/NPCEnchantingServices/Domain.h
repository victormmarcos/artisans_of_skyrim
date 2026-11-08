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

#include "ArtisansOfSkyrim/BarterPricing.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace NPCES
{
    enum class ServiceTier : std::uint8_t
    {
        kAdept,
        kExpert,
        kMaster
    };

    enum class EnchantmentType : std::uint8_t
    {
        kWeapon,
        kApparel
    };

    enum class ItemCategory : std::uint8_t
    {
        kWeapons,
        kArmor,
        kJewelry
    };

    struct TierValue
    {
        std::optional<float> magnitude;
        std::optional<std::uint32_t> duration;
        std::optional<float> chargeCost;
        std::int32_t price{ 0 };
        std::string valueText;

        [[nodiscard]] bool Available() const noexcept
        {
            return price > 0;
        }
    };

    struct Pricing
    {
        std::int32_t grandSoulServiceCost{ 1000 };
    };

    using BarterPricing = AOS::BarterPricing;

    [[nodiscard]] std::optional<ServiceTier> ParseTier(std::string_view a_value);
    [[nodiscard]] std::optional<EnchantmentType> ParseType(std::string_view a_value);
    [[nodiscard]] std::string_view ToString(ServiceTier a_tier) noexcept;
    [[nodiscard]] std::string_view ToString(EnchantmentType a_type) noexcept;
    [[nodiscard]] std::string_view ToString(ItemCategory a_category) noexcept;
    [[nodiscard]] std::size_t TierIndex(ServiceTier a_tier) noexcept;
    [[nodiscard]] bool TierAllows(
        ServiceTier a_serviceTier,
        ServiceTier a_minimumTier) noexcept;
    [[nodiscard]] bool CanUseDual(ServiceTier a_tier) noexcept;
    [[nodiscard]] std::int32_t SinglePrice(
        std::int32_t a_tablePrice,
        const Pricing& a_pricing) noexcept;
    [[nodiscard]] std::int32_t ApplyPriceDiscount(
        std::int32_t a_price,
        double a_discount) noexcept;
    [[nodiscard]] std::int32_t AddPrices(
        std::int32_t a_first,
        std::int32_t a_second) noexcept;
}
