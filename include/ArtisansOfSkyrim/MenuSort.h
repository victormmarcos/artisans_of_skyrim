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
#include <cctype>
#include <string_view>
#include <vector>

namespace AOS
{
    [[nodiscard]] inline bool CaseInsensitiveDisplayLess(
        std::string_view a_lhs,
        std::string_view a_rhs) noexcept
    {
        const auto common = std::min(a_lhs.size(), a_rhs.size());
        for (std::size_t index = 0; index < common; ++index) {
            const auto lhs = static_cast<unsigned char>(a_lhs[index]);
            const auto rhs = static_cast<unsigned char>(a_rhs[index]);
            const auto lhsLower = static_cast<unsigned char>(std::tolower(lhs));
            const auto rhsLower = static_cast<unsigned char>(std::tolower(rhs));
            if (lhsLower != rhsLower) {
                return lhsLower < rhsLower;
            }
        }
        return a_lhs.size() < a_rhs.size();
    }

    template <class T, class Label>
    void StableSortByDisplayLabel(
        std::vector<T>& a_values,
        Label&& a_label)
    {
        std::stable_sort(
            a_values.begin(),
            a_values.end(),
            [&](const T& a_lhs, const T& a_rhs) {
                const auto lhsLabel = a_label(a_lhs);
                const auto rhsLabel = a_label(a_rhs);
                return CaseInsensitiveDisplayLess(
                    std::string_view{ lhsLabel },
                    std::string_view{ rhsLabel });
            });
    }
}
