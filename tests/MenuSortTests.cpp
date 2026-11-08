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

#include "ArtisansOfSkyrim/MenuSort.h"

#include <cassert>
#include <string>
#include <vector>

namespace
{
    struct Option
    {
        std::string label;
        int originalPosition;
    };
}

int main()
{
    std::vector<Option> options{
        { "beta", 0 },
        { "Alpha", 1 },
        { "alpha", 2 },
        { "Gamma", 3 },
        { "ALPHA", 4 },
    };

    AOS::StableSortByDisplayLabel(
        options,
        [](const Option& a_option) -> const std::string& {
            return a_option.label;
        });

    assert(options[0].label == "Alpha");
    assert(options[1].label == "alpha");
    assert(options[2].label == "ALPHA");
    assert(options[3].label == "beta");
    assert(options[4].label == "Gamma");
    assert(options[0].originalPosition == 1);
    assert(options[1].originalPosition == 2);
    assert(options[2].originalPosition == 4);

    assert(AOS::CaseInsensitiveDisplayLess("Poison", "Potion"));
    assert(!AOS::CaseInsensitiveDisplayLess("Steel", "steel"));
    assert(!AOS::CaseInsensitiveDisplayLess("steel", "Steel"));

    return 0;
}
