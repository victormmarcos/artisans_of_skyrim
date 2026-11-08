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

#include "NPCEnchantingServices/Domain.h"

#include <cassert>
#include <limits>

int main()
{
    using namespace NPCES;

    assert(ParseTier("Adept") == ServiceTier::kAdept);
    assert(ParseTier("expert") == ServiceTier::kExpert);
    assert(ParseTier("MASTER") == ServiceTier::kMaster);
    assert(!ParseTier("Block"));

    assert(ParseType("Weapon") == EnchantmentType::kWeapon);
    assert(ParseType("apparel") == EnchantmentType::kApparel);
    assert(!ParseType("Spell"));

    assert(TierAllows(ServiceTier::kAdept, ServiceTier::kAdept));
    assert(!TierAllows(ServiceTier::kAdept, ServiceTier::kExpert));
    assert(TierAllows(ServiceTier::kMaster, ServiceTier::kAdept));
    assert(CanUseDual(ServiceTier::kMaster));
    assert(!CanUseDual(ServiceTier::kExpert));

    const Pricing pricing{
        .grandSoulServiceCost = 1000
    };
    assert(SinglePrice(500, pricing) == 1500);
    assert(SinglePrice(4000, pricing) == 5000);
    assert(ApplyPriceDiscount(10000, 0.05) == 9500);
    assert(ApplyPriceDiscount(10000, 0.20) == 8000);
    assert(ApplyPriceDiscount(10000, 2.00) == 8000);
    assert(ApplyPriceDiscount(10000, -1.0) == 10000);
    assert(AddPrices(3000, 5000) == 8000);
    assert(AddPrices(-100, 5000) == 5000);
    assert(AddPrices(
        std::numeric_limits<std::int32_t>::max(),
        5000) == std::numeric_limits<std::int32_t>::max());
    assert(
        SinglePrice(
            std::numeric_limits<std::int32_t>::max(),
            pricing) ==
        std::numeric_limits<std::int32_t>::max());

    return 0;
}
