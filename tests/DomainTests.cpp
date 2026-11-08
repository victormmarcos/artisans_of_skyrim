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

#include "NPCSmithServices/Domain.h"
#include "NPCSmithServices/Config.h"
#include "NPCSmithServices/DurationFormatter.h"
#include "NPCSmithServices/OrderDomain.h"

#include <cassert>
#include <array>

int main()
{
    using namespace NPCSS;

    assert(FormatDurationHours(0.0) == "ready now");
    assert(FormatDurationHours(0.01) == "1 hour");
    assert(FormatDurationHours(1.0) == "1 hour");
    assert(FormatDurationHours(2.0) == "2 hours");
    assert(FormatDurationHours(24.0) == "1 day");
    assert(FormatDurationHours(25.0) == "1 day, 1 hour");
    assert(FormatDurationHours(26.0) == "1 day, 2 hours");
    assert(FormatDurationHours(49.0) == "2 days, 1 hour");

    const ImprovementPricing pricing{};
    const CraftingPricing craftingPricing{};
    const TemperTargets targets{};
    const SmithingOrderDurations orderDurations{};

    assert(orderDurations.daggerHours == 24.0);
    assert(orderDurations.swordHours == 48.0);
    assert(orderDurations.bootsHours == 48.0);
    assert(orderDurations.gauntletsHours == 48.0);
    assert(orderDurations.helmetHours == 48.0);
    assert(orderDurations.lightBodyArmorHours == 48.0);
    assert(orderDurations.greatswordHours == 72.0);
    assert(orderDurations.heavyBodyArmorHours == 72.0);
    assert(orderDurations.crossbowHours == 72.0);

    assert(ParseTier("Adept") == SmithTier::kAdept);
    assert(ParseTier("EXPERT") == SmithTier::kExpert);
    assert(ParseTier("master") == SmithTier::kMaster);
    assert(!ParseTier("journeyman"));

    assert(TargetHealth(TemperQuality::kFine, targets) == 1.19);
    assert(TargetHealth(TemperQuality::kSuperior, targets) == 1.29);
    assert(TargetHealth(TemperQuality::kExquisite, targets) == 1.39);
    assert(TargetHealth(TemperQuality::kFlawless, targets) == 1.49);
    assert(TargetHealth(TemperQuality::kEpic, targets) == 1.59);
    assert(TargetHealth(TemperQuality::kLegendary, targets) == 2.0);
    assert(TargetHealth(SmithTier::kAdept, targets) == 1.29);
    assert(TargetHealth(SmithTier::kExpert, targets) == 1.59);
    assert(TargetHealth(SmithTier::kMaster, targets) == 2.0);

    assert(CanOfferQuality(SmithTier::kAdept, TemperQuality::kFine));
    assert(CanOfferQuality(SmithTier::kAdept, TemperQuality::kSuperior));
    assert(!CanOfferQuality(SmithTier::kAdept, TemperQuality::kExquisite));
    assert(CanOfferQuality(SmithTier::kExpert, TemperQuality::kEpic));
    assert(!CanOfferQuality(SmithTier::kExpert, TemperQuality::kLegendary));
    assert(CanOfferQuality(SmithTier::kMaster, TemperQuality::kLegendary));

    assert(QualityFromHealth(1.0) == TemperQuality::kUnimproved);
    assert(QualityFromHealth(1.2) == TemperQuality::kSuperior);
    assert(QualityFromHealth(1.5) == TemperQuality::kEpic);
    assert(QualityFromHealth(1.6) == TemperQuality::kLegendary);

    // Fee for the quality and size + value x 0.20.
    assert(ImprovementPrice(25, TemperQuality::kFine, ServiceComplexity::kSmall, pricing) == 205);
    assert(ImprovementPrice(25, TemperQuality::kFine, ServiceComplexity::kMedium, pricing) == 255);
    assert(ImprovementPrice(25, TemperQuality::kFine, ServiceComplexity::kLarge, pricing) == 305);
    assert(ImprovementPrice(25, TemperQuality::kEpic, ServiceComplexity::kSmall, pricing) == 1005);
    assert(ImprovementPrice(25, TemperQuality::kEpic, ServiceComplexity::kMedium, pricing) == 1255);
    assert(ImprovementPrice(25, TemperQuality::kEpic, ServiceComplexity::kLarge, pricing) == 1505);
    assert(ImprovementPrice(25, TemperQuality::kLegendary, ServiceComplexity::kSmall, pricing) == 1505);
    assert(ImprovementPrice(25, TemperQuality::kLegendary, ServiceComplexity::kMedium, pricing) == 2005);
    assert(ImprovementPrice(25, TemperQuality::kLegendary, ServiceComplexity::kLarge, pricing) == 2505);
    assert(ImprovementPrice(3200, TemperQuality::kLegendary, ServiceComplexity::kLarge, pricing) == 3140);
    assert(ImprovementPrice(10000, TemperQuality::kFine, ServiceComplexity::kSmall, pricing) == 2200);
    assert(ImprovementServiceFee(TemperQuality::kSuperior, ServiceComplexity::kSmall, pricing) == 300.0);
    assert(ImprovementServiceFee(TemperQuality::kSuperior, ServiceComplexity::kMedium, pricing) == 350.0);
    assert(ImprovementServiceFee(TemperQuality::kSuperior, ServiceComplexity::kLarge, pricing) == 400.0);
    assert(ImprovementServiceFee(TemperQuality::kExquisite, ServiceComplexity::kSmall, pricing) == 400.0);
    assert(ImprovementServiceFee(TemperQuality::kExquisite, ServiceComplexity::kMedium, pricing) == 450.0);
    assert(ImprovementServiceFee(TemperQuality::kExquisite, ServiceComplexity::kLarge, pricing) == 500.0);
    assert(ImprovementServiceFee(TemperQuality::kFlawless, ServiceComplexity::kSmall, pricing) == 500.0);
    assert(ImprovementServiceFee(TemperQuality::kFlawless, ServiceComplexity::kMedium, pricing) == 750.0);
    assert(ImprovementServiceFee(TemperQuality::kFlawless, ServiceComplexity::kLarge, pricing) == 1000.0);
    // Fee for the size + value x 1.25.
    assert(CraftingPrice(1250, ServiceComplexity::kSmall, craftingPricing) == 1963);
    assert(CraftingPrice(45, ServiceComplexity::kSmall, craftingPricing) == 456);
    assert(CraftingPrice(45, ServiceComplexity::kMedium, craftingPricing) == 756);
    assert(CraftingPrice(45, ServiceComplexity::kLarge, craftingPricing) == 1056);
    assert(CraftingPrice(125, ServiceComplexity::kLarge, craftingPricing) == 1156);
    assert(CraftingPrice(1, ServiceComplexity::kAmmunition, craftingPricing, 24) == 230);
    assert(ApplyPriceDiscount(1000, 0.05) == 950);
    assert(ApplyPriceDiscount(1000, 0.20) == 800);
    assert(ApplyPriceDiscount(1000, 2.00) == 800);
    assert(FixedBatchPrice(100, 1, 24, 0.20) == 80);
    assert(FixedBatchPrice(600, 15, 10, 0.20) == 480);
    assert(FixedBatchPrice(100, 5, 24, 0.20) == 125);  // resale floor 121, rounded up to 5

    assert(ContainsWholeToken("ArmorMaterialOrcish", "orcish"));
    assert(!ContainsWholeToken("ArmorCuirass", "orc"));
    assert(ContainsWholeToken("DLC1DwarvenCrossBow", "dwarven crossbow"));
    assert(ContainsWholeToken("Exploding Dwarven Bolt of Fire", "dwarven bolt"));

    assert(IsOfficialEquipmentPlugin("Skyrim.esm"));
    assert(IsOfficialEquipmentPlugin("DAWNGUARD.ESM"));
    assert(IsOfficialEquipmentPlugin("HearthFires.esm"));
    assert(IsOfficialEquipmentPlugin("Dragonborn.esm"));
    assert(IsOfficialEquipmentPlugin("ArtisansOfSkyrim.esp"));
    assert(!IsOfficialEquipmentPlugin("ccBGSSSE001-Fish.esm"));
    assert(!IsOfficialEquipmentPlugin("WeaponsAndArmor.esp"));
    assert(!IsOfficialEquipmentPlugin("EquipmentPatch.esl"));

    const std::array timeline{
        OrderTimelineEntry{
            0x100,
            CraftingOrderStatus::kPending,
            150.0 },
        OrderTimelineEntry{
            0x100,
            CraftingOrderStatus::kReady,
            120.0 },
        OrderTimelineEntry{
            0x200,
            CraftingOrderStatus::kPending,
            300.0 }
    };
    const auto queued = PreviewOrderSchedule(
        0x100,
        100.0,
        72.0,
        5,
        timeline);
    assert(queued.accepted);
    assert(queued.activeOrders == 2);
    assert(queued.startGameHour == 150.0);
    assert(queued.readyGameHour == 222.0);

    const auto independent = PreviewOrderSchedule(
        0x300,
        100.0,
        96.0,
        5,
        timeline);
    assert(independent.startGameHour == 100.0);
    assert(independent.readyGameHour == 196.0);

    // Different speakers at one shared workshop deliberately pass the same
    // workshop key, so both draw from the same five-slot FIFO timeline.
    const auto sharedPartner = PreviewOrderSchedule(
        0x100,
        100.0,
        48.0,
        5,
        timeline);
    assert(sharedPartner.activeOrders == 2);
    assert(sharedPartner.startGameHour == 150.0);
    assert(sharedPartner.readyGameHour == 198.0);

    // A successor uses its own key and therefore begins with a fresh queue.
    const auto freshSuccessor = PreviewOrderSchedule(
        0x101,
        100.0,
        48.0,
        5,
        timeline);
    assert(freshSuccessor.activeOrders == 0);
    assert(freshSuccessor.startGameHour == 100.0);
    assert(freshSuccessor.readyGameHour == 148.0);

    const std::array fullQueue{
        OrderTimelineEntry{ 0x100, CraftingOrderStatus::kPending, 110.0 },
        OrderTimelineEntry{ 0x100, CraftingOrderStatus::kPending, 120.0 },
        OrderTimelineEntry{ 0x100, CraftingOrderStatus::kPending, 130.0 },
        OrderTimelineEntry{ 0x100, CraftingOrderStatus::kReady, 90.0 },
        OrderTimelineEntry{ 0x100, CraftingOrderStatus::kReady, 95.0 }
    };
    assert(!PreviewOrderSchedule(0x100, 100.0, 72.0, 5, fullQueue).accepted);
    assert(IsInstantOrderCategory(CraftingOrderCategory::kArrow));
    assert(IsInstantOrderCategory(CraftingOrderCategory::kBolt));
    assert(!IsInstantOrderCategory(CraftingOrderCategory::kDagger));
    assert(ComplexityForCategory(CraftingOrderCategory::kArrow) == ServiceComplexity::kAmmunition);
    assert(ComplexityForCategory(CraftingOrderCategory::kDagger) == ServiceComplexity::kSmall);
    assert(ComplexityForCategory(CraftingOrderCategory::kSword) == ServiceComplexity::kMedium);
    assert(ComplexityForCategory(CraftingOrderCategory::kLightBodyArmor) == ServiceComplexity::kMedium);
    assert(ComplexityForCategory(CraftingOrderCategory::kGreatsword) == ServiceComplexity::kLarge);
    assert(ComplexityForCategory(CraftingOrderCategory::kHeavyBodyArmor) == ServiceComplexity::kLarge);
    assert(
        CraftingItemTypeLabel(
            CraftingOrderCategory::kHelmet,
            ArmorWeightClass::kLight) == "Light Helmet");
    assert(
        CraftingItemTypeLabel(
            CraftingOrderCategory::kHelmet,
            ArmorWeightClass::kHeavy) == "Heavy Helmet");
    assert(
        CraftingItemTypeLabel(
            CraftingOrderCategory::kBoots,
            ArmorWeightClass::kLight) == "Light Boots");
    assert(
        CraftingItemTypeLabel(
            CraftingOrderCategory::kGauntlets,
            ArmorWeightClass::kHeavy) == "Heavy Gauntlets");
    assert(
        CraftingItemTypeLabel(
            CraftingOrderCategory::kShield,
            ArmorWeightClass::kLight) == "Light Shield");
    assert(
        CraftingItemTypeLabel(
            CraftingOrderCategory::kLightBodyArmor,
            ArmorWeightClass::kLight) == "Light Body Armor");
    assert(
        CraftingItemTypeLabel(
            CraftingOrderCategory::kHeavyBodyArmor,
            ArmorWeightClass::kHeavy) == "Heavy Body Armor");
    assert(
        CraftingItemTypeLabel(
            CraftingOrderCategory::kDagger,
            ArmorWeightClass::kHeavy) == "Dagger");
    // Vanilla Leather, Dragonplate, and Dragonscale helmets use Hair +
    // Circlet slots. Their ArmorHelmet keyword must win over slot layout.
    assert(IsSupportedHelmet(true, true, false, true, true, false));
    // A modded protective helmet can use unconventional head-related slots.
    assert(IsSupportedHelmet(false, true, false, true, true, false));
    // Slot 42 alone must never turn an actual circlet into craftable armor.
    assert(!IsSupportedHelmet(false, false, false, false, true, true));
    assert(!IsSupportedHelmet(true, true, true, false, true, true));
    assert(
        UpdatedOrderStatus(
            CraftingOrderStatus::kPending,
            120.0,
            120.0,
            false) == CraftingOrderStatus::kReady);
    assert(
        UpdatedOrderStatus(
            CraftingOrderStatus::kReady,
            100.0,
            200.0,
            true) == CraftingOrderStatus::kLost);

    return 0;
}
