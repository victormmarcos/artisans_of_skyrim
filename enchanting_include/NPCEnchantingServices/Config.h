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

#include "NPCEnchantingServices/Domain.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

namespace NPCES
{
    struct EnchantmentDefinition
    {
        std::string id;
        std::string displayName;
        EnchantmentType type{ EnchantmentType::kApparel };
        std::string group;
        ServiceTier minimumTier{ ServiceTier::kAdept };
        std::string sourceForm;
        bool blocked{ false };
        bool exclusive{ false };
        std::array<TierValue, 3> values;
    };

    struct UserInterface
    {
        bool enableHotkey{ false };
        std::uint32_t interactionScanCode{ 68 };
        std::size_t pageSize{ 3 };
    };

    struct Forms
    {
        std::string farengar{ "Skyrim.esm|00013BBB" };
        std::string calcelmo{ "Skyrim.esm|0001338E" };
        std::string sergius{ "Skyrim.esm|0001C23E" };
        std::string sybille{ "Skyrim.esm|000132AA" };
        std::string neloth{ "Dragonborn.esm|0001773A" };
        std::string feranSadri{ "Dawnguard.esm|00003366" };
        std::string collegeFaction{ "Skyrim.esm|0001F259" };
        std::string volkiharFaction{ "Dawnguard.esm|00003376" };
        std::string dawnguardFaction{ "Dawnguard.esm|00003375" };
        std::string potemaFirstQuestEditorID{ "MS06Start" };
        std::string nelothDwemerQuestEditorID{ "DLC2MQ04" };
    };

    struct Config
    {
        // [services] enchanting: false hides the service everywhere.
        bool serviceEnabled{ true };
        Pricing pricing;
        BarterPricing barterPricing;
        UserInterface ui;
        Forms forms;
        std::uint16_t grandSoulWeaponCharge{ 3000 };
        // Offers the six custom enchantments.
        bool enableCustomEnchantments{ true };
        std::vector<std::string> blockItems;
        std::vector<std::string> artifactKeywordTokens{
            "daedricartifact",
            "weaptypeartifact",
            "artifact"
        };
        std::vector<std::string> specialItemTokens{
            "unique",
            "artifact"
        };
        std::vector<EnchantmentDefinition> enchantments;

        [[nodiscard]] static std::filesystem::path DefaultPath();
        [[nodiscard]] static Config Load(const std::filesystem::path& a_path);
        // Builds the configuration from an already merged settings table.
        [[nodiscard]] static Config FromTable(const toml::table& a_root);
    };
}
