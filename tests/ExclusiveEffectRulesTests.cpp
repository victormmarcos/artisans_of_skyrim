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

#include "ArtisansOfSkyrim/ExclusiveEffectRules.h"

#include <cassert>
#include <cmath>

int main()
{
    using namespace AOS;

    assert(SelectTier(0.0F, kHonedMetalMagnitudes) == EffectTier::kNone);
    assert(SelectTier(15.0F, kHonedMetalMagnitudes) == EffectTier::kExpert);
    assert(SelectTier(30.0F, kHonedMetalMagnitudes) == EffectTier::kMaster);
    assert(SelectTier(20.0F, kBerserkerSoulMagnitudes) == EffectTier::kExpert);
    assert(SelectTier(40.0F, kBerserkerSoulMagnitudes) == EffectTier::kMaster);

    assert(MagnitudesMatch(25.0F, 50.0F, kSilverBlessingMagnitudes));
    assert(!MagnitudesMatch(25.0F, 40.0F, kSilverBlessingMagnitudes));

    assert(ElementalBurstBonus(0.0F) == 0.0F);
    assert(ElementalBurstBonus(25.0F) == 25.0F);
    assert(ElementalBurstBonus(25.0F + 25.0F) == 50.0F);
    assert(ElementalBurstBonus(25.0F + 50.0F) == 75.0F);
    assert(ElementalBurstBonus(50.0F + 50.0F) == 100.0F);
    assert(ElementalBurstBonus(150.0F) == 100.0F);

    assert(std::abs(PercentageMultiplier(15.0F) - 1.15F) < 0.0001F);
    assert(std::abs(PercentageMultiplier(50.0F) - 1.50F) < 0.0001F);
    assert(PercentageMultiplier(0.0F) == 1.0F);
    assert(PercentageMultiplier(NAN) == 1.0F);

    assert(ArmorBypassDamageMultiplier(0.0F, 40.0F) == 1.0F);
    assert(ArmorBypassDamageMultiplier(100.0F, 0.0F) == 1.0F);
    assert(std::abs(
        ArmorBypassDamageMultiplier(100.0F, 20.0F) -
        (0.904F / 0.88F)) < 0.0001F);
    assert(std::abs(
        ArmorBypassDamageMultiplier(667.0F, 40.0F) -
        (0.51976F / 0.20F)) < 0.0001F);

    constexpr auto normalLoss = SprintStaminaRefund(
        7.0F,
        1.0F,
        0.30F,
        7.0F,
        1.0F);
    constexpr auto lossWithExternalDamage = SprintStaminaRefund(
        107.0F,
        1.0F,
        0.30F,
        7.0F,
        1.0F);
    assert(std::abs(normalLoss - 2.1F) < 0.0001F);
    assert(lossWithExternalDamage == normalLoss);
    assert(SprintStaminaRefund(0.0F, 1.0F, 0.30F, 7.0F, 1.0F) == 0.0F);
    assert(std::abs(
        SprintStaminaRefund(1.0F, 1.0F, 0.30F, 7.0F, 1.0F) - 0.3F) < 0.0001F);

    return 0;
}
