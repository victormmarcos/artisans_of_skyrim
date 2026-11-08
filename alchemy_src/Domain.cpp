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

#include "NPCAlchemyServices/Domain.h"

#include "ArtisansOfSkyrim/PriceRounding.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>

namespace
{
    [[nodiscard]] std::string Lower(std::string_view a_value)
    {
        std::string result{ a_value };
        std::ranges::transform(result, result.begin(), [](unsigned char a_char) {
            return static_cast<char>(std::tolower(a_char));
        });
        return result;
    }

    [[nodiscard]] std::int32_t RoundPrice(double a_value) noexcept
    {
        if (!std::isfinite(a_value) || a_value <= 0.0) {
            return 0;
        }
        const auto cap = static_cast<double>(
            std::numeric_limits<std::int32_t>::max());
        return static_cast<std::int32_t>(std::min(std::ceil(a_value), cap));
    }
}

namespace NPCAS
{
    std::optional<ServiceTier> ParseTier(std::string_view a_value)
    {
        const auto value = Lower(a_value);
        if (value == "adept") {
            return ServiceTier::kAdept;
        }
        if (value == "expert") {
            return ServiceTier::kExpert;
        }
        if (value == "master") {
            return ServiceTier::kMaster;
        }
        return std::nullopt;
    }

    std::optional<AlchemyItemType> ParseItemType(std::string_view a_value)
    {
        const auto value = Lower(a_value);
        if (value == "potion") {
            return AlchemyItemType::kPotion;
        }
        if (value == "poison") {
            return AlchemyItemType::kPoison;
        }
        return std::nullopt;
    }

    std::string_view ToString(ServiceTier a_tier) noexcept
    {
        switch (a_tier) {
        case ServiceTier::kAdept:
            return "Adept";
        case ServiceTier::kExpert:
            return "Expert";
        case ServiceTier::kMaster:
            return "Master";
        }
        return "Unknown";
    }

    std::string_view ToString(AlchemyItemType a_type) noexcept
    {
        switch (a_type) {
        case AlchemyItemType::kPotion:
            return "Potion";
        case AlchemyItemType::kPoison:
            return "Poison";
        }
        return "Item";
    }

    std::size_t TierIndex(ServiceTier a_tier) noexcept
    {
        return static_cast<std::size_t>(a_tier);
    }

    bool TierAllows(
        ServiceTier a_serviceTier,
        ServiceTier a_itemTier) noexcept
    {
        // Alchemy tiers are intentionally exclusive: each alchemist lists only
        // the recipes assigned to their exact rank. Higher-ranked alchemists do
        // not repeat lower-tier potions or poisons.
        return a_serviceTier == a_itemTier;
    }

    std::int32_t ApplyPriceDiscount(
        std::int32_t a_price,
        double a_discount) noexcept
    {
        const auto price = static_cast<double>(std::max(a_price, 0));
        const auto discount = std::clamp(a_discount, 0.0, 0.20);
        return AOS::RoundUpServicePrice(RoundPrice(price * (1.0 - discount)));
    }

    std::int32_t MaximumSafeResaleValue(
        std::int32_t a_batchPrice,
        std::int32_t a_batchSize,
        double a_maxDiscount) noexcept
    {
        const auto batchSize = std::max(a_batchSize, 1);
        const auto minimumPaid = ApplyPriceDiscount(
            a_batchPrice,
            a_maxDiscount);
        return std::max(0, (minimumPaid - 1) / batchSize);
    }

    bool IsResaleSafe(
        std::int32_t a_alchValue,
        std::int32_t a_batchPrice,
        std::int32_t a_batchSize,
        double a_maxDiscount) noexcept
    {
        const auto value = static_cast<std::int64_t>(
            std::max(a_alchValue, 0));
        const auto batchSize = static_cast<std::int64_t>(
            std::max(a_batchSize, 1));
        const auto minimumPaid = static_cast<std::int64_t>(
            ApplyPriceDiscount(a_batchPrice, a_maxDiscount));
        return value * batchSize < minimumPaid;
    }

    std::int32_t MinimumSafeBatchPrice(
        std::int32_t a_alchValue,
        std::int32_t a_batchSize,
        double a_maxDiscount) noexcept
    {
        constexpr auto maximum =
            static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
        const auto resale =
            static_cast<std::int64_t>(std::max(a_alchValue, 0)) *
            static_cast<std::int64_t>(std::max(a_batchSize, 1));
        const auto discount = std::clamp(a_maxDiscount, 0.0, 0.20);
        // Estimate from the inverse of the discount, then step up past rounding.
        auto price = std::clamp<std::int64_t>(
            static_cast<std::int64_t>(static_cast<double>(resale) / (1.0 - discount)),
            1,
            maximum);
        while (price < maximum &&
               !IsResaleSafe(
                   a_alchValue,
                   static_cast<std::int32_t>(price),
                   a_batchSize,
                   a_maxDiscount)) {
            ++price;
        }
        return static_cast<std::int32_t>(price);
    }
}
