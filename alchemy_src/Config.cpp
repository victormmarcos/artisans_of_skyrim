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

#include "NPCAlchemyServices/Config.h"

#include "REL/Relocation.h"

#include <algorithm>
#include <cctype>
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
        for (const auto& node : *array) {
            if (const auto value = node.value<std::string>(); value && !value->empty()) {
                a_target.push_back(*value);
            }
        }
    }

    void Validate(NPCAS::Config& a_config)
    {
        a_config.barterPricing.maxDiscount =
            std::clamp(a_config.barterPricing.maxDiscount, 0.0, 0.20);
        a_config.barterPricing.speechBaseline =
            std::max(a_config.barterPricing.speechBaseline, 0.0);
        a_config.barterPricing.speechSkillDiscountPerPoint =
            std::max(a_config.barterPricing.speechSkillDiscountPerPoint, 0.0);
        a_config.ui.pageSize =
            std::clamp<std::size_t>(a_config.ui.pageSize, 1, 3);

        std::unordered_set<std::string> ids;
        for (auto& item : a_config.items) {
            if (item.id.empty()) {
                throw std::runtime_error("An alchemy item has no id.");
            }
            if (!ids.insert(item.id).second) {
                throw std::runtime_error("Duplicate alchemy id: " + item.id);
            }
            if (item.displayName.empty()) {
                throw std::runtime_error(
                    "Alchemy item '" + item.id + "' has no displayName.");
            }
            if (item.itemForm.empty()) {
                throw std::runtime_error(
                    "Alchemy item '" + item.id + "' has no itemForm.");
            }
            if (item.groups.empty()) {
                throw std::runtime_error(
                    "Alchemy item '" + item.id + "' has no groups.");
            }
            std::unordered_set<std::string> groups;
            std::erase_if(item.groups, [&](const std::string& a_group) {
                return a_group.empty() || !groups.insert(a_group).second;
            });
            item.batchSize = std::max(item.batchSize, 1);
            item.batchPrice = std::max(item.batchPrice, 0);
            item.alchValue = std::max(item.alchValue, 0);
            if (!NPCAS::IsResaleSafe(
                    item.alchValue,
                    item.batchPrice,
                    item.batchSize,
                    a_config.barterPricing.maxDiscount)) {
                // A player-set price below this point would let the batch be
                // resold at a profit, so raise it instead of failing the service.
                const auto safePrice = NPCAS::MinimumSafeBatchPrice(
                    item.alchValue,
                    item.batchSize,
                    a_config.barterPricing.maxDiscount);
                SKSE::log::warn(
                    "Alchemy item '{}': batch price {} can be resold at a profit; using {}.",
                    item.id,
                    item.batchPrice,
                    safePrice);
                item.batchPrice = safePrice;
            }

            const auto effect = Lower(item.effect);
            if (effect.find("fortify enchanting") != std::string::npos ||
                effect.find("fortify smithing") != std::string::npos ||
                effect.find("fortify barter") != std::string::npos) {
                throw std::runtime_error(
                    "Blocked alchemy effect was reintroduced: " + item.effect);
            }
        }
    }
}

namespace NPCAS
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
            ReadValue(*table, "alchemy", result.serviceEnabled);
        }

        if (const auto* table = root["features"].as_table()) {
            // 1.7 used one switch for all custom content; it stays as the
            // fallback for older configuration files.
            ReadValue(*table, "enableExclusiveEffects", result.enableCustomPotions);
            ReadValue(*table, "enableCustomPotions", result.enableCustomPotions);
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

        if (const auto* table = root["pricing"]["barter"].as_table()) {
            ReadValue(*table, "enabled", result.barterPricing.enabled);
            ReadValue(*table, "maxDiscount", result.barterPricing.maxDiscount);
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

        if (const auto* table = root["forms"].as_table()) {
            ReadValue(*table, "alchemyFaction", result.forms.alchemyFaction);
            ReadValue(
                *table,
                "wayOfVoiceQuestEditorID",
                result.forms.wayOfVoiceQuestEditorID);
        }

        if (const auto* table = root["alchemy"]["rules"].as_table()) {
            ReadValue(
                *table,
                "allowDisplayNameFallback",
                result.allowDisplayNameFallback);
            if (const auto tier = table->get_as<std::string>("modAddedAlchemistTier")) {
                if (const auto parsed = ParseTier(tier->get())) {
                    result.modAddedAlchemistTier = *parsed;
                }
            }
        }

        if (const auto* array = root["alchemy"]["npcs"].as_array()) {
            result.npcOverrides.clear();
            result.npcOverrides.reserve(array->size());
            for (const auto& node : *array) {
                const auto* table = node.as_table();
                if (!table) {
                    continue;
                }

                NpcTierOverride entry;
                ReadValue(*table, "name", entry.name);
                ReadValue(*table, "editorID", entry.editorID);
                ReadValue(*table, "form", entry.form);
                ReadValue(*table, "requiredQuest", entry.requiredQuest);
                ReadValue(*table, "lockedReason", entry.lockedReason);
                if (const auto tier = table->get_as<std::string>("tier")) {
                    const auto parsed = ParseTier(tier->get());
                    if (!parsed) {
                        throw std::runtime_error(
                            "Invalid alchemy NPC tier for '" + entry.name + "'.");
                    }
                    entry.tier = *parsed;
                }
                if (!entry.name.empty() || !entry.editorID.empty() || !entry.form.empty()) {
                    result.npcOverrides.push_back(std::move(entry));
                }
            }
        }

        if (const auto* array = root["alchemy"]["items"].as_array()) {
            result.items.clear();
            result.items.reserve(array->size());
            for (const auto& node : *array) {
                const auto* table = node.as_table();
                if (!table) {
                    continue;
                }

                AlchemyDefinition item;
                ReadValue(*table, "id", item.id);
                ReadValue(*table, "displayName", item.displayName);
                ReadValue(*table, "effect", item.effect);
                ReadStringArray(*table, "groups", item.groups);
                if (item.groups.empty()) {
                    std::string legacyGroup;
                    ReadValue(*table, "group", legacyGroup);
                    if (!legacyGroup.empty()) {
                        item.groups.push_back(std::move(legacyGroup));
                    }
                }
                ReadValue(*table, "batchSize", item.batchSize);
                ReadValue(*table, "batchPrice", item.batchPrice);
                ReadValue(*table, "alchValue", item.alchValue);
                ReadValue(*table, "valueText", item.valueText);
                ReadValue(*table, "riskClass", item.riskClass);
                ReadValue(*table, "itemForm", item.itemForm);
                ReadValue(*table, "notes", item.notes);
                ReadValue(*table, "exclusive", item.exclusive);
                ReadValue(*table, "preserveFullMenuName", item.preserveFullMenuName);

                if (const auto type = table->get_as<std::string>("itemType")) {
                    const auto parsed = ParseItemType(type->get());
                    if (!parsed) {
                        throw std::runtime_error(
                            "Invalid itemType for alchemy item '" + item.id + "'.");
                    }
                    item.itemType = *parsed;
                }
                if (const auto tier = table->get_as<std::string>("minimumTier")) {
                    const auto parsed = ParseTier(tier->get());
                    if (!parsed) {
                        throw std::runtime_error(
                            "Invalid minimumTier for alchemy item '" + item.id + "'.");
                    }
                    item.minimumTier = *parsed;
                }

                result.items.push_back(std::move(item));
            }
        }

        Validate(result);
        return result;
    }
}
