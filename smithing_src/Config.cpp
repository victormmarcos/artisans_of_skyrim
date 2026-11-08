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

#include "NPCSmithServices/Config.h"

#include "REL/Relocation.h"

#include <algorithm>
#include <stdexcept>

namespace
{
    template <class T>
    void ReadValue(const toml::table& a_table, std::string_view a_key, T& a_target)
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

    void Validate(NPCSS::Config& a_config)
    {
        auto& pricing = a_config.improvementPricing;
        pricing.valueMultiplier = std::max(pricing.valueMultiplier, 0.0);
        for (auto* fees : {
                 &pricing.fine,
                 &pricing.superior,
                 &pricing.exquisite,
                 &pricing.flawless,
                 &pricing.epic,
                 &pricing.legendary }) {
            fees->smallItem = std::max(fees->smallItem, 0.0);
            fees->mediumItem = std::max(fees->mediumItem, 0.0);
            fees->largeItem = std::max(fees->largeItem, 0.0);
        }

        auto& crafting = a_config.craftingPricing;
        crafting.multiplier = std::max(crafting.multiplier, 0.0);
        crafting.smallServiceFee = std::max(crafting.smallServiceFee, 0.0);
        crafting.mediumServiceFee = std::max(crafting.mediumServiceFee, 0.0);
        crafting.largeServiceFee = std::max(crafting.largeServiceFee, 0.0);
        crafting.ammunitionServiceFee = std::max(crafting.ammunitionServiceFee, 0.0);

        auto& barter = a_config.barterPricing;
        barter.maxDiscount = std::clamp(barter.maxDiscount, 0.0, 0.20);
        barter.speechBaseline = std::max(barter.speechBaseline, 0.0);
        barter.speechSkillDiscountPerPoint = std::max(barter.speechSkillDiscountPerPoint, 0.0);

        auto& targets = a_config.temperTargets;
        targets.fine = std::clamp(targets.fine, 1.1, 1.199);
        targets.superior = std::clamp(targets.superior, 1.2, 1.299);
        targets.exquisite = std::clamp(targets.exquisite, 1.3, 1.399);
        targets.flawless = std::clamp(targets.flawless, 1.4, 1.499);
        targets.epic = std::clamp(targets.epic, 1.5, 1.599);
        targets.legendary = std::max(targets.legendary, 1.6);

        a_config.ui.pageSize = std::clamp<std::size_t>(a_config.ui.pageSize, 1, 5);
        a_config.orders.maximumActivePerSmith =
            std::clamp<std::size_t>(
                a_config.orders.maximumActivePerSmith,
                1,
                5);
        auto& durations = a_config.orders.smithing;
        for (auto* duration : {
                 &durations.daggerHours,
                 &durations.swordHours,
                 &durations.warAxeHours,
                 &durations.maceHours,
                 &durations.bootsHours,
                 &durations.gauntletsHours,
                 &durations.bowHours,
                 &durations.shieldHours,
                 &durations.helmetHours,
                 &durations.greatswordHours,
                 &durations.battleaxeHours,
                 &durations.warhammerHours,
                 &durations.lightBodyArmorHours,
                 &durations.heavyBodyArmorHours,
                 &durations.crossbowHours }) {
            *duration = std::max(*duration, 1.0);
        }
    }
}

namespace NPCSS
{
    std::filesystem::path Config::DefaultPath()
    {
        const std::filesystem::path executable{ REL::Module::get().filePath().data() };
        return executable.parent_path() / "Data" / "SKSE" / "Plugins" / "ArtisansOfSkyrim.toml";
    }

    Config Config::Load(const std::filesystem::path& a_path)
    {
        return FromTable(toml::parse_file(a_path.string()));
    }

