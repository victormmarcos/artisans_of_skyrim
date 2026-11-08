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

#include "NPCAlchemyServices/Domain.h"

#include <filesystem>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

namespace NPCAS
{
    struct AlchemyDefinition
    {
        std::string id;
        std::string displayName;
        AlchemyItemType itemType{ AlchemyItemType::kPotion };
        std::string effect;
        std::vector<std::string> groups;
        ServiceTier minimumTier{ ServiceTier::kAdept };
        std::int32_t batchSize{ 5 };
        std::int32_t batchPrice{ 0 };
        std::int32_t alchValue{ 0 };
        std::string valueText;
        std::string riskClass;
        std::string itemForm;
        std::string notes;
        bool exclusive{ false };
        bool preserveFullMenuName{ false };
    };

    struct NpcTierOverride
    {
        std::string name;
        std::string editorID;
        std::string form;
        std::string requiredQuest;
        std::string lockedReason;
        ServiceTier tier{ ServiceTier::kAdept };
    };

    struct UserInterface
    {
        bool enableHotkey{ false };
        std::uint32_t interactionScanCode{ 68 };
        std::size_t pageSize{ 3 };
    };

    struct Forms
    {
        std::string alchemyFaction{ "JobApothecaryFaction" };
        std::string wayOfVoiceQuestEditorID{ "MQ105" };
    };

    struct Config
    {
        // [services] alchemy: false hides the service everywhere.
        bool serviceEnabled{ true };
        UserInterface ui;
        BarterPricing barterPricing;
        Forms forms;
        bool allowDisplayNameFallback{ false };
        // Tier for alchemists from other mods detected by faction.
        ServiceTier modAddedAlchemistTier{ ServiceTier::kAdept };
        // Offers the Health, Magicka and Stamina Over Time potions.
        bool enableCustomPotions{ true };
        std::vector<NpcTierOverride> npcOverrides;
        std::vector<AlchemyDefinition> items;

        [[nodiscard]] static std::filesystem::path DefaultPath();
        [[nodiscard]] static Config Load(const std::filesystem::path& a_path);
        // Builds the configuration from an already merged settings table.
        [[nodiscard]] static Config FromTable(const toml::table& a_root);
    };
}
