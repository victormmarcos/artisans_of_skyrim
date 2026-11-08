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

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string_view>

namespace NPCAS
{
    enum class ServiceTier : std::uint8_t
    {
        kAdept,
        kExpert,
        kMaster
    };

    enum class AlchemyItemType : std::uint8_t
    {
        kPotion,
        kPoison
    };

    using BarterPricing = AOS::BarterPricing;

    [[nodiscard]] std::optional<ServiceTier> ParseTier(std::string_view a_value);
    [[nodiscard]] std::optional<AlchemyItemType> ParseItemType(std::string_view a_value);
    [[nodiscard]] std::string_view ToString(ServiceTier a_tier) noexcept;
    [[nodiscard]] std::string_view ToString(AlchemyItemType a_type) noexcept;
    [[nodiscard]] std::size_t TierIndex(ServiceTier a_tier) noexcept;
    [[nodiscard]] bool TierAllows(
        ServiceTier a_serviceTier,
        ServiceTier a_itemTier) noexcept;
    [[nodiscard]] std::int32_t ApplyPriceDiscount(
        std::int32_t a_price,
        double a_discount) noexcept;
    [[nodiscard]] std::int32_t MaximumSafeResaleValue(
        std::int32_t a_batchPrice,
        std::int32_t a_batchSize,
        double a_maxDiscount) noexcept;
    [[nodiscard]] bool IsResaleSafe(
        std::int32_t a_alchValue,
        std::int32_t a_batchPrice,
        std::int32_t a_batchSize,
        double a_maxDiscount) noexcept;
    // Smallest batch price for which IsResaleSafe holds.
    [[nodiscard]] std::int32_t MinimumSafeBatchPrice(
        std::int32_t a_alchValue,
        std::int32_t a_batchSize,
        double a_maxDiscount) noexcept;
}
