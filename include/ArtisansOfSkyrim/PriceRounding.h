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

#include <cstdint>
#include <limits>

namespace AOS
{
    // Final service prices are charged in steps of 5 gold. Rounding up keeps
    // every price at or above its resale-safety minimum.
    inline constexpr std::int32_t kPriceStep = 5;

    [[nodiscard]] constexpr std::int32_t RoundUpServicePrice(std::int32_t a_price) noexcept
    {
        if (a_price <= 0) {
            return 0;
        }
        constexpr auto maximum = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
        const auto rounded =
            (static_cast<std::int64_t>(a_price) + kPriceStep - 1) / kPriceStep * kPriceStep;
        return static_cast<std::int32_t>(rounded > maximum ? maximum - maximum % kPriceStep : rounded);
    }
}
