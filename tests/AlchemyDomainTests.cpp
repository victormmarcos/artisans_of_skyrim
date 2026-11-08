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

#include "NPCAlchemyServices/Domain.h"

#include <cassert>

int main()
{
    using namespace NPCAS;

    assert(ParseTier("Adept") == ServiceTier::kAdept);
    assert(ParseTier("expert") == ServiceTier::kExpert);
    assert(ParseTier("MASTER") == ServiceTier::kMaster);
    assert(!ParseTier("journeyman"));

    assert(ParseItemType("Potion") == AlchemyItemType::kPotion);
    assert(ParseItemType("poison") == AlchemyItemType::kPoison);
    assert(!ParseItemType("scroll"));

    assert(TierAllows(ServiceTier::kAdept, ServiceTier::kAdept));
    assert(!TierAllows(ServiceTier::kAdept, ServiceTier::kExpert));
    assert(!TierAllows(ServiceTier::kMaster, ServiceTier::kAdept));
    assert(!TierAllows(ServiceTier::kExpert, ServiceTier::kAdept));
    assert(TierAllows(ServiceTier::kExpert, ServiceTier::kExpert));
    assert(TierAllows(ServiceTier::kMaster, ServiceTier::kMaster));

    assert(ApplyPriceDiscount(250, 0.0) == 250);
    assert(ApplyPriceDiscount(250, 0.20) == 200);
    assert(ApplyPriceDiscount(250, 2.00) == 200);
    assert(ApplyPriceDiscount(751, 0.20) == 605);  // 600.8 -> 601 -> steps of 5

    assert(MaximumSafeResaleValue(250, 5, 0.20) == 39);
    // Discounted prices round up, so 188 * 0.8 = 150.4 is charged as 151.
    assert(MinimumSafeBatchPrice(30, 5, 0.20) == 188);
    assert(IsResaleSafe(30, 188, 5, 0.20));
    assert(!IsResaleSafe(30, 187, 5, 0.20));
    assert(MinimumSafeBatchPrice(0, 5, 0.20) == 1);
    assert(MinimumSafeBatchPrice(30, 5, 0.0) == 151);
    assert(MaximumSafeResaleValue(500, 5, 0.20) == 79);
    assert(MaximumSafeResaleValue(1000, 5, 0.20) == 159);
    assert(IsResaleSafe(30, 250, 5, 0.20));
    assert(IsResaleSafe(60, 500, 5, 0.20));
    assert(IsResaleSafe(120, 1000, 5, 0.20));
    assert(!IsResaleSafe(40, 250, 5, 0.20));
    assert(!IsResaleSafe(80, 500, 5, 0.20));
    assert(!IsResaleSafe(160, 1000, 5, 0.20));

    return 0;
}
