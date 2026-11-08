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
#include <optional>
#include <string>
#include <string_view>

namespace NPCSS
{
    enum class SmithTier : std::uint8_t
    {
        kAdept,
        kExpert,
        kMaster
    };

    enum class TemperQuality : std::uint8_t
    {
        kUnimproved,
        kFine,
        kSuperior,
        kExquisite,
        kFlawless,
        kEpic,
        kLegendary
    };

    enum class ServiceComplexity : std::uint8_t
    {
        kSmall,
        kMedium,
        kLarge,
        kAmmunition
    };

    struct QualityServiceFees
    {
        double smallItem{ 0.0 };  // "small" is a Windows header macro
        double mediumItem{ 0.0 };
        double largeItem{ 0.0 };
    };

    // Improvement price = fee for the quality and item size
    //                     + item value x valueMultiplier.
    struct ImprovementPricing
    {
        double valueMultiplier{ 0.20 };
        QualityServiceFees fine{ 200.0, 250.0, 300.0 };
        QualityServiceFees superior{ 300.0, 350.0, 400.0 };
        QualityServiceFees exquisite{ 400.0, 450.0, 500.0 };
        QualityServiceFees flawless{ 500.0, 750.0, 1000.0 };
        QualityServiceFees epic{ 1000.0, 1250.0, 1500.0 };
        QualityServiceFees legendary{ 1500.0, 2000.0, 2500.0 };
    };

    struct CraftingPricing
    {
        double multiplier{ 1.25 };
        double smallServiceFee{ 400.0 };
        double mediumServiceFee{ 700.0 };
        double largeServiceFee{ 1000.0 };
        double ammunitionServiceFee{ 200.0 };
    };

    struct TemperTargets
    {
        double fine{ 1.19 };
        double superior{ 1.29 };
        double exquisite{ 1.39 };
        double flawless{ 1.49 };
        double epic{ 1.59 };
        double legendary{ 2.0 };
    };

    using BarterPricing = AOS::BarterPricing;

    [[nodiscard]] std::optional<SmithTier> ParseTier(std::string_view a_value);
    [[nodiscard]] std::string_view ToString(SmithTier a_tier) noexcept;
    [[nodiscard]] std::string_view ToString(TemperQuality a_quality) noexcept;
    [[nodiscard]] double TargetHealth(TemperQuality a_quality, const TemperTargets& a_targets) noexcept;
    [[nodiscard]] double TargetHealth(SmithTier a_tier, const TemperTargets& a_targets) noexcept;
    [[nodiscard]] TemperQuality QualityFromHealth(double a_health) noexcept;
    [[nodiscard]] TemperQuality TargetQuality(SmithTier a_tier) noexcept;
    [[nodiscard]] bool CanOfferQuality(SmithTier a_tier, TemperQuality a_quality) noexcept;
    [[nodiscard]] std::int32_t ImprovementPrice(
        std::int32_t a_baseValue,
        TemperQuality a_quality,
        ServiceComplexity a_complexity,
        const ImprovementPricing& a_pricing) noexcept;
    [[nodiscard]] double ImprovementServiceFee(
        TemperQuality a_quality,
        ServiceComplexity a_complexity,
        const ImprovementPricing& a_pricing) noexcept;
    [[nodiscard]] double CraftingServiceFee(
        ServiceComplexity a_complexity,
        const CraftingPricing& a_pricing) noexcept;
    [[nodiscard]] std::int32_t CraftingPrice(
        std::int32_t a_baseValue,
        ServiceComplexity a_complexity,
        const CraftingPricing& a_pricing,
        std::int32_t a_count = 1) noexcept;
    [[nodiscard]] std::int32_t ApplyPriceDiscount(
        std::int32_t a_price,
        double a_discount) noexcept;
    [[nodiscard]] std::int32_t FixedBatchPrice(
        std::int32_t a_fixedPrice,
        std::int32_t a_baseValue,
        std::int32_t a_count,
        double a_discount) noexcept;
    [[nodiscard]] bool ContainsWholeToken(
        std::string_view a_text,
        std::string_view a_token);
    [[nodiscard]] bool IsOfficialEquipmentPlugin(
        std::string_view a_filename);
}
