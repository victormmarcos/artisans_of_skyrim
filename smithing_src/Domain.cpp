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

#include "NPCSmithServices/Domain.h"

#include "ArtisansOfSkyrim/PriceRounding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

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

        const auto cap = static_cast<double>(std::numeric_limits<std::int32_t>::max());
        return static_cast<std::int32_t>(std::min(std::round(a_value), cap));
    }

    [[nodiscard]] std::vector<std::string> Words(std::string_view a_value)
    {
        std::vector<std::string> result;
        std::string current;
        char previous = '\0';

        auto flush = [&]() {
            if (!current.empty()) {
                result.push_back(std::move(current));
                current.clear();
            }
        };

        for (const auto raw : a_value) {
            const auto ch = static_cast<unsigned char>(raw);
            if (!std::isalnum(ch)) {
                flush();
                previous = '\0';
                continue;
            }

            const bool camelBoundary =
                !current.empty() && std::isupper(ch) &&
                (std::islower(static_cast<unsigned char>(previous)) ||
                 std::isdigit(static_cast<unsigned char>(previous)));
            if (camelBoundary) {
                flush();
            }
            current.push_back(static_cast<char>(std::tolower(ch)));
            previous = raw;
        }
        flush();
        return result;
    }
}

namespace NPCSS
{
    std::optional<SmithTier> ParseTier(std::string_view a_value)
    {
        const auto value = Lower(a_value);
        if (value == "adept") {
            return SmithTier::kAdept;
        }
        if (value == "expert") {
            return SmithTier::kExpert;
        }
        if (value == "master") {
            return SmithTier::kMaster;
        }
        return std::nullopt;
    }

    std::string_view ToString(SmithTier a_tier) noexcept
    {
        switch (a_tier) {
        case SmithTier::kAdept:
            return "Adept";
        case SmithTier::kExpert:
            return "Expert";
        case SmithTier::kMaster:
            return "Master";
        }
        return "Unknown";
    }

    std::string_view ToString(TemperQuality a_quality) noexcept
    {
        switch (a_quality) {
        case TemperQuality::kUnimproved:
            return "Unimproved";
        case TemperQuality::kFine:
            return "Fine";
        case TemperQuality::kSuperior:
            return "Superior";
        case TemperQuality::kExquisite:
            return "Exquisite";
        case TemperQuality::kFlawless:
            return "Flawless";
        case TemperQuality::kEpic:
            return "Epic";
        case TemperQuality::kLegendary:
            return "Legendary";
        }
        return "Unknown";
    }

    double TargetHealth(TemperQuality a_quality, const TemperTargets& a_targets) noexcept
    {
        switch (a_quality) {
        case TemperQuality::kFine:
            return a_targets.fine;
        case TemperQuality::kSuperior:
            return a_targets.superior;
        case TemperQuality::kExquisite:
            return a_targets.exquisite;
        case TemperQuality::kFlawless:
            return a_targets.flawless;
        case TemperQuality::kEpic:
            return a_targets.epic;
        case TemperQuality::kLegendary:
            return a_targets.legendary;
        case TemperQuality::kUnimproved:
            break;
        }
        return 1.0;
    }

    double TargetHealth(SmithTier a_tier, const TemperTargets& a_targets) noexcept
    {
        return TargetHealth(TargetQuality(a_tier), a_targets);
    }

    TemperQuality QualityFromHealth(double a_health) noexcept
    {
        constexpr double epsilon = 0.0005;
        if (a_health >= 1.6 - epsilon) {
            return TemperQuality::kLegendary;
        }
        if (a_health >= 1.5 - epsilon) {
            return TemperQuality::kEpic;
        }
        if (a_health >= 1.4 - epsilon) {
            return TemperQuality::kFlawless;
        }
        if (a_health >= 1.3 - epsilon) {
            return TemperQuality::kExquisite;
        }
        if (a_health >= 1.2 - epsilon) {
            return TemperQuality::kSuperior;
        }
        if (a_health >= 1.1 - epsilon) {
            return TemperQuality::kFine;
        }
        return TemperQuality::kUnimproved;
    }

    TemperQuality TargetQuality(SmithTier a_tier) noexcept
    {
        switch (a_tier) {
        case SmithTier::kAdept:
            return TemperQuality::kSuperior;
        case SmithTier::kExpert:
            return TemperQuality::kEpic;
        case SmithTier::kMaster:
            return TemperQuality::kLegendary;
        }
        return TemperQuality::kUnimproved;
    }

    bool CanOfferQuality(SmithTier a_tier, TemperQuality a_quality) noexcept
    {
        return a_quality != TemperQuality::kUnimproved &&
            static_cast<std::uint8_t>(a_quality) <=
                static_cast<std::uint8_t>(TargetQuality(a_tier));
    }

