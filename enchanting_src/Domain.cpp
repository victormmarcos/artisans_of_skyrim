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

#include "NPCEnchantingServices/Domain.h"

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
        return static_cast<std::int32_t>(std::min(std::round(a_value), cap));
    }

}

namespace NPCES
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

    std::optional<EnchantmentType> ParseType(std::string_view a_value)
    {
        const auto value = Lower(a_value);
        if (value == "weapon") {
            return EnchantmentType::kWeapon;
        }
        if (value == "apparel") {
            return EnchantmentType::kApparel;
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

    std::string_view ToString(EnchantmentType a_type) noexcept
    {
        switch (a_type) {
        case EnchantmentType::kWeapon:
            return "Weapon";
        case EnchantmentType::kApparel:
            return "Apparel";
        }
        return "Unknown";
    }

    std::string_view ToString(ItemCategory a_category) noexcept
    {
        switch (a_category) {
        case ItemCategory::kWeapons:
            return "Weapons";
        case ItemCategory::kArmor:
            return "Armor";
        case ItemCategory::kJewelry:
            return "Jewelry";
        }
        return "Items";
    }

    std::size_t TierIndex(ServiceTier a_tier) noexcept
    {
        return static_cast<std::size_t>(a_tier);
    }

    bool TierAllows(
        ServiceTier a_serviceTier,
        ServiceTier a_minimumTier) noexcept
    {
        return TierIndex(a_serviceTier) >= TierIndex(a_minimumTier);
    }

    bool CanUseDual(ServiceTier a_tier) noexcept
    {
        return a_tier == ServiceTier::kMaster;
    }

    std::int32_t SinglePrice(
        std::int32_t a_tablePrice,
        const Pricing& a_pricing) noexcept
    {
        const auto tablePrice = std::max(a_tablePrice, 0);
        const auto soulCost = std::max(a_pricing.grandSoulServiceCost, 0);
        return RoundPrice(
            static_cast<double>(tablePrice) +
            static_cast<double>(soulCost));
    }

    std::int32_t ApplyPriceDiscount(
        std::int32_t a_price,
        double a_discount) noexcept
    {
        const auto price = static_cast<double>(std::max(a_price, 0));
        const auto discount = std::clamp(a_discount, 0.0, 0.20);
        return AOS::RoundUpServicePrice(RoundPrice(price * (1.0 - discount)));
    }

    std::int32_t AddPrices(
        std::int32_t a_first,
        std::int32_t a_second) noexcept
    {
        const auto total =
            static_cast<std::int64_t>(std::max(a_first, 0)) +
            static_cast<std::int64_t>(std::max(a_second, 0));
        return static_cast<std::int32_t>(std::min(
            total,
            static_cast<std::int64_t>(
                std::numeric_limits<std::int32_t>::max())));
    }

}
