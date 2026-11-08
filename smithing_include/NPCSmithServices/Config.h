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

#include "NPCSmithServices/Domain.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

namespace NPCSS
{
    // [[items.tiers]]: blacksmith tier needed for an item (form) or for every
    // item with a material keyword (keyword). Used for materials the mod does
    // not know, such as guild armor or equipment from other mods.
    struct ItemTierOverride
    {
        std::string form;
        std::string keyword;
        SmithTier tier{ SmithTier::kAdept };
        // Fixed price for the item (or batch). Required when the item has no
        // forge recipe or is worth 0 gold; then it is offered directly.
        std::int32_t price{ 0 };
        // Batch size for arrows and bolts without a forge recipe.
        std::int32_t count{ 0 };
    };

    struct NpcOverride
    {
        std::string name;
        std::string form;
        std::string courierLocation;
        SmithTier tier{ SmithTier::kAdept };
    };

    struct MenuLabelOverride
    {
        std::string form;
        std::string label;
        bool preserveFull{ false };
    };

    struct MaterialLabelOverride
    {
        std::string form;
        std::string label;
    };

    struct Rules
    {
        SmithTier defaultSmithTier{ SmithTier::kAdept };
        // Tier for blacksmiths from other mods detected by faction.
        SmithTier modAddedSmithTier{ SmithTier::kAdept };
        bool includeModAddedEquipment{ false };
        bool requireBlacksmithFaction{ true };
        bool requireTemperingRecipe{ true };
        bool allowRecognizedKeywordFallback{ true };
        bool allowUnknownMaterialFallbackForExpertOrMaster{ true };
        // Mod-added forge recipes that turn another weapon/armor into an item
        // (conversions of unique items) are not offered.
        bool excludeConversionRecipes{ true };
        // Mod-added forge recipes that create a unique/quest item are not offered.
        bool excludeUniqueCrafting{ true };
        bool uniqueItemsRequireMaster{ true };
        bool artifactsRequireMaster{ true };
        bool allowEnchantedItems{ true };
        bool enchantedItemsRequireExpert{ true };
        bool allowEquippedItems{ false };
        bool showBlockedItemsWithReason{ false };
        bool stalhrimOnlyBaldorForCrafting{ true };
        bool stalhrimOnlyBaldorForImprovement{ true };
        bool eorlundRequiresTrustQuest{ true };
        bool gunmarRequiresRecruitmentQuest{ true };
        bool baldorRequiresQuest{ true };
        bool dawnguardGearOnlyGunmar{ true };
        bool solstheimGearOnlySolstheimSmiths{ true };
        bool dragonGearRequiresWayOfVoice{ true };
        bool dragonGearRequiresMaster{ true };
        bool enhancedCrossbowsRequireAncientTechnology{ true };
    };

    struct UserInterface
    {
        bool enableHotkey{ false };
        std::uint32_t interactionScanCode{ 68 };  // DirectInput DIK_F10, debug only when enableHotkey=true
        std::size_t pageSize{ 3 };
    };

    struct CraftingCategories
    {
        bool weapons{ true };
        bool armor{ true };
        bool shields{ true };
        bool arrows{ true };
        bool bolts{ true };
    };

    struct SmithingOrderDurations
    {
        double daggerHours{ 24.0 };
        double swordHours{ 48.0 };
        double warAxeHours{ 48.0 };
        double maceHours{ 48.0 };
        double bootsHours{ 48.0 };
        double gauntletsHours{ 48.0 };
        double bowHours{ 48.0 };
        double shieldHours{ 48.0 };
        double helmetHours{ 48.0 };
        double greatswordHours{ 72.0 };
        double battleaxeHours{ 72.0 };
        double warhammerHours{ 72.0 };
        double lightBodyArmorHours{ 48.0 };
        double heavyBodyArmorHours{ 72.0 };
        double crossbowHours{ 72.0 };
    };

    struct OrderSettings
    {
        bool enabled{ false };
        std::size_t maximumActivePerSmith{ 5 };
        SmithingOrderDurations smithing;
    };

    struct Config
    {
        // [services] smithing: false hides the service everywhere.
        bool serviceEnabled{ true };
        ImprovementPricing improvementPricing;
        CraftingPricing craftingPricing;
        TemperTargets temperTargets;
        BarterPricing barterPricing;
        Rules rules;
        UserInterface ui;
        CraftingCategories craftingCategories;
        OrderSettings orders;
        // Eorlund's custom Skyforge Steel equipment and stats.
        bool enableSkyforgeEquipment{ true };