    std::int32_t ImprovementPrice(
        std::int32_t a_baseValue,
        TemperQuality a_quality,
        ServiceComplexity a_complexity,
        const ImprovementPricing& a_pricing) noexcept
    {
        // Fee for the quality and size (the work) plus the item value times
        // the multiplier (the material). Works for mod-added items too: only
        // the gold value and size class are used.
        const auto value = static_cast<double>(std::max(a_baseValue, 0));
        return RoundPrice(
            ImprovementServiceFee(a_quality, a_complexity, a_pricing) +
            value * std::max(a_pricing.valueMultiplier, 0.0));
    }

    double ImprovementServiceFee(
        TemperQuality a_quality,
        ServiceComplexity a_complexity,
        const ImprovementPricing& a_pricing) noexcept
    {
        const QualityServiceFees* fees = nullptr;
        switch (a_quality) {
        case TemperQuality::kFine: fees = &a_pricing.fine; break;
        case TemperQuality::kSuperior: fees = &a_pricing.superior; break;
        case TemperQuality::kExquisite: fees = &a_pricing.exquisite; break;
        case TemperQuality::kFlawless: fees = &a_pricing.flawless; break;
        case TemperQuality::kEpic: fees = &a_pricing.epic; break;
        case TemperQuality::kLegendary: fees = &a_pricing.legendary; break;
        case TemperQuality::kUnimproved: return 0.0;
        }
        if (!fees) {
            return 0.0;
        }

        switch (a_complexity) {
        case ServiceComplexity::kSmall: return std::max(fees->smallItem, 0.0);
        case ServiceComplexity::kMedium: return std::max(fees->mediumItem, 0.0);
        case ServiceComplexity::kLarge: return std::max(fees->largeItem, 0.0);
        case ServiceComplexity::kAmmunition: return 0.0;
        }
        return 0.0;
    }

    double CraftingServiceFee(
        ServiceComplexity a_complexity,
        const CraftingPricing& a_pricing) noexcept
    {
        switch (a_complexity) {
        case ServiceComplexity::kSmall:
            return std::max(a_pricing.smallServiceFee, 0.0);
        case ServiceComplexity::kMedium:
            return std::max(a_pricing.mediumServiceFee, 0.0);
        case ServiceComplexity::kLarge:
            return std::max(a_pricing.largeServiceFee, 0.0);
        case ServiceComplexity::kAmmunition:
            return std::max(a_pricing.ammunitionServiceFee, 0.0);
        }
        return 0.0;
    }

    std::int32_t CraftingPrice(
        std::int32_t a_baseValue,
        ServiceComplexity a_complexity,
        const CraftingPricing& a_pricing,
        std::int32_t a_count) noexcept
    {
        const auto value = static_cast<double>(std::max(a_baseValue, 0));
        const auto count = static_cast<double>(std::max(a_count, 1));
        // Fixed fee for the item size (the work) plus the total item value
        // times the multiplier (the material). Both parts always count.
        const auto totalBaseValue = value * count;
        return RoundPrice(
            CraftingServiceFee(a_complexity, a_pricing) +
            totalBaseValue * std::max(a_pricing.multiplier, 0.0));
    }

    std::int32_t ApplyPriceDiscount(std::int32_t a_price, double a_discount) noexcept
    {
        const auto price = static_cast<double>(std::max(a_price, 0));
        const auto discount = std::clamp(a_discount, 0.0, 0.20);
        return AOS::RoundUpServicePrice(RoundPrice(price * (1.0 - discount)));
    }

    std::int32_t FixedBatchPrice(
        std::int32_t a_fixedPrice,
        std::int32_t a_baseValue,
        std::int32_t a_count,
        double a_discount) noexcept
    {
        const auto discounted = ApplyPriceDiscount(a_fixedPrice, a_discount);
        const auto baseValue = static_cast<std::int64_t>(std::max(a_baseValue, 0));
        const auto count = static_cast<std::int64_t>(std::max(a_count, 1));
        const auto resaleFloor = std::min<std::int64_t>(
            baseValue * count + 1,
            std::numeric_limits<std::int32_t>::max());
        return AOS::RoundUpServicePrice(std::max(discounted, static_cast<std::int32_t>(resaleFloor)));
    }

    bool ContainsWholeToken(std::string_view a_text, std::string_view a_token)
    {
        const auto textWords = Words(a_text);
        const auto tokenWords = Words(a_token);
        if (tokenWords.empty()) {
            return false;
        }

        std::string target;
        for (const auto& word : tokenWords) {
            target += word;
        }
        for (std::size_t first = 0; first < textWords.size(); ++first) {
            std::string candidate;
            for (std::size_t last = first; last < textWords.size() && candidate.size() <= target.size(); ++last) {
                candidate += textWords[last];
                if (candidate == target) {
                    return true;
                }
            }
        }
        return false;
    }

    bool IsOfficialEquipmentPlugin(std::string_view a_filename)
    {
        const auto filename = Lower(a_filename);
        return filename == "skyrim.esm" ||
            filename == "dawnguard.esm" ||
            filename == "hearthfires.esm" ||
            filename == "dragonborn.esm" ||
            filename == "artisansofskyrim.esp";
    }
}
