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

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace AOS
{
    enum class EffectTier
    {
        kNone,
        kExpert,
        kMaster
    };

    struct TierMagnitudes
    {
        float expert;
        float master;
    };

    inline constexpr TierMagnitudes kSilverBlessingMagnitudes{ 25.0F, 50.0F };
    inline constexpr TierMagnitudes kBerserkerSoulMagnitudes{ 20.0F, 40.0F };
    inline constexpr TierMagnitudes kHonedMetalMagnitudes{ 15.0F, 30.0F };
    inline constexpr TierMagnitudes kUnburdenMagnitudes{ 15.0F, 30.0F };
    inline constexpr TierMagnitudes kReinforcementMagnitudes{ 20.0F, 40.0F };
    inline constexpr TierMagnitudes kElementalBurstMagnitudes{ 25.0F, 50.0F };

    [[nodiscard]] inline EffectTier SelectTier(
        float a_magnitude,
        TierMagnitudes a_supported) noexcept
    {
        if (!(a_magnitude > 0.0F) || !std::isfinite(a_magnitude)) {
            return EffectTier::kNone;
        }
        const auto midpoint = (a_supported.expert + a_supported.master) * 0.5F;
        return a_magnitude < midpoint ?
            EffectTier::kExpert : EffectTier::kMaster;
    }

    [[nodiscard]] inline bool MagnitudesMatch(
        float a_expert,
        float a_master,
        TierMagnitudes a_supported,
        float a_tolerance = 0.001F) noexcept
    {
        return std::isfinite(a_expert) && std::isfinite(a_master) &&
            std::abs(a_expert - a_supported.expert) <= a_tolerance &&
            std::abs(a_master - a_supported.master) <= a_tolerance;
    }

    [[nodiscard]] inline float ElementalBurstBonus(
        float a_totalMagnitude) noexcept
    {
        if (!(a_totalMagnitude > 0.0F) || !std::isfinite(a_totalMagnitude)) {
            return 0.0F;
        }
        const auto total = std::clamp(a_totalMagnitude, 0.0F, 100.0F);
        if (total < 37.5F) {
            return 25.0F;
        }
        if (total < 62.5F) {
            return 50.0F;
        }
        if (total < 87.5F) {
            return 75.0F;
        }
        return 100.0F;
    }

    [[nodiscard]] inline float PercentageMultiplier(
        float a_percentage) noexcept
    {
        if (!(a_percentage > 0.0F) || !std::isfinite(a_percentage)) {
            return 1.0F;
        }
        return 1.0F + std::clamp(a_percentage, 0.0F, 100.0F) / 100.0F;
    }

    [[nodiscard]] inline float ArmorBypassDamageMultiplier(
        float a_armorRating,
        float a_ignorePercentage,
        float a_armorScalingFactor = 0.12F,
        float a_maxArmorReduction = 80.0F) noexcept
    {
        if (!(a_armorRating > 0.0F) ||
            !(a_ignorePercentage > 0.0F) ||
            !(a_armorScalingFactor > 0.0F) ||
            !(a_maxArmorReduction > 0.0F) ||
            !std::isfinite(a_armorRating) ||
            !std::isfinite(a_ignorePercentage) ||
            !std::isfinite(a_armorScalingFactor) ||
            !std::isfinite(a_maxArmorReduction)) {
            return 1.0F;
        }

        const auto ignoredFraction =
            std::clamp(a_ignorePercentage, 0.0F, 100.0F) / 100.0F;
        const auto reductionCap =
            std::clamp(a_maxArmorReduction, 0.0F, 99.0F) / 100.0F;
        const auto currentReduction = std::min(
            reductionCap,
            a_armorRating * a_armorScalingFactor / 100.0F);
        const auto bypassedReduction = std::min(
            reductionCap,
            a_armorRating * (1.0F - ignoredFraction) *
                a_armorScalingFactor / 100.0F);
        return (1.0F - bypassedReduction) / (1.0F - currentReduction);
    }

    [[nodiscard]] constexpr float SprintStaminaRefund(
        float a_observedLoss,
        float a_deltaSeconds,
        float a_reduction,
        float a_drainMultiplier,
        float a_weightBase) noexcept
    {
        if (!(a_observedLoss > 0.0F) || !(a_deltaSeconds > 0.0F) ||
            !(a_reduction > 0.0F) || !(a_drainMultiplier > 0.0F) ||
            !(a_weightBase > 0.0F)) {
            return 0.0F;
        }
        const auto expectedSprintLoss =
            a_drainMultiplier * a_weightBase * a_deltaSeconds;
        return std::max(
            0.0F,
            std::min(a_observedLoss, expectedSprintLoss) *
                std::clamp(a_reduction, 0.0F, 1.0F));
    }
}
