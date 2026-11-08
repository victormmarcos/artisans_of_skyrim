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
#include <string>
#include <string_view>

namespace NPCSS
{
    [[nodiscard]] inline std::string FormatDurationHours(double a_hours)
    {
        const auto totalHours = static_cast<std::int64_t>(
            std::ceil(std::max(a_hours, 0.0)));
        if (totalHours == 0) {
            return "ready now";
        }

        const auto days = totalHours / 24;
        const auto hours = totalHours % 24;
        const auto formatUnit = [](std::int64_t a_value,
                                   std::string_view a_singular,
                                   std::string_view a_plural) {
            return std::to_string(a_value) + " " +
                std::string{ a_value == 1 ? a_singular : a_plural };
        };

        if (days > 0 && hours > 0) {
            return formatUnit(days, "day", "days") + ", " +
                formatUnit(hours, "hour", "hours");
        }
        if (days > 0) {
            return formatUnit(days, "day", "days");
        }
        return formatUnit(hours, "hour", "hours");
    }
}