    Config Config::FromTable(const toml::table& a_root)
    {
        Config result;
        const auto& root = a_root;

        if (const auto* table = root["pricing"]["improvement"].as_table()) {
            ReadValue(*table, "valueMultiplier", result.improvementPricing.valueMultiplier);
        }

        if (const auto* table = root["pricing"]["crafting"].as_table()) {
            ReadValue(*table, "craftMultiplier", result.craftingPricing.multiplier);
            ReadValue(*table, "smallServiceFee", result.craftingPricing.smallServiceFee);
            ReadValue(*table, "mediumServiceFee", result.craftingPricing.mediumServiceFee);
            ReadValue(*table, "largeServiceFee", result.craftingPricing.largeServiceFee);
            ReadValue(*table, "ammunitionServiceFee", result.craftingPricing.ammunitionServiceFee);
        }

        if (const auto* table = root["pricing"]["improvementFees"].as_table()) {
            ReadValue(*table, "fineSmall", result.improvementPricing.fine.smallItem);
            ReadValue(*table, "fineMedium", result.improvementPricing.fine.mediumItem);
            ReadValue(*table, "fineLarge", result.improvementPricing.fine.largeItem);
            ReadValue(*table, "superiorSmall", result.improvementPricing.superior.smallItem);
            ReadValue(*table, "superiorMedium", result.improvementPricing.superior.mediumItem);
            ReadValue(*table, "superiorLarge", result.improvementPricing.superior.largeItem);
            ReadValue(*table, "exquisiteSmall", result.improvementPricing.exquisite.smallItem);
            ReadValue(*table, "exquisiteMedium", result.improvementPricing.exquisite.mediumItem);
            ReadValue(*table, "exquisiteLarge", result.improvementPricing.exquisite.largeItem);
            ReadValue(*table, "flawlessSmall", result.improvementPricing.flawless.smallItem);
            ReadValue(*table, "flawlessMedium", result.improvementPricing.flawless.mediumItem);
            ReadValue(*table, "flawlessLarge", result.improvementPricing.flawless.largeItem);
            ReadValue(*table, "epicSmall", result.improvementPricing.epic.smallItem);
            ReadValue(*table, "epicMedium", result.improvementPricing.epic.mediumItem);
            ReadValue(*table, "epicLarge", result.improvementPricing.epic.largeItem);
            ReadValue(*table, "legendarySmall", result.improvementPricing.legendary.smallItem);
            ReadValue(*table, "legendaryMedium", result.improvementPricing.legendary.mediumItem);
            ReadValue(*table, "legendaryLarge", result.improvementPricing.legendary.largeItem);
        }

        if (const auto* table = root["features"].as_table()) {
            // 1.7 used one switch for all custom content; it stays as the
            // fallback for older configuration files.
            ReadValue(*table, "enableExclusiveEffects", result.enableSkyforgeEquipment);
            ReadValue(*table, "enableSkyforgeEquipment", result.enableSkyforgeEquipment);
        }

        if (const auto* table = root["pricing"]["barter"].as_table()) {
            ReadValue(*table, "enabled", result.barterPricing.enabled);
            ReadValue(*table, "maxDiscount", result.barterPricing.maxDiscount);
            ReadValue(*table, "includeSpeechSkill", result.barterPricing.includeSpeechSkill);
            ReadValue(*table, "speechBaseline", result.barterPricing.speechBaseline);
            ReadValue(*table, "speechSkillDiscountPerPoint", result.barterPricing.speechSkillDiscountPerPoint);
            ReadValue(*table, "includeSpeechcraftModifiers", result.barterPricing.includeSpeechcraftModifiers);
            ReadValue(*table, "includeHagglingPerks", result.barterPricing.includeHagglingPerks);
        }

        if (const auto* table = root["tempering"]["targets"].as_table()) {
            ReadValue(*table, "fine", result.temperTargets.fine);
            ReadValue(*table, "superior", result.temperTargets.superior);
            ReadValue(*table, "exquisite", result.temperTargets.exquisite);
            ReadValue(*table, "flawless", result.temperTargets.flawless);
            ReadValue(*table, "epic", result.temperTargets.epic);
            ReadValue(*table, "legendary", result.temperTargets.legendary);
        }

        if (const auto* table = root["services"].as_table()) {
            ReadValue(*table, "smithing", result.serviceEnabled);
        }

        if (const auto* table = root["rules"].as_table()) {
            if (const auto tier = table->get_as<std::string>("defaultSmithTier")) {
                if (const auto parsed = ParseTier(tier->get())) {
                    result.rules.defaultSmithTier = *parsed;
                }
            }
            if (const auto tier = table->get_as<std::string>("modAddedSmithTier")) {
                if (const auto parsed = ParseTier(tier->get())) {
                    result.rules.modAddedSmithTier = *parsed;
                }
            }
            ReadValue(
                *table,
                "includeModAddedEquipment",
                result.rules.includeModAddedEquipment);
            ReadValue(*table, "requireBlacksmithFaction", result.rules.requireBlacksmithFaction);
            ReadValue(*table, "requireTemperingRecipe", result.rules.requireTemperingRecipe);
            ReadValue(*table, "allowRecognizedKeywordFallback", result.rules.allowRecognizedKeywordFallback);
            ReadValue(
                *table,
                "allowUnknownMaterialFallbackForExpertOrMaster",
                result.rules.allowUnknownMaterialFallbackForExpertOrMaster);
            ReadValue(*table, "uniqueItemsRequireMaster", result.rules.uniqueItemsRequireMaster);
            ReadValue(*table, "excludeConversionRecipes", result.rules.excludeConversionRecipes);
            ReadValue(*table, "excludeUniqueCrafting", result.rules.excludeUniqueCrafting);
            ReadValue(*table, "artifactsRequireMaster", result.rules.artifactsRequireMaster);
            ReadValue(*table, "allowEnchantedItems", result.rules.allowEnchantedItems);
            ReadValue(*table, "enchantedItemsRequireExpert", result.rules.enchantedItemsRequireExpert);
            ReadValue(*table, "allowEquippedItems", result.rules.allowEquippedItems);
            ReadValue(*table, "showBlockedItemsWithReason", result.rules.showBlockedItemsWithReason);
            ReadValue(*table, "stalhrimOnlyBaldorForCrafting", result.rules.stalhrimOnlyBaldorForCrafting);
            ReadValue(*table, "stalhrimOnlyBaldorForImprovement", result.rules.stalhrimOnlyBaldorForImprovement);
            ReadValue(*table, "eorlundRequiresTrustQuest", result.rules.eorlundRequiresTrustQuest);
            ReadValue(*table, "gunmarRequiresRecruitmentQuest", result.rules.gunmarRequiresRecruitmentQuest);
            ReadValue(*table, "baldorRequiresQuest", result.rules.baldorRequiresQuest);
            ReadValue(*table, "dawnguardGearOnlyGunmar", result.rules.dawnguardGearOnlyGunmar);
            ReadValue(*table, "solstheimGearOnlySolstheimSmiths", result.rules.solstheimGearOnlySolstheimSmiths);
            ReadValue(*table, "dragonGearRequiresWayOfVoice", result.rules.dragonGearRequiresWayOfVoice);
            ReadValue(*table, "dragonGearRequiresMaster", result.rules.dragonGearRequiresMaster);
            ReadValue(*table, "enhancedCrossbowsRequireAncientTechnology", result.rules.enhancedCrossbowsRequireAncientTechnology);
        }

        if (const auto* table = root["ui"].as_table()) {
            ReadValue(*table, "enableHotkey", result.ui.enableHotkey);
            std::int64_t scanCode = result.ui.interactionScanCode;
            std::int64_t pageSize = static_cast<std::int64_t>(result.ui.pageSize);
            ReadValue(*table, "interactionScanCode", scanCode);
            ReadValue(*table, "pageSize", pageSize);
            result.ui.interactionScanCode =
                static_cast<std::uint32_t>(std::clamp<std::int64_t>(scanCode, 0, 255));
            result.ui.pageSize =
                static_cast<std::size_t>(std::max<std::int64_t>(pageSize, 1));
        }

        if (const auto* table = root["crafting"]["allowedCategories"].as_table()) {
            ReadValue(*table, "weapons", result.craftingCategories.weapons);
            ReadValue(*table, "armor", result.craftingCategories.armor);
            ReadValue(*table, "shields", result.craftingCategories.shields);
            ReadValue(*table, "arrows", result.craftingCategories.arrows);
            ReadValue(*table, "bolts", result.craftingCategories.bolts);
        }

        if (const auto* table = root["orders"].as_table()) {
            ReadValue(*table, "enabled", result.orders.enabled);
        }
        if (const auto* table = root["orders"]["smithing"].as_table()) {
            std::int64_t maximumActive =
                static_cast<std::int64_t>(
                    result.orders.maximumActivePerSmith);
            ReadValue(*table, "maximumActivePerSmith", maximumActive);
            result.orders.maximumActivePerSmith =
                static_cast<std::size_t>(
                    std::max<std::int64_t>(maximumActive, 1));

            auto& durations = result.orders.smithing;
            ReadValue(*table, "daggerHours", durations.daggerHours);
            ReadValue(*table, "swordHours", durations.swordHours);
            ReadValue(*table, "warAxeHours", durations.warAxeHours);
            ReadValue(*table, "maceHours", durations.maceHours);
            ReadValue(*table, "bootsHours", durations.bootsHours);
            ReadValue(*table, "gauntletsHours", durations.gauntletsHours);
            ReadValue(*table, "bowHours", durations.bowHours);
            ReadValue(*table, "shieldHours", durations.shieldHours);
            ReadValue(*table, "helmetHours", durations.helmetHours);
            ReadValue(*table, "greatswordHours", durations.greatswordHours);
            ReadValue(*table, "battleaxeHours", durations.battleaxeHours);
            ReadValue(*table, "warhammerHours", durations.warhammerHours);
            ReadValue(*table, "lightBodyArmorHours", durations.lightBodyArmorHours);
            ReadValue(*table, "heavyBodyArmorHours", durations.heavyBodyArmorHours);
            ReadValue(*table, "crossbowHours", durations.crossbowHours);
        }

        if (const auto* table = root["forms"].as_table()) {
            ReadValue(*table, "blacksmithFaction", result.blacksmithFactionForm);
            ReadValue(*table, "eorlund", result.eorlundForm);
            ReadValue(*table, "baldor", result.baldorForm);
            ReadValue(*table, "gunmar", result.gunmarForm);
            ReadValue(*table, "glover", result.gloverForm);
            ReadValue(*table, "eorlundProvingHonorQuestEditorID", result.eorlundProvingHonorQuestEditorID);
            ReadValue(*table, "eorlundMissingInActionQuestEditorID", result.eorlundMissingInActionQuestEditorID);
            ReadValue(*table, "gunmarRecruitmentQuestEditorID", result.gunmarRecruitmentQuestEditorID);
            ReadValue(*table, "baldorQuestEditorID", result.baldorQuestEditorID);
            ReadValue(*table, "wayOfVoiceQuestEditorID", result.wayOfVoiceQuestEditorID);
            ReadValue(*table, "skyforgeUnlockQuestEditorID", result.skyforgeUnlockQuestEditorID);
            ReadValue(*table, "ancientTechnologyQuestEditorID", result.ancientTechnologyQuestEditorID);
            ReadStringArray(*table, "temperingBenchEditorIDs", result.temperingBenchEditorIDs);
            ReadStringArray(*table, "forgeBenchEditorIDs", result.forgeBenchEditorIDs);
        }

        if (const auto* table = root["materials"].as_table()) {
            ReadStringArray(*table, "adeptTokens", result.adeptMaterialTokens);
            ReadStringArray(*table, "expertTokens", result.expertMaterialTokens);
            ReadStringArray(*table, "masterTokens", result.masterMaterialTokens);
            ReadStringArray(*table, "artifactKeywordTokens", result.artifactKeywordTokens);
            ReadStringArray(*table, "dragonGearTokens", result.dragonGearTokens);
            ReadStringArray(*table, "dawnguardGearTokens", result.dawnguardGearTokens);
            ReadStringArray(*table, "enhancedCrossbowTokens", result.enhancedCrossbowTokens);
            ReadStringArray(*table, "solstheimGearTokens", result.solstheimGearTokens);
        }

        if (const auto* table = root["items"].as_table()) {
            ReadStringArray(*table, "allow", result.allowItems);
            ReadStringArray(*table, "block", result.blockItems);
            ReadStringArray(*table, "defaultBlock", result.defaultBlockItems);
            ReadStringArray(*table, "special", result.specialItems);
            if (const auto* tiers = (*table)["tiers"].as_array()) {
                for (const auto& node : *tiers) {
                    const auto* entry = node.as_table();
                    const auto tier = entry ? entry->get_as<std::string>("tier") : nullptr;
                    const auto parsed = tier ? ParseTier(tier->get()) : std::optional<SmithTier>{};
                    if (!parsed) continue;
                    ItemTierOverride override;
                    override.tier = *parsed;
                    if (const auto form = entry->get_as<std::string>("form")) override.form = form->get();
                    if (const auto keyword = entry->get_as<std::string>("keyword")) override.keyword = keyword->get();
                    if (const auto price = entry->get_as<std::int64_t>("price")) {
                        override.price = static_cast<std::int32_t>(std::clamp<std::int64_t>(price->get(), 0, 10'000'000));
                    }
                    if (const auto count = entry->get_as<std::int64_t>("count")) {
                        override.count = static_cast<std::int32_t>(std::clamp<std::int64_t>(count->get(), 0, 1000));
                    }
                    if (!override.form.empty() || !override.keyword.empty()) {
                        result.itemTiers.push_back(std::move(override));
                    }
                }
            }
        }

        if (const auto* table = root["smithing"]["specialCategories"].as_table()) {
            ReadStringArray(*table, "nordHeroWeapons", result.nordHeroWeapons);
            ReadStringArray(*table, "ancientNordArmor", result.ancientNordArmor);
            ReadStringArray(*table, "manualDawnguardItems", result.manualDawnguardItems);
            ReadStringArray(*table, "manualWolfItems", result.manualWolfItems);
            ReadStringArray(*table, "manualAdeptItems", result.manualAdeptItems);
            ReadStringArray(
                *table,
                "ancientNordWeaponKeywords",
                result.ancientNordWeaponKeywords);
        }

        if (const auto* array = root["smithing"]["menuLabelOverrides"].as_array()) {
            result.menuLabelOverrides.clear();
            for (const auto& node : *array) {
                const auto* table = node.as_table();
                if (!table) {
                    continue;
                }
                MenuLabelOverride entry;
                ReadValue(*table, "form", entry.form);
                ReadValue(*table, "label", entry.label);
                ReadValue(*table, "preserveFull", entry.preserveFull);
                if (!entry.form.empty() && !entry.label.empty()) {
                    result.menuLabelOverrides.push_back(std::move(entry));
                }
            }
        }

        if (const auto* array = root["smithing"]["materialLabelOverrides"].as_array()) {
            result.materialLabelOverrides.clear();
            for (const auto& node : *array) {
                const auto* table = node.as_table();
                if (!table) {
                    continue;
                }
                MaterialLabelOverride entry;
                ReadValue(*table, "form", entry.form);
                ReadValue(*table, "label", entry.label);
                if (!entry.form.empty() && !entry.label.empty()) {
                    result.materialLabelOverrides.push_back(std::move(entry));
                }
            }
        }

        if (const auto* array = root["npcs"]["overrides"].as_array()) {
            for (const auto& node : *array) {
                const auto* table = node.as_table();
                if (!table) {
                    continue;
                }

                const auto form = table->get_as<std::string>("form");
                const auto tier = table->get_as<std::string>("tier");
                if (!form || !tier) {
                    continue;
                }
                const auto parsedTier = ParseTier(tier->get());
                if (!parsedTier) {
                    continue;
                }

                NpcOverride override;
                override.form = form->get();
                override.tier = *parsedTier;
                if (const auto name = table->get_as<std::string>("name")) {
                    override.name = name->get();
                }
                if (const auto location =
                        table->get_as<std::string>("courierLocation")) {
                    override.courierLocation = location->get();
                }
                result.npcOverrides.push_back(std::move(override));
            }
        }

        Validate(result);
        return result;
    }
}
