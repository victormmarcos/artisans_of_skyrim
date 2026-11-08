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

#include "ArtisansOfSkyrim/UserSettings.h"

#include "REL/Relocation.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <initializer_list>
#include <charconv>
#include <cmath>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    struct McmValue
    {
        bool isFloat{ true };
        double floatValue{ 0.0 };
        std::int64_t intValue{ 0 };
    };

    struct ParsedKey
    {
        char type{ 'f' };
        std::vector<std::string> path;
    };

    std::mutex g_mcmLock;
    std::atomic<AOS::UserSettings::ReloadHandler> g_reloadHandler{ nullptr };
    std::map<std::string, McmValue, std::less<>> g_mcmValues;

    // Paths the MCM layer may change. Discount settings stay TOML-only.
    constexpr std::array<std::string_view, 9> kMcmPrefixes{
        "pricing.crafting",
        "pricing.improvement",
        "pricing.improvementFees",
        "pricing.grandSoulServiceCost",
        "tempering.targets",
        "enchantmentPrices",
        "alchemyPrices",
        "rules.modAddedSmithTier",
        "alchemy.rules.modAddedAlchemistTier",
    };
    constexpr std::array<std::string_view, 3> kTierLabels{ "Adept", "Expert", "Master" };

    constexpr std::array<std::string_view, 3> kTierNames{ "adept", "expert", "master" };

    // One file per FOMOD checkbox; each switches on a single [features] key.
    constexpr std::array<std::string_view, 3> kInstallerOptionFiles{
        "ArtisansOfSkyrim_SkyforgeEquipment.toml",
        "ArtisansOfSkyrim_CustomEnchantments.toml",
        "ArtisansOfSkyrim_CustomPotions.toml",
    };

    [[nodiscard]] std::filesystem::path DataPath()
    {
        const std::filesystem::path executable{ REL::Module::get().filePath().data() };
        return executable.parent_path() / "Data";
    }

    [[nodiscard]] std::string_view Trim(std::string_view a_value)
    {
        constexpr std::string_view whitespace = " \t\r\n";
        const auto begin = a_value.find_first_not_of(whitespace);
        if (begin == std::string_view::npos) {
            return {};
        }
        const auto end = a_value.find_last_not_of(whitespace);
        return a_value.substr(begin, end - begin + 1);
    }

    [[nodiscard]] std::optional<ParsedKey> ParseSettingKey(std::string_view a_settingId)
    {
        // MCM Helper ids look like "fname:Section"; the section is organizational.
        const auto key = Trim(a_settingId.substr(0, a_settingId.find(':')));
        if (key.size() < 2 || (key.front() != 'f' && key.front() != 'i')) {
            return std::nullopt;
        }

        ParsedKey result;
        result.type = key.front();
        auto rest = key.substr(1);
        while (!rest.empty()) {
            const auto separator = rest.find("__");
            const auto segment = rest.substr(0, separator);
            if (segment.empty()) {
                return std::nullopt;
            }
            result.path.emplace_back(segment);
            if (separator == std::string_view::npos) {
                break;
            }
            rest = rest.substr(separator + 2);
            if (rest.empty()) {
                return std::nullopt;
            }
        }
        if (result.path.size() < 2) {
            return std::nullopt;
        }

        std::string joined;
        for (const auto& segment : result.path) {
            if (!joined.empty()) joined += '.';
            joined += segment;
        }
        const bool allowed = std::ranges::any_of(kMcmPrefixes, [&](std::string_view a_prefix) {
            return joined == a_prefix ||
                (joined.size() > a_prefix.size() && joined.starts_with(a_prefix) &&
                 joined[a_prefix.size()] == '.');
        });
        if (!allowed) {
            return std::nullopt;
        }
        return result;
    }

    [[nodiscard]] std::optional<double> ParseNumber(std::string_view a_text)
    {
        const auto text = Trim(a_text);
        double value = 0.0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value)) {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] std::string JoinPath(const std::vector<std::string>& a_path)
    {
        std::string result;
        for (const auto& segment : a_path) {
            if (!result.empty()) {
                result += '.';
            }
            result += segment;
        }
        return result;
    }

    // Integer from an int or float node, so "3000.0" in a custom file still works.
    [[nodiscard]] std::optional<std::int64_t> ReadInteger(const toml::node* a_node)
    {
        if (!a_node) {
            return std::nullopt;
        }
        if (const auto value = a_node->value<std::int64_t>()) {
            return *value;
        }
        if (const auto value = a_node->value<double>(); value && std::isfinite(*value)) {
            return static_cast<std::int64_t>(std::llround(*value));
        }
        return std::nullopt;
    }

    // Copies scalars from a_overlay into a_base, recursing into tables.
    // Arrays are never replaced so catalog entries cannot be dropped by mistake.
    void MergeTables(toml::table& a_base, const toml::table& a_overlay, const std::string& a_path)
    {
        for (auto&& [rawKey, node] : a_overlay) {
            const std::string key{ rawKey.str() };
            const auto path = a_path.empty() ? key : a_path + "." + key;

            if (const auto* overlayTable = node.as_table()) {
                auto* baseTable = a_base[key].as_table();
                if (!baseTable) {
                    if (a_base.contains(key)) {
                        SKSE::log::warn("Custom settings: '{}' must not be a table; ignored.", path);
                        continue;
                    }
                    a_base.insert(key, toml::table{});
                    baseTable = a_base[key].as_table();
                }
                MergeTables(*baseTable, *overlayTable, path);
                continue;
            }
            if (node.is_array() && a_path == "items") {
                auto* target = a_base[key].as_array();
                if (!target) {
                    a_base.insert_or_assign(key, toml::array{});
                    target = a_base[key].as_array();
                }
                for (const auto& entry : *node.as_array()) target->push_back(entry);
                continue;
            }
            if (node.is_array()) {
                SKSE::log::warn(
                    "Custom settings: arrays such as '{}' cannot be overridden; "
                    "use [enchantmentPrices] or [alchemyPrices] for item prices.",
                    path);
                continue;
            }
            if (a_base[key].is_table()) {
                SKSE::log::warn("Custom settings: '{}' must be a table; ignored.", path);
                continue;
            }

            if (const auto boolean = node.as_boolean()) {
                a_base.insert_or_assign(key, boolean->get());
            } else if (const auto integer = node.as_integer()) {
                a_base.insert_or_assign(key, integer->get());
            } else if (const auto floating = node.as_floating_point()) {
                a_base.insert_or_assign(key, floating->get());
            } else if (const auto text = node.as_string()) {
                a_base.insert_or_assign(key, text->get());
            } else {
                SKSE::log::warn("Custom settings: unsupported value type at '{}'; ignored.", path);
            }
        }
    }

    bool SetByPath(toml::table& a_root, const ParsedKey& a_key, const McmValue& a_value)
    {
        toml::table* current = std::addressof(a_root);
        for (std::size_t index = 0; index + 1 < a_key.path.size(); ++index) {
            const auto& segment = a_key.path[index];
            auto* next = (*current)[segment].as_table();
            if (!next) {
                if (current->contains(segment)) {
                    return false;
                }
                current->insert(segment, toml::table{});
                next = (*current)[segment].as_table();
            }
            current = next;
        }

        const auto& leaf = a_key.path.back();
        if ((*current)[leaf].is_table()) {
            return false;
        }
        if (a_key.type == 'i' && leaf.ends_with("Tier")) {
            // Tier steppers store 0/1/2; the configuration expects the name.
            const auto index = std::clamp<std::int64_t>(a_value.intValue, 0, 2);
            current->insert_or_assign(leaf, std::string{ kTierLabels[static_cast<std::size_t>(index)] });
        } else if (a_key.type == 'i') {
            current->insert_or_assign(leaf, a_value.intValue);
        } else {
            current->insert_or_assign(leaf, a_value.floatValue);
        }
        return true;
    }

    // Merges an optional TOML file; a malformed file is logged and skipped.
    void MergeOptionalFile(toml::table& a_root, const std::filesystem::path& a_path)
    {
        if (!std::filesystem::exists(a_path)) {
            return;
        }
        try {
            const auto overlay = toml::parse_file(a_path.string());
            MergeTables(a_root, overlay, {});
            SKSE::log::info("Applied settings from '{}'.", a_path.string());
        } catch (const toml::parse_error& error) {
            SKSE::log::error(
                "Settings '{}' could not be read and were skipped: {}",
                a_path.string(),
                error.description());
        }
    }

    // ---------------------------------------------------------------------
    // Third-party patch files: Data/SKSE/Plugins/ArtisansOfSkyrim/*.toml.
    // They may only ADD entries. Each entry is checked first, so a bad entry
    // is skipped with a log line instead of disabling a whole service.
    // ---------------------------------------------------------------------
    [[nodiscard]] std::string LowerText(std::string_view a_value)
    {
        std::string result{ a_value };
        std::ranges::transform(result, result.begin(), [](unsigned char a_char) {
            return static_cast<char>(std::tolower(a_char));
        });
        return result;
    }

    [[nodiscard]] std::string TextField(const toml::table& a_table, std::string_view a_key)
    {
        const auto value = a_table[a_key].value<std::string>();
        return value ? *value : std::string{};
    }

    [[nodiscard]] bool ValidTier(const toml::table& a_table, std::string_view a_key)
    {
        const auto value = a_table[a_key].value<std::string>();
        if (!value) return true;  // optional
        const auto tier = LowerText(*value);
        return tier == "adept" || tier == "expert" || tier == "master";
    }

    toml::array& ArrayAt(toml::table& a_root, std::initializer_list<std::string_view> a_path)
    {
        toml::table* current = std::addressof(a_root);
        const std::vector<std::string_view> path{ a_path };
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            auto* next = (*current)[path[i]].as_table();
            if (!next) {
                current->insert_or_assign(std::string{ path[i] }, toml::table{});
                next = (*current)[path[i]].as_table();
            }
            current = next;
        }
        auto* array = (*current)[path.back()].as_array();
        if (!array) {
            current->insert_or_assign(std::string{ path.back() }, toml::array{});
            array = (*current)[path.back()].as_array();
        }
        return *array;
    }

    [[nodiscard]] std::unordered_set<std::string> ExistingKeys(const toml::array& a_array, std::string_view a_key)
    {
        std::unordered_set<std::string> keys;
        for (const auto& node : a_array) {
            if (const auto* table = node.as_table()) {
                if (const auto value = (*table)[a_key].value<std::string>()) keys.insert(LowerText(*value));
            }
        }
        return keys;
    }

    // Reason a patch entry cannot be used, or empty when it is valid.
    [[nodiscard]] std::string EnchantmentProblem(const toml::table& a_entry)
    {
        if (TextField(a_entry, "id").empty()) return "missing id";
        if (TextField(a_entry, "displayName").empty()) return "missing displayName";
        if (TextField(a_entry, "sourceForm").empty()) return "missing sourceForm";
        const auto type = LowerText(TextField(a_entry, "type"));
        if (type != "weapon" && type != "apparel") return "type must be Weapon or Apparel";
        if (!ValidTier(a_entry, "minimumTier")) return "invalid minimumTier";
        if (TextField(a_entry, "group").empty()) return "missing group (the tab in the menu)";
        bool priced = false;
        for (const auto tier : kTierNames) {
            const std::string prefix{ tier };
            const auto price = a_entry[prefix + "Price"].value<std::int64_t>();
            if (!price || *price <= 0) continue;
            priced = true;
            // Every rank that is sold needs its strength and the text shown in the menu.
            if (!a_entry[prefix + "Magnitude"].value<double>()) return "missing " + prefix + "Magnitude";
            if (TextField(a_entry, prefix + "Value").empty()) return "missing " + prefix + "Value";
        }
        return priced ? std::string{} : "no positive adeptPrice/expertPrice/masterPrice";
    }

    [[nodiscard]] std::string AlchemyProblem(const toml::table& a_entry)
    {
        if (TextField(a_entry, "id").empty()) return "missing id";
        if (TextField(a_entry, "displayName").empty()) return "missing displayName";
        if (TextField(a_entry, "itemForm").empty()) return "missing itemForm";
        const auto* groups = a_entry["groups"].as_array();
        if ((!groups || groups->empty()) && TextField(a_entry, "group").empty()) return "missing groups";
        const auto type = LowerText(TextField(a_entry, "itemType"));
        if (type != "potion" && type != "poison") return "itemType must be Potion or Poison";
        if (!ValidTier(a_entry, "minimumTier")) return "invalid minimumTier";
        if (TextField(a_entry, "effect").empty()) return "missing effect";
        if (TextField(a_entry, "valueText").empty()) return "missing valueText (the strength shown in the menu)";
        const auto effect = LowerText(TextField(a_entry, "effect"));
        for (const auto blocked : { "fortify enchanting", "fortify smithing", "fortify barter" }) {
            if (effect.find(blocked) != std::string::npos) return "this effect is not sold by artisans";
        }
        const auto price = a_entry["batchPrice"].value<std::int64_t>();
        return price && *price > 0 ? std::string{} : "batchPrice must be positive";
    }

    [[nodiscard]] std::string NpcProblem(const toml::table& a_entry)
    {
        if (TextField(a_entry, "form").empty()) return "missing form";
        return ValidTier(a_entry, "tier") ? std::string{} : "tier must be Adept, Expert or Master";
    }

    // Appends the valid, new entries of a_list to the target array.
    void AppendEntries(
        const toml::array* a_list,
        toml::array& a_target,
        std::string_view a_idKey,
        std::string (*a_problem)(const toml::table&),
        std::string_view a_label,
        const std::string& a_file)
    {
        if (!a_list) return;
        auto existing = ExistingKeys(a_target, a_idKey);
        for (const auto& node : *a_list) {
            const auto* entry = node.as_table();
            if (!entry) continue;
            const auto key = LowerText(TextField(*entry, a_idKey));
            if (const auto problem = a_problem(*entry); !problem.empty()) {
                SKSE::log::warn("Patch '{}': {} '{}' skipped: {}.", a_file, a_label, key, problem);
                continue;
            }
            if (!existing.insert(key).second) {
                SKSE::log::warn("Patch '{}': {} '{}' already exists; skipped.", a_file, a_label, key);
                continue;
            }
            toml::table copy = *entry;
            // Custom Effects are reserved for Artisans' own records.
            copy.erase("exclusive");
            copy.erase("blocked");
            a_target.push_back(std::move(copy));
            SKSE::log::info("Patch '{}': added {} '{}'.", a_file, a_label, key);
        }
    }

    void ApplyPatchFiles(toml::table& a_root, const std::filesystem::path& a_folder)
    {
        std::error_code error;
        if (!std::filesystem::is_directory(a_folder, error)) return;
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(a_folder, error)) {
            if (entry.is_regular_file() && LowerText(entry.path().extension().string()) == ".toml") {
                files.push_back(entry.path());
            }
        }
        std::ranges::sort(files);
        for (const auto& path : files) {
            const auto name = path.filename().string();
            toml::table patch;
            try {
                patch = toml::parse_file(path.string());
            } catch (const toml::parse_error& parseError) {
                SKSE::log::error("Patch '{}' could not be read and was skipped: {}", name, parseError.description());
                continue;
            }
            AppendEntries(patch["enchantments"].as_array(), ArrayAt(a_root, { "enchantments" }),
                "id", &EnchantmentProblem, "enchantment", name);
            AppendEntries(patch["alchemy"]["items"].as_array(), ArrayAt(a_root, { "alchemy", "items" }),
                "id", &AlchemyProblem, "alchemy item", name);
            AppendEntries(patch["npcs"]["overrides"].as_array(), ArrayAt(a_root, { "npcs", "overrides" }),
                "form", &NpcProblem, "blacksmith", name);
            AppendEntries(patch["alchemy"]["npcs"].as_array(), ArrayAt(a_root, { "alchemy", "npcs" }),
                "form", &NpcProblem, "alchemist", name);
            // [[items.tiers]]: valid entries need a form or keyword and a tier.
            if (const auto* tiers = patch["items"]["tiers"].as_array()) {
                auto& target = ArrayAt(a_root, { "items", "tiers" });
                for (const auto& node : *tiers) {
                    const auto* entry = node.as_table();
                    if (!entry) continue;
                    const auto form = TextField(*entry, "form");
                    const auto keyword = TextField(*entry, "keyword");
                    const auto label = form.empty() ? keyword : form;
                    if (label.empty() || TextField(*entry, "tier").empty() || !ValidTier(*entry, "tier")) {
                        SKSE::log::warn("Patch '{}': item tier '{}' skipped: needs form or keyword and tier Adept, Expert or Master.", name, label);
                        continue;
                    }
                    const auto price = (*entry)["price"].value<std::int64_t>();
                    const auto count = (*entry)["count"].value<std::int64_t>();
                    if ((entry->contains("price") && (!price || *price <= 0)) ||
                        (entry->contains("count") && (!count || *count <= 0))) {
                        SKSE::log::warn("Patch '{}': item tier '{}' skipped: price and count must be whole numbers above 0.", name, label);
                        continue;
                    }
                    if (form.empty() && (price || count)) {
                        SKSE::log::warn("Patch '{}': item tier '{}': price and count only apply to a form entry; ignored.", name, label);
                    }
                    target.push_back(*entry);
                    SKSE::log::info("Patch '{}': added item tier '{}'.", name, label);
                }
            }
            for (const auto list : { "block", "allow", "special" }) {
                const auto* forms = patch["items"][list].as_array();
                if (!forms) continue;
                auto& target = ArrayAt(a_root, { "items", list });
                for (const auto& form : *forms) {
                    if (const auto value = form.value<std::string>()) target.push_back(*value);
                }
            }
            for (auto&& [key, node] : patch) {
                const std::string section{ key.str() };
                if (section != "enchantments" && section != "alchemy" && section != "npcs" && section != "items") {
                    SKSE::log::warn(
                        "Patch '{}': [{}] is not supported in patch files (use ArtisansOfSkyrim_Custom.toml).",
                        name,
                        section);
                }
            }
        }
    }

    void ApplyMcmValues(toml::table& a_root)
    {
        std::map<std::string, McmValue, std::less<>> values;
        {
            const std::scoped_lock lock{ g_mcmLock };
            values = g_mcmValues;
        }

        std::size_t applied = 0;
        for (const auto& [settingKey, value] : values) {
            const auto parsed = ParseSettingKey(settingKey);
            if (!parsed) {
                continue;
            }
            if (SetByPath(a_root, *parsed, value)) {
                ++applied;
            } else {
                SKSE::log::warn("MCM setting '{}' does not match the configuration layout.", settingKey);
            }
        }
        if (applied > 0) {
            SKSE::log::info("Applied {} MCM pricing value(s).", applied);
        }
    }

    // [enchantmentPrices.<id>] adept/expert/master -> [[enchantments]] <tier>Price.
    void ApplyEnchantmentPrices(toml::table& a_root)
    {
        const auto* prices = a_root["enchantmentPrices"].as_table();
        auto* definitions = a_root["enchantments"].as_array();
        if (!prices || !definitions) {
            return;
        }

        std::unordered_set<std::string> found;
        for (auto& node : *definitions) {
            auto* definition = node.as_table();
            const auto id = definition ? (*definition)["id"].value<std::string>() : std::nullopt;
            if (!id) {
                continue;
            }
            const auto* priceTable = (*prices)[*id].as_table();
            if (!priceTable) {
                continue;
            }
            found.insert(*id);

            for (const auto tier : kTierNames) {
                const auto price = ReadInteger(priceTable->get(tier));
                if (!price) {
                    continue;
                }
                const auto priceKey = std::string{ tier } + "Price";
                const auto current = ReadInteger(definition->get(priceKey)).value_or(0);
                if (current <= 0) {
                    SKSE::log::warn(
                        "Price for '{}' {} ignored: this effect is not offered at that rank.",
                        *id,
                        tier);
                } else if (*price <= 0) {
                    SKSE::log::warn("Price for '{}' {} must be positive; ignored.", *id, tier);
                } else {
                    definition->insert_or_assign(priceKey, *price);
                }
            }
        }

        for (auto&& [key, node] : *prices) {
            if (!found.contains(std::string{ key.str() })) {
                SKSE::log::warn("enchantmentPrices: unknown enchantment id '{}'.", key.str());
            }
        }
    }

    // [alchemyPrices] <id> = batch price -> [[alchemy.items]] batchPrice.
    void ApplyAlchemyPrices(toml::table& a_root)
    {
        const auto* prices = a_root["alchemyPrices"].as_table();
        auto* items = a_root["alchemy"]["items"].as_array();
        if (!prices || !items) {
            return;
        }

        std::unordered_set<std::string> found;
        for (auto& node : *items) {
            auto* item = node.as_table();
            const auto id = item ? (*item)["id"].value<std::string>() : std::nullopt;
            if (!id) {
                continue;
            }
            const auto price = ReadInteger(prices->get(*id));
            if (!price) {
                continue;
            }
            found.insert(*id);
            if (*price <= 0) {
                SKSE::log::warn("Batch price for '{}' must be positive; ignored.", *id);
                continue;
            }
            // The alchemy service raises unsafe prices to the resale-safe minimum.
            item->insert_or_assign("batchPrice", *price);
        }

        for (auto&& [key, node] : *prices) {
            if (!found.contains(std::string{ key.str() })) {
                SKSE::log::warn("alchemyPrices: unknown alchemy id '{}'.", key.str());
            }
        }
    }
}

