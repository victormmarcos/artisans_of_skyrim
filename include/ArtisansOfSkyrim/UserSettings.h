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

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include <toml++/toml.hpp>

// Player-adjustable configuration layers, lowest to highest priority:
//   1. Data/SKSE/Plugins/ArtisansOfSkyrim.toml         (FOMOD preset)
//   2. Data/SKSE/Plugins/ArtisansOfSkyrim_<Option>.toml (FOMOD checkboxes:
//      SkyforgeEquipment, CustomEnchantments, CustomPotions)
//   +  Data/SKSE/Plugins/ArtisansOfSkyrim/*.toml        (third-party patches:
//      add enchantments, alchemy items, NPC tiers and item lists)
//   3. Data/SKSE/Plugins/ArtisansOfSkyrim_Custom.toml  (optional, player edits)
//   4. Data/MCM/Settings/ArtisansOfSkyrim.ini          (optional, MCM Helper)
//
// The custom TOML is merged table by table; arrays such as [[enchantments]]
// are never replaced. Per-item prices use dedicated tables instead:
//
//   [enchantmentPrices.fortify_health]
//   master = 3000
//
//   [alchemyPrices]
//   potion_health_over_time_master = 1200
//
// MCM setting ids encode the TOML path with "__" separators after the MCM
// Helper type prefix, for example "fpricing__crafting__craftMultiplier" or
// "ienchantmentPrices__fortify_health__master". Only pricing paths are
// accepted from the MCM layer.
namespace AOS::UserSettings
{
    [[nodiscard]] std::filesystem::path PresetPath();
    [[nodiscard]] std::filesystem::path CustomPath();
    [[nodiscard]] std::filesystem::path McmSettingsPath();

    // Reads the MCM Helper settings file once. Later changes arrive through
    // RecordMcmChange while the game runs.
    void LoadMcmSettingsFile();

    // Preset + custom TOML + MCM values. Returns nullopt when the preset file
    // is missing. Throws toml::parse_error for a malformed preset; a malformed
    // custom file is logged and skipped.
    [[nodiscard]] std::optional<toml::table> LoadMergedTable();

    // Stores one MCM value in memory. The setting id may include the MCM
    // Helper ":Section" suffix. Returns false for ids outside the pricing paths.
    bool RecordMcmChange(
        std::string_view a_settingId,
        double a_floatValue,
        std::int32_t a_intValue);

    // Function that re-applies prices after a change; set once at plugin load.
    using ReloadHandler = void (*)();
    void SetReloadHandler(ReloadHandler a_handler);

    // Runs the reload handler on the game task queue.
    void QueueReload();
}