        std::string blacksmithFactionForm{ "Skyrim.esm|0005091D" };
        std::string eorlundForm{ "Skyrim.esm|00013B9D" };
        std::string baldorForm{ "Dragonborn.esm|00018FB5" };
        std::string gunmarForm{ "Dawnguard.esm|0000336C" };
        std::string gloverForm{ "Dragonborn.esm|0001828E" };
        std::string eorlundProvingHonorQuestEditorID{ "C01" };
        std::string eorlundMissingInActionQuestEditorID{ "MS09" };
        std::string gunmarRecruitmentQuestEditorID{ "DLC1HunterBaseIntro" };
        std::string baldorQuestEditorID{ "DLC2SV02" };
        std::string wayOfVoiceQuestEditorID{ "MQ105" };
        std::string skyforgeUnlockQuestEditorID{ "C06" };
        // This EditorID is configurable because Dawnguard schematic unlocks are quest/stage driven.
        // If the local runtime/modlist uses a different gate, override it in TOML or disable the rule.
        std::string ancientTechnologyQuestEditorID{ "DLC1RH05" };

        std::vector<NpcOverride> npcOverrides;
        std::vector<std::string> allowItems;
        // [items] block: the player's and patches' blocks (always win).
        std::vector<std::string> blockItems;
        // [items] defaultBlock: the mod's own choices; an item named in a
        // patch ([[items.tiers]] form or allow) is offered anyway.
        std::vector<std::string> defaultBlockItems;
        std::vector<std::string> specialItems;
        std::vector<ItemTierOverride> itemTiers;
        std::vector<MenuLabelOverride> menuLabelOverrides;
        std::vector<MaterialLabelOverride> materialLabelOverrides;

        std::vector<std::string> nordHeroWeapons{
            "Skyrim.esm|00068C57",
            "Skyrim.esm|00068C63",
            "Skyrim.esm|00068C79",
            "Skyrim.esm|00068C7B",
            "Skyrim.esm|000A3115"
        };
        std::vector<std::string> ancientNordArmor{
            "Skyrim.esm|00018388",
            "Skyrim.esm|00056A9D",
            "Skyrim.esm|00056A9E",
            "Skyrim.esm|00056B17"
        };
        std::vector<std::string> manualDawnguardItems{
            "Dawnguard.esm|0000D098",
            "Dawnguard.esm|0000DD55",
            "Dawnguard.esm|0000F3FB",
            "Dawnguard.esm|0000F402",
            "Dawnguard.esm|0000F404",
            "Dawnguard.esm|0000F400",
            "Dawnguard.esm|0000F3FE",
            "Dawnguard.esm|0001989E",
            "Dawnguard.esm|0000F3F7",
            "Dawnguard.esm|0000F3FA",
            "Dawnguard.esm|00014757",
            "Dawnguard.esm|00014758",
            "Dawnguard.esm|000050D0",
            "Dawnguard.esm|000150B8"
        };
        std::vector<std::string> manualWolfItems{
            "Skyrim.esm|000CAE15",
            "Skyrim.esm|000CEE7C",
            "Skyrim.esm|000CEE7E",
            "Skyrim.esm|0004C3D0"
        };
        std::vector<std::string> manualAdeptItems{
            "Skyrim.esm|000135B8",
            "Skyrim.esm|00013841",
            "Skyrim.esm|00013985"
        };
        std::vector<std::string> ancientNordWeaponKeywords{
            "WeapMaterialDraugr",
            "WeapMaterialDraugrHoned"
        };

        std::vector<std::string> temperingBenchEditorIDs{
            "CraftingSmithingArmorTable",
            "CraftingSmithingSharpeningWheel"
        };
        std::vector<std::string> forgeBenchEditorIDs{
            "CraftingSmithingForge",
            "CraftingSmithingSkyforge"
        };
        std::vector<std::string> adeptMaterialTokens{
            "iron", "steel", "leather", "hide", "studded", "imperial"
        };
        std::vector<std::string> expertMaterialTokens{
            "steelplate", "scaled", "silver", "dwarven", "dwemer", "dwarf", "orcish", "elven", "nordic",
            "glass", "ebony", "bonemold", "chitin"
        };
        std::vector<std::string> masterMaterialTokens{
            "daedric", "dragonbone", "dragonplate", "dragonscale", "stalhrim"
        };
        std::vector<std::string> artifactKeywordTokens{
            "daedricartifact", "weaptypeartifact", "artifact"
        };
        std::vector<std::string> dragonGearTokens{
            "dragonbone", "dragonplate", "dragonscale"
        };
        std::vector<std::string> dawnguardGearTokens{
            "dawnguard", "crossbow", "bolt"
        };
        std::vector<std::string> enhancedCrossbowTokens{
            "enhancedcrossbow", "enhanced crossbow", "enhanceddwarvencrossbow",
            "enhanced dwarven crossbow", "dwarvencrossbow", "dwarven crossbow",
            "dwarvenbolt", "dwarven bolt", "exploding", "explodingbolt", "exploding bolt"
        };
        std::vector<std::string> solstheimGearTokens{
            "bonemold", "chitin"
        };

        [[nodiscard]] static std::filesystem::path DefaultPath();
        [[nodiscard]] static Config Load(const std::filesystem::path& a_path);
        // Builds the configuration from an already merged settings table.
        [[nodiscard]] static Config FromTable(const toml::table& a_root);
    };
}
