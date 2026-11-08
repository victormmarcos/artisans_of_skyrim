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

#include "ArtisansOfSkyrim/RuntimeCompatibility.h"
#include <cassert>
int main()
{
    using namespace AOS::RuntimeCompatibility;
    assert(Supports(0x01050610)); // SE 1.5.97
    assert(Supports(0x01062931)); // GOG 1.6.659, real SKSE identifier
    assert(Supports(0x010649B1)); // GOG 1.6.1179, real SKSE identifier
    assert(!Supports(0x01062930)); // regression: GOG mistaken for Steam
    assert(!Supports(0x010649B0));
    assert(!Supports(Pack(1,6,1170,1))); // unverified GOG 1170 is not Steam 1170
    assert(!Supports(Pack(1,6,678,2))); // Epic
    assert(!Supports(Pack(1,4,15))); // VR
    assert(!Supports(Pack(1,7,105))); // future runtime
    for(auto version : Versions) assert(Supports(version));
}