namespace AOS::UserSettings
{
    std::filesystem::path PresetPath()
    {
        return DataPath() / "SKSE" / "Plugins" / "ArtisansOfSkyrim.toml";
    }

    std::filesystem::path CustomPath()
    {
        return DataPath() / "SKSE" / "Plugins" / "ArtisansOfSkyrim_Custom.toml";
    }

    std::filesystem::path McmSettingsPath()
    {
        return DataPath() / "MCM" / "Settings" / "ArtisansOfSkyrim.ini";
    }

    void LoadMcmSettingsFile()
    {
        const auto path = McmSettingsPath();
        std::ifstream input{ path };
        if (!input) {
            return;
        }

        std::map<std::string, McmValue, std::less<>> values;
        std::string line;
        while (std::getline(input, line)) {
            const auto text = Trim(line);
            if (text.empty() || text.front() == ';' || text.front() == '#' || text.front() == '[') {
                continue;
            }
            const auto equals = text.find('=');
            if (equals == std::string_view::npos) {
                continue;
            }
            const auto key = Trim(text.substr(0, equals));
            const auto parsed = ParseSettingKey(key);
            const auto number = ParseNumber(text.substr(equals + 1));
            if (!parsed || !number) {
                continue;
            }
            McmValue value;
            value.isFloat = parsed->type == 'f';
            value.floatValue = *number;
            value.intValue = static_cast<std::int64_t>(std::llround(*number));
            values.insert_or_assign(std::string{ key }, value);
        }

        const std::scoped_lock lock{ g_mcmLock };
        g_mcmValues = std::move(values);
        SKSE::log::info(
            "Read {} MCM pricing value(s) from '{}'.",
            g_mcmValues.size(),
            path.string());
    }

