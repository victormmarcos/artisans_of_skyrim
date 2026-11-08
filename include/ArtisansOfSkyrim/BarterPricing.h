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

namespace AOS
{
    // Barter discount settings shared by Smithing, Enchanting, and Alchemy.
    // Each module reads the same [pricing.barter] table into this struct.
    struct BarterPricing
    {
        bool enabled{ true };
        double maxDiscount{ 0.20 };
        bool includeSpeechSkill{ true };
        double speechBaseline{ 15.0 };
        double speechSkillDiscountPerPoint{ 0.0025 };
        bool includeSpeechcraftModifiers{ true };
        bool includeHagglingPerks{ true };
    };
}
