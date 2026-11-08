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

#include "NPCEnchantingServices/Config.h"

#include "ArtisansOfSkyrim/ExclusiveEffectRules.h"
#include "REL/Relocation.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace
{
    [[nodiscard]] std::string Lower(std::string_view a_value)
    {
        std::string result{ a_value };
        std::ranges::transform(result, result.begin(), [](unsigned char a_char) {
            return static_cast<char>(std::tolower(a_char));
        });
        return result;
    }

    template <class T>
    void ReadValue(
        const toml::table& a_table,
        std::string_view a_key,
        T& a_target)
    {
        if (const auto value = a_table[a_key].value<T>()) {
            a_target = *value;
        }
    }

    void ReadStringArray(
        const toml::table& a_parent,
        std::string_view a_key,
        std::vector<std::string>& a_target)
    {
        const auto* array = a_parent[a_key].as_array();
        if (!array) {
            return;
        }

        std::vector<std::string> values;
        values.reserve(array->size());
        for (const auto& node : *array) {
            if (const auto value = node.value<std::string>()) {
                values.push_back(*value);
            }
        }
        a_target = std::move(values);
    }

    [[nodiscard]] std::optional<float> ReadOptionalFloat(
        const toml::table& a_table,
        std::string_view a_key)
    {
        if (const auto value = a_table[a_key].value<double>()) {
            return static_cast<float>(*value);
        }
        if (const auto value = a_table[a_key].value<std::int64_t>()) {
            return static_cast<float>(*value);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<AOS::TierMagnitudes>
    SupportedMagnitudes(std::string_view a_id)
    {
        if (a_id == "silver_blessing") {
            return AOS::kSilverBlessingMagnitudes;
        }
        if (a_id == "berserker_soul") {
            return AOS::kBerserkerSoulMagnitudes;
        }
        if (a_id == "honed_metal") {
            return AOS::kHonedMetalMagnitudes;
        }
        if (a_id == "unburden") {
            return AOS::kUnburdenMagnitudes;
        }
        if (a_id == "reinforcement") {
            return AOS::kReinforcementMagnitudes;
        }
        if (a_id == "elemental_burst") {
            return AOS::kElementalBurstMagnitudes;
        }
        return std::nullopt;
    }

    void ReadTierValue(
        const toml::table& a_table,
        std::string_view a_prefix,
        NPCES::TierValue& a_target)
    {
        const auto prefix = std::string{ a_prefix };
        a_target.magnitude = ReadOptionalFloat(
            a_table,
            prefix + "Magnitude");
        if (const auto duration =
                a_table[prefix + "Duration"].value<std::int64_t>()) {
            a_target.duration = static_cast<std::uint32_t>(
                std::clamp<std::int64_t>(
                    *duration,
                    0,
                    static_cast<std::int64_t>(
                        std::numeric_limits<std::uint32_t>::max())));
        }
        if (const auto price =
                a_table[prefix + "Price"].value<std::int64_t>()) {
            a_target.price = static_cast<std::int32_t>(
                std::clamp<std::int64_t>(
                    *price,
                    0,
                    static_cast<std::int64_t>(
                        std::numeric_limits<std::int32_t>::max())));
        }
        a_target.chargeCost = ReadOptionalFloat(
            a_table,
            prefix + "ChargeCost");
        ReadValue(a_table, prefix + "Value", a_target.valueText);
    }

    void Validate(NPCES::Config& a_config)
    {
        a_config.pricing.grandSoulServiceCost =
            std::max(a_config.pricing.grandSoulServiceCost, 0);
        a_config.barterPricing.maxDiscount =
            std::clamp(a_config.barterPricing.maxDiscount, 0.0, 0.20);
        a_config.barterPricing.speechBaseline =
            std::max(a_config.barterPricing.speechBaseline, 0.0);
        a_config.barterPricing.speechSkillDiscountPerPoint =
            std::max(
                a_config.barterPricing.speechSkillDiscountPerPoint,
                0.0);
        a_config.ui.pageSize =
            std::clamp<std::size_t>(a_config.ui.pageSize, 1, 3);
        a_config.grandSoulWeaponCharge =
            std::max<std::uint16_t>(a_config.grandSoulWeaponCharge, 1);

        std::unordered_set<std::string> ids;
        for (auto& definition : a_config.enchantments) {
            if (definition.id.empty()) {
                throw std::runtime_error(
                    "An enchantment definition has no id.");
            }
            if (!ids.insert(definition.id).second) {
                throw std::runtime_error(
                    "Duplicate enchantment id: " + definition.id);
            }
            if (definition.displayName.empty()) {
                throw std::runtime_error(
                    "Enchantment '" + definition.id +
                    "' has no displayName.");
            }
            if (!definition.blocked && definition.sourceForm.empty()) {
                throw std::runtime_error(
                    "Enchantment '" + definition.id +
                    "' has no sourceForm.");
            }
            if (definition.exclusive) {
                const auto supported = SupportedMagnitudes(definition.id);
                if (!supported) {
                    throw std::runtime_error(
                        "Unsupported exclusive enchantment: " + definition.id);
                }
                if (definition.minimumTier != NPCES::ServiceTier::kExpert) {
                    throw std::runtime_error(
                        "Exclusive enchantment '" + definition.id +
                        "' must begin at Expert.");
                }
                const auto& adept = definition.values[
                    NPCES::TierIndex(NPCES::ServiceTier::kAdept)];
                const auto& expert = definition.values[
                    NPCES::TierIndex(NPCES::ServiceTier::kExpert)];
                const auto& master = definition.values[
                    NPCES::TierIndex(NPCES::ServiceTier::kMaster)];
                if (adept.Available() || !expert.magnitude || !master.magnitude ||
                    !AOS::MagnitudesMatch(
                        *expert.magnitude,
                        *master.magnitude,
                        *supported)) {
                    throw std::runtime_error(
                        "Exclusive enchantment '" + definition.id +
                        "' does not match its supported Expert/Master magnitudes.");
                }
                if (!expert.Available() || !master.Available()) {
                    throw std::runtime_error(
                        "Exclusive enchantment '" + definition.id +
                        "' must have positive Expert and Master prices.");
                }
            }
        }
    }
}

namespace NPCES
{
    std::filesystem::path Config::DefaultPath()
    {
        const std::filesystem::path executable{
            REL::Module::get().filePath().data()
        };
        return executable.parent_path() / "Data" / "SKSE" / "Plugins" /
            "ArtisansOfSkyrim.toml";
    }

    Config Config::Load(const std::filesystem::path& a_path)
    {
        return FromTable(toml::parse_file(a_path.string()));
    }

    Config Config::FromTable(const toml::table& a_root)
    {
        Config result;
        const auto& root = a_root;

        if (const auto* table = root["services"].as_table()) {
            ReadValue(*table, "enchanting", result.serviceEnabled);
        }

        if (const auto* table = root["features"].as_table()) {
            // 1.7 used one switch for all custom content; it stays as the
            // fallback for older configuration files.
            ReadValue(*table, "enableExclusiveEffects", result.enableCustomEnchantments);
            ReadValue(*table, "enableCustomEnchantments", result.enableCustomEnchantments);
        }

        if (const auto* table = root["pricing"].as_table()) {
            ReadValue(
                *table,
                "grandSoulServiceCost",
                result.pricing.grandSoulServiceCost);
            std::int64_t weaponCharge = result.grandSoulWeaponCharge;
            ReadValue(*table, "grandSoulWeaponCharge", weaponCharge);
            result.grandSoulWeaponCharge = static_cast<std::uint16_t>(
                std::clamp<std::int64_t>(weaponCharge, 1, 65535));
        }

        if (const auto* table = root["pricing"]["barter"].as_table()) {
            ReadValue(*table, "enabled", result.barterPricing.enabled);
            ReadValue(
                *table,
                "maxDiscount",
                result.barterPricing.maxDiscount);
            ReadValue(
                *table,
                "includeSpeechSkill",
                result.barterPricing.includeSpeechSkill);
            ReadValue(
                *table,
                "speechBaseline",
                result.barterPricing.speechBaseline);
            ReadValue(
                *table,
                "speechSkillDiscountPerPoint",
                result.barterPricing.speechSkillDiscountPerPoint);
            ReadValue(
                *table,
                "includeSpeechcraftModifiers",
                result.barterPricing.includeSpeechcraftModifiers);
            ReadValue(
                *table,
                "includeHagglingPerks",
                result.barterPricing.includeHagglingPerks);
        }

        if (const auto* table = root["ui"].as_table()) {
            ReadValue(*table, "enableHotkey", result.ui.enableHotkey);
            std::int64_t scanCode = result.ui.interactionScanCode;
            std::int64_t pageSize =
                static_cast<std::int64_t>(result.ui.pageSize);
            ReadValue(*table, "interactionScanCode", scanCode);
            ReadValue(*table, "pageSize", pageSize);
            result.ui.interactionScanCode =
                static_cast<std::uint32_t>(
                    std::clamp<std::int64_t>(scanCode, 0, 255));
            result.ui.pageSize = static_cast<std::size_t>(
                std::max<std::int64_t>(pageSize, 1));
        }

        if (const auto* table = root["forms"].as_table()) {
            ReadValue(*table, "farengar", result.forms.farengar);
            ReadValue(*table, "calcelmo", result.forms.calcelmo);
            ReadValue(*table, "sergius", result.forms.sergius);
            ReadValue(*table, "sybille", result.forms.sybille);
            ReadValue(*table, "neloth", result.forms.neloth);
            ReadValue(*table, "feranSadri", result.forms.feranSadri);
            ReadValue(
                *table,
                "collegeFaction",
                result.forms.collegeFaction);
            ReadValue(
                *table,
                "volkiharFaction",
                result.forms.volkiharFaction);
            ReadValue(
                *table,
                "dawnguardFaction",
                result.forms.dawnguardFaction);
            ReadValue(
                *table,
                "potemaFirstQuestEditorID",
                result.forms.potemaFirstQuestEditorID);
            ReadValue(
                *table,
                "nelothDwemerQuestEditorID",
                result.forms.nelothDwemerQuestEditorID);
        }

        if (const auto* table = root["items"].as_table()) {
            ReadStringArray(*table, "block", result.blockItems);
            ReadStringArray(
                *table,
                "artifactKeywordTokens",
                result.artifactKeywordTokens);
            ReadStringArray(
                *table,
                "specialItemTokens",
                result.specialItemTokens);
        }

        if (const auto* array = root["enchantments"].as_array()) {
            result.enchantments.clear();
            result.enchantments.reserve(array->size());
            for (const auto& node : *array) {
                const auto* table = node.as_table();
                if (!table) {
                    continue;
                }

                EnchantmentDefinition definition;
                ReadValue(*table, "id", definition.id);
                ReadValue(
                    *table,
                    "displayName",
                    definition.displayName);
                ReadValue(*table, "group", definition.group);
                ReadValue(*table, "sourceForm", definition.sourceForm);
                ReadValue(*table, "blocked", definition.blocked);
                ReadValue(*table, "exclusive", definition.exclusive);

                if (const auto type =
                        table->get_as<std::string>("type")) {
                    const auto parsed = ParseType(type->get());
                    if (!parsed) {
                        throw std::runtime_error(
                            "Invalid type for enchantment '" +
                            definition.id + "'.");
                    }
                    definition.type = *parsed;
                }

                if (const auto minimum =
                        table->get_as<std::string>("minimumTier")) {
                    if (Lower(minimum->get()) == "block") {
                        definition.blocked = true;
                    } else {
                        const auto parsed = ParseTier(minimum->get());
                        if (!parsed) {
                            throw std::runtime_error(
                                "Invalid minimumTier for enchantment '" +
                                definition.id + "'.");
                        }
                        definition.minimumTier = *parsed;
                    }
                }

                ReadTierValue(
                    *table,
                    "adept",
                    definition.values[TierIndex(ServiceTier::kAdept)]);
                ReadTierValue(
                    *table,
                    "expert",
                    definition.values[TierIndex(ServiceTier::kExpert)]);
                ReadTierValue(
                    *table,
                    "master",
                    definition.values[TierIndex(ServiceTier::kMaster)]);
                result.enchantments.push_back(std::move(definition));
            }
        }

        Validate(result);
        return result;
    }
}