    std::optional<toml::table> LoadMergedTable()
    {
        const auto presetPath = PresetPath();
        if (!std::filesystem::exists(presetPath)) {
            return std::nullopt;
        }
        auto root = toml::parse_file(presetPath.string());

        const auto pluginsPath = presetPath.parent_path();
        for (const auto name : kInstallerOptionFiles) {
            MergeOptionalFile(root, pluginsPath / name);
        }
        ApplyPatchFiles(root, pluginsPath / "ArtisansOfSkyrim");
        MergeOptionalFile(root, CustomPath());

        ApplyMcmValues(root);
        ApplyEnchantmentPrices(root);
        ApplyAlchemyPrices(root);
        return root;
    }

    bool RecordMcmChange(
        std::string_view a_settingId,
        double a_floatValue,
        std::int32_t a_intValue)
    {
        const auto parsed = ParseSettingKey(a_settingId);
        if (!parsed) {
            SKSE::log::warn("Ignored MCM setting '{}': not a pricing setting.", a_settingId);
            return false;
        }

        McmValue value;
        value.isFloat = parsed->type == 'f';
        value.floatValue = std::isfinite(a_floatValue) ? a_floatValue : 0.0;
        value.intValue = a_intValue;

        const std::string key{ Trim(a_settingId.substr(0, a_settingId.find(':'))) };
        {
            const std::scoped_lock lock{ g_mcmLock };
            g_mcmValues.insert_or_assign(key, value);
        }
        SKSE::log::info(
            "MCM changed {} to {}.",
            JoinPath(parsed->path),
            value.isFloat ? std::to_string(value.floatValue) : std::to_string(value.intValue));
        return true;
    }

    void SetReloadHandler(ReloadHandler a_handler)
    {
        g_reloadHandler.store(a_handler);
    }

    void QueueReload()
    {
        const auto handler = g_reloadHandler.load();
        const auto tasks = SKSE::GetTaskInterface();
        if (!handler || !tasks) {
            return;
        }
        tasks->AddTask([handler] { handler(); });
    }
}
