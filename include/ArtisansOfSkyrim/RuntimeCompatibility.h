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
#include <array>
#include <cstdint>
namespace AOS::RuntimeCompatibility
{
    // SKSE's low nibble is the distribution: 0 = Steam, 1 = GOG.
    // Do not normalize it away or reuse file-version constants for GOG.
    constexpr std::uint32_t Pack(unsigned major, unsigned minor, unsigned revision, unsigned store = 0)
    {
        return (major << 24) | (minor << 16) | (revision << 4) | store;
    }
    inline constexpr std::array Versions{
        Pack(1,5,97), Pack(1,6,317), Pack(1,6,318), Pack(1,6,323),
        Pack(1,6,342), Pack(1,6,353), Pack(1,6,629), Pack(1,6,640),
        Pack(1,6,659,1), Pack(1,6,1130), Pack(1,6,1170),
        Pack(1,6,1179,1), Pack(1,7,99), Pack(1,7,104)
    };
    constexpr bool Supports(std::uint32_t version)
    {
        return std::find(Versions.begin(), Versions.end(), version) != Versions.end();
    }
    static_assert(Versions.size() < 16); // SKSE metadata needs a zero terminator.
}
