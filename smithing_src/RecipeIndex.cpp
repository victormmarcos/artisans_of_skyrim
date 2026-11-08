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

#include "NPCSmithServices/RecipeIndex.h"

#include "NPCSmithServices/EquipmentOrigin.h"
#include "NPCSmithServices/FormResolver.h"
#include "NPCSmithServices/OrderService.h"

#include "ArtisansOfSkyrim/ServiceDiscount.h"

#include <algorithm>
#include <array>
#include <utility>

namespace
{
    struct FixedAmmoSpec
    {
        std::string_view form;
        std::int32_t count;
        std::int32_t price;
    };

    constexpr std::array<FixedAmmoSpec, 19> FIXED_AMMO_SPECS{ {
        { "Skyrim.esm|0001397D", 24, 100 },
        { "Skyrim.esm|0001397F", 24, 120 },
        { "Skyrim.esm|000139BB", 24, 140 },
        { "Skyrim.esm|000139BC", 24, 160 },
        { "Dragonborn.esm|0002623B", 24, 180 },
        { "Skyrim.esm|000139BD", 24, 200 },
        { "Skyrim.esm|000139BE", 24, 220 },
        { "Skyrim.esm|000139BF", 24, 240 },
        { "Dragonborn.esm|00026239", 24, 260 },
        { "Skyrim.esm|000139C0", 24, 280 },
        { "Dawnguard.esm|000176F4", 24, 300 },
        { "Dawnguard.esm|00000BB3", 10, 100 },
        { "Dawnguard.esm|0000D099", 10, 200 },
        { "Dawnguard.esm|0000F1A0", 10, 400 },
        { "Dawnguard.esm|0000F1BB", 10, 400 },
        { "Dawnguard.esm|0000F1BC", 10, 400 },
        { "Dawnguard.esm|0000F1B1", 10, 600 },
        { "Dawnguard.esm|0000F1B7", 10, 600 },
        { "Dawnguard.esm|0000F1B9", 10, 600 },
    } };

    [[nodiscard]] bool ContainsEditorID(
        const RE::BGSKeyword* a_keyword,
        std::span<const std::string> a_allowed)
    {
        if (!a_keyword) {
            return false;
        }
        const auto editorID = NPCSS::FormResolver::Lower(a_keyword->GetFormEditorID());
        return std::ranges::any_of(a_allowed, [&](const std::string& a_value) {
            return editorID == NPCSS::FormResolver::Lower(a_value);
        });
    }

    [[nodiscard]] int TierRank(NPCSS::SmithTier a_tier)
    {
        return static_cast<int>(a_tier);
    }

    [[nodiscard]] bool IsBaseEnchanted(const RE::TESBoundObject* a_object)
    {
        const auto enchantable = a_object ? a_object->As<RE::TESEnchantableForm>() : nullptr;
        return enchantable && enchantable->formEnchanting;
    }

    [[nodiscard]] bool IsCompleted(RE::TESQuest* a_quest)
    {
        return a_quest && a_quest->IsCompleted();
    }

    [[nodiscard]] RE::TESBoundObject* EorlundFinalOutput(
        RE::TESBoundObject* a_source,
        bool a_isEorlund,
        bool a_skyforgeEquipmentEnabled)
    {
        if (!a_source || !a_isEorlund || !a_skyforgeEquipmentEnabled) {
            return a_source;
        }

        struct Substitution
        {
            RE::FormID steel;
            std::string_view plugin;
            RE::FormID replacement;
            std::string_view replacementEditorID;
        };
        static constexpr std::array<Substitution, 12> substitutions{ {
            { 0x00013986, "ArtisansOfSkyrim.esp", 0x000F31, "AOS_WeapSkyforgeDagger" },
            { 0x00013989, "ArtisansOfSkyrim.esp", 0x000F32, "AOS_WeapSkyforgeSword" },
            { 0x00013983, "ArtisansOfSkyrim.esp", 0x000F33, "AOS_WeapSkyforgeWarAxe" },
            { 0x00013987, "ArtisansOfSkyrim.esp", 0x000F34, "AOS_WeapSkyforgeGreatsword" },
            { 0x00013984, "ArtisansOfSkyrim.esp", 0x000F35, "AOS_WeapSkyforgeBattleaxe" },
            { 0x00013988, "ArtisansOfSkyrim.esp", 0x000F1B, "AOS_WeapSkyforgeMace" },
            { 0x0001398A, "ArtisansOfSkyrim.esp", 0x000F1C, "AOS_WeapSkyforgeWarhammer" },
            { 0x0001395B, "ArtisansOfSkyrim.esp", 0x000F1D, "AOS_ArmorSkyforgePlateBoots" },
            { 0x0001395C, "ArtisansOfSkyrim.esp", 0x000F1E, "AOS_ArmorSkyforgePlateCuirass" },
            { 0x0001395D, "ArtisansOfSkyrim.esp", 0x000F1F, "AOS_ArmorSkyforgePlateGauntlets" },
            { 0x0001395E, "ArtisansOfSkyrim.esp", 0x000F20, "AOS_ArmorSkyforgePlateHelmet" },
            { 0x00013955, "ArtisansOfSkyrim.esp", 0x000F29, "AOS_ArmorSkyforgeSteelShield" },
        } };
        const auto sourceFile = a_source->GetFile(0);
        if (!sourceFile ||
            !std::string_view{ sourceFile->GetFilename() }.ends_with("Skyrim.esm")) {
            return a_source;
        }
        const auto match = std::ranges::find(
            substitutions,
            a_source->GetLocalFormID(),
            &Substitution::steel);
        if (match == substitutions.end()) {
            return a_source;
        }
        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        auto replacement = dataHandler ?
            dataHandler->LookupForm<RE::TESBoundObject>(
                match->replacement,
                match->plugin) :
            nullptr;
        if (!replacement) {
            replacement = RE::TESForm::LookupByEditorID<RE::TESBoundObject>(
                match->replacementEditorID);
        }
        if (!replacement || !replacement->GetPlayable() ||
            replacement->GetGoldValue() <= 0) {
            SKSE::log::error(
                "Eorlund substitution {:08X} -> {}|{:06X} is unusable; "
                "falling back to the original Steel item.",
                match->steel,
                match->plugin,
                match->replacement);
            return a_source;
        }
        SKSE::log::info(
            "Eorlund Skyforge substitution {}|{:06X} -> {:08X} '{}'.",
            match->plugin,
            match->replacement,
            replacement->GetFormID(),
            replacement->GetName());
        return replacement;
    }

    [[nodiscard]] bool IsDragonGear(const NPCSS::Config& a_config, const RE::TESBoundObject* a_object)
    {
        return NPCSS::FormResolver::HasTextToken(a_object, a_config.dragonGearTokens);
    }

    [[nodiscard]] bool IsDawnguardGear(const NPCSS::Config& a_config, const RE::TESBoundObject* a_object)
    {
        return NPCSS::FormResolver::HasTextToken(a_object, a_config.dawnguardGearTokens);
    }

    [[nodiscard]] bool IsEnhancedCrossbowGear(const NPCSS::Config& a_config, const RE::TESBoundObject* a_object)
    {
        return NPCSS::FormResolver::HasTextToken(a_object, a_config.enhancedCrossbowTokens);
    }

    [[nodiscard]] bool IsSolstheimGear(const NPCSS::Config& a_config, const RE::TESBoundObject* a_object)
    {
        return NPCSS::FormResolver::HasTextToken(a_object, a_config.solstheimGearTokens);
    }

    [[nodiscard]] bool PassesProgressionRestrictions(
        const NPCSS::Config& a_config,
        const NPCSS::SmithContext& a_smith,
        const RE::TESBoundObject* a_object)
    {
        const auto& resolver = NPCSS::FormResolver::GetSingleton();

        if (resolver.IsAncientNordWeapon(a_object) &&
            !resolver.IsNordHero(a_object->GetFormID()) &&
            !resolver.IsAllowed(a_object->GetFormID())) {
            return false;
        }

        const bool skyforgeSpecial =
            resolver.IsNordHero(a_object->GetFormID()) ||
            resolver.IsAncientNordArmor(a_object->GetFormID());
        if (skyforgeSpecial &&
            (a_smith.tier != NPCSS::SmithTier::kMaster ||
             !a_smith.isEorlund ||
             !IsCompleted(resolver.SkyforgeUnlockQuest()))) {
            return false;
        }

        if (a_config.rules.dragonGearRequiresMaster &&
            IsDragonGear(a_config, a_object) &&
            a_smith.tier != NPCSS::SmithTier::kMaster) {
            return false;
        }
        if (a_config.rules.dragonGearRequiresWayOfVoice &&
            IsDragonGear(a_config, a_object) &&
            !IsCompleted(resolver.WayOfVoiceQuest())) {
            return false;
        }

        if (a_config.rules.dawnguardGearOnlyGunmar &&
            IsDawnguardGear(a_config, a_object) &&
            !a_smith.isGunmar) {
            return false;
        }

        if (a_config.rules.enhancedCrossbowsRequireAncientTechnology &&
            IsEnhancedCrossbowGear(a_config, a_object) &&
            !IsCompleted(resolver.AncientTechnologyQuest())) {
            return false;
        }

        if (a_config.rules.solstheimGearOnlySolstheimSmiths &&
            IsSolstheimGear(a_config, a_object) &&
            !a_smith.isSolstheimSmith) {
            return false;
        }

        return true;
    }
    // Official game files and Artisans itself; everything else is mod-added.
    [[nodiscard]] bool IsOfficialRecipe(const RE::TESForm* a_form)
    {
        const auto file = a_form ? a_form->GetFile(0) : nullptr;
        const auto name = file ? file->GetFilename() : std::string_view{};
        return name == "ArtisansOfSkyrim.esp" ||
            (!name.empty() && NPCSS::IsOfficialEquipmentPlugin(name));
    }

    // A recipe that consumes a weapon or armor turns one item into another.
    [[nodiscard]] bool IsConversionRecipe(const RE::BGSConstructibleObject* a_recipe)
    {
        const auto& items = a_recipe->requiredItems;
        for (std::uint32_t i = 0; i < items.numContainerObjects; ++i) {
            const auto entry = items.containerObjects[i];
            if (entry && entry->obj &&
                (entry->obj->Is(RE::FormType::Weapon) || entry->obj->Is(RE::FormType::Armor))) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::int32_t DiscountedServicePrice(
        std::int32_t a_price,
        const NPCSS::Config& a_config)
    {
        return NPCSS::ApplyPriceDiscount(
            a_price,
            AOS::PlayerServiceDiscount(a_config.barterPricing));
    }

}

namespace NPCSS
{
    RecipeIndex& RecipeIndex::GetSingleton()
    {
        static RecipeIndex singleton;
        return singleton;
    }

    bool RecipeIndex::IsUnique(const RE::TESBoundObject* a_object) const
    {
        if (!config_ || !a_object ||
            (!a_object->Is(RE::FormType::Weapon) && !a_object->Is(RE::FormType::Armor))) {
            return false;
        }
        const auto& resolver = FormResolver::GetSingleton();
        if (resolver.IsAllowed(a_object->GetFormID())) {
            return false;
        }
        if (resolver.IsSpecial(a_object->GetFormID()) ||
            FormResolver::HasKeywordToken(a_object, config_->artifactKeywordTokens) ||
            FormResolver::HasExactKeyword(a_object, std::array<std::string, 1>{ "MagicDisallowEnchanting" })) {
            return true;
        }
        // Manual service lists are curated equipment, never quest items.
        const auto listed = [a_object](const std::vector<RE::TESBoundObject*>& a_list) {
            return std::ranges::find(a_list, a_object) != a_list.end();
        };
        if (listed(manualDawnguardItems_) || listed(manualWolfItems_) || listed(manualAdeptItems_)) {
            return false;
        }
        return !commonItems_.empty() && !commonItems_.contains(a_object->GetFormID());
    }

    void RecipeIndex::Build(const Config& a_config)
    {
        config_ = std::addressof(a_config);
        temperableItems_.clear();
        fixedAmmoBatches_.clear();
        forgeRecipes_.clear();
        manualDawnguardItems_.clear();
        manualWolfItems_.clear();
        manualAdeptItems_.clear();
        commonItems_.clear();

        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            SKSE::log::error("TESDataHandler is unavailable; recipe index was not built");
            return;
        }

        const auto& resolver = FormResolver::GetSingleton();
        for (const auto& spec : FIXED_AMMO_SPECS) {
            if (const auto form = resolver.Resolve(spec.form)) {
                fixedAmmoBatches_.insert_or_assign(
                    form->GetFormID(),
                    FixedAmmoBatch{ spec.count, spec.price });
            } else {
                SKSE::log::error("Fixed ammunition form '{}' could not be resolved", spec.form);
            }
        }
        if (fixedAmmoBatches_.size() != FIXED_AMMO_SPECS.size()) {
            SKSE::log::error(
                "Fixed ammunition table resolved {}/{} records",
                fixedAmmoBatches_.size(),
                FIXED_AMMO_SPECS.size());
        }

        for (const auto& spec : a_config.manualDawnguardItems) {
            const auto form = resolver.Resolve(spec);
            const auto object = form ? form->As<RE::TESBoundObject>() : nullptr;
            if (object) {
                manualDawnguardItems_.push_back(object);
            } else {
                SKSE::log::error(
                    "Manual Dawnguard crafting form '{}' could not be resolved",
                    spec);
            }
        }

        for (const auto& spec : a_config.manualWolfItems) {
            const auto form = resolver.Resolve(spec);
            const auto object = form ? form->As<RE::TESBoundObject>() : nullptr;
            if (object) {
                manualWolfItems_.push_back(object);
            } else {
                SKSE::log::error(
                    "Manual Wolf crafting form '{}' could not be resolved",
                    spec);
            }
        }

        for (const auto& spec : a_config.manualAdeptItems) {
            const auto form = resolver.Resolve(spec);
            const auto object = form ? form->As<RE::TESBoundObject>() : nullptr;
            if (object) {
                manualAdeptItems_.push_back(object);
            } else {
                SKSE::log::error(
                    "Manual Adept crafting form '{}' could not be resolved",
                    spec);
            }
        }

        // Items the game hands out normally: leveled lists and NPC outfits.
        for (const auto list : dataHandler->GetFormArray<RE::TESLevItem>()) {
            if (!list) continue;
            for (const auto& entry : list->entries) {
                if (entry.form) commonItems_.insert(entry.form->GetFormID());
            }
        }
        for (const auto outfit : dataHandler->GetFormArray<RE::BGSOutfit>()) {
            if (!outfit) continue;
            for (const auto item : outfit->outfitItems) {
                if (item) commonItems_.insert(item->GetFormID());
            }
        }

        std::size_t skippedConversions = 0;
        std::vector<std::pair<RE::BGSConstructibleObject*, RE::TESBoundObject*>> modRecipes;
        for (const auto recipe : dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
            if (!recipe || !recipe->createdItem || !recipe->benchKeyword) {
                continue;
            }

            auto object = recipe->createdItem->As<RE::TESBoundObject>();
            if (!object) {
                continue;
            }
            if (!IsAllowedEquipmentOrigin(
                    object,
                    a_config.rules.includeModAddedEquipment)) {
                continue;
            }

            if (ContainsEditorID(recipe->benchKeyword, a_config.temperingBenchEditorIDs) &&
                (object->Is(RE::FormType::Weapon) || object->Is(RE::FormType::Armor))) {
                temperableItems_.insert(object->GetFormID());
            }

            if (ContainsEditorID(recipe->benchKeyword, a_config.forgeBenchEditorIDs) &&
                IsAllowedCraftingCategory(object)) {
                const bool officialRecipe = IsOfficialRecipe(recipe);
                const bool conversion = IsConversionRecipe(recipe);
                // A regular forge recipe makes an item "common". For an official
                // item only official recipes count, so a mod that makes a vanilla
                // unique craftable does not hide that it is unique.
                if (!conversion && (officialRecipe || !IsOfficialRecipe(object))) {
                    commonItems_.insert(object->GetFormID());
                }
                if (officialRecipe) {
                    forgeRecipes_.emplace_back(recipe, object);
                } else if (conversion && a_config.rules.excludeConversionRecipes &&
                           !resolver.IsAllowed(object->GetFormID())) {
                    ++skippedConversions;
                    SKSE::log::debug("Skipped conversion recipe {:08X} for {:08X}",
                        recipe->GetFormID(), object->GetFormID());
                } else {
                    modRecipes.emplace_back(recipe, object);
                }
            }
        }

        // Mod recipes are checked once every regular recipe is known.
        std::size_t skippedUniques = 0;
        for (const auto& [recipe, object] : modRecipes) {
            if (a_config.rules.excludeUniqueCrafting && IsUnique(object)) {
                ++skippedUniques;
                SKSE::log::debug("Skipped mod recipe {:08X} for unique item {:08X}",
                    recipe->GetFormID(), object->GetFormID());
                continue;
            }
            forgeRecipes_.emplace_back(recipe, object);
        }
        LogUnknownMaterials();
        LogNamedItems();
        if (skippedConversions > 0 || skippedUniques > 0) {
            SKSE::log::info(
                "Crafting: skipped {} mod conversion recipe(s) and {} mod recipe(s) for unique items.",
                skippedConversions,
                skippedUniques);
        }

        SKSE::log::info(
            "Recipe index built: {} temperable forms, {} filtered forge recipes, {} manual Dawnguard items, {} manual Wolf items, {} manual Adept items",
            temperableItems_.size(),
            forgeRecipes_.size(),
            manualDawnguardItems_.size(),
            manualWolfItems_.size(),
            manualAdeptItems_.size());
    }

    bool RecipeIndex::HasTemperingRecipe(RE::FormID a_formID) const
    {
        return temperableItems_.contains(a_formID);
    }

    // Forge items left out because their material is unknown, written as lines
    // ready to paste into a patch file ([[items.tiers]]).
    void RecipeIndex::LogUnknownMaterials() const
    {
        const auto& resolver = FormResolver::GetSingleton();
        std::unordered_set<RE::FormID> seen;
        std::size_t count = 0;
        for (const auto& [recipe, object] : forgeRecipes_) {
            if (!object || !seen.insert(object->GetFormID()).second) continue;
            if (!object->GetPlayable() || object->GetGoldValue() <= 0 ||
                (IsBaseEnchanted(object) && !resolver.IsAllowed(object->GetFormID())) ||
                resolver.IsBlocked(object->GetFormID()) || MaterialTier(object)) {
                continue;
            }
            const auto file = object->GetFile(0);
            std::string materials;
            for (const auto& keyword : FormResolver::KeywordEditorIDs(object)) {
                if (FormResolver::Lower(keyword).find("material") == std::string::npos) continue;
                if (!materials.empty()) materials += ", ";
                materials += keyword;
            }
            SKSE::log::info(
                "Unknown material, not crafted: '{}' -> [[items.tiers]] form = \"{}|{:06X}\" tier = \"Expert\"{}",
                object->GetName(),
                file ? file->GetFilename() : std::string_view{ "?" },
                object->GetLocalFormID(),
                materials.empty() ? std::string{} : " (or keyword = \"" + materials.substr(0, materials.find(',')) + "\"; material keywords: " + materials + ")");
            ++count;
        }
        if (count > 0) {
            SKSE::log::info(
                "Crafting: {} forge item(s) have an unknown material. Add [[items.tiers]] entries "
                "to a patch file in Data/SKSE/Plugins/ArtisansOfSkyrim/ to offer them.",
                count);
        }
    }

    // One line per named item that cannot be offered, with what is missing.
    void RecipeIndex::LogNamedItems() const
    {
        const auto& resolver = FormResolver::GetSingleton();
        std::unordered_set<RE::FormID> forged;
        for (const auto& [recipe, object] : forgeRecipes_) {
            if (object) forged.insert(object->GetFormID());
        }
        for (const auto formID : resolver.NamedItems()) {
            auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(formID);
            if (!object) continue;
            const auto name = object->GetName();
            const auto price = resolver.ItemPriceFor(formID);
            std::string problem;
            if (!object->GetPlayable()) {
                problem = "it is not playable";
            } else if (!IsAllowedCraftingCategory(object)) {
                problem = "blacksmiths only make weapons, armor, shields, arrows and bolts (no jewelry, circlets or staves)";
            } else if (!forged.contains(formID) && !price) {
                problem = "it has no forge recipe; add price (and count for arrows/bolts) to offer it";
            } else if (object->GetGoldValue() <= 0 && !price) {
                problem = "it is worth 0 gold; add price to offer it";
            } else if (!forged.contains(formID) && object->Is(RE::FormType::Ammo) && price->count <= 0) {
                problem = "arrows and bolts without a forge recipe need count (the batch size)";
            }
            if (!problem.empty()) {
                SKSE::log::warn("Patch item {:08X} '{}' is not offered for crafting: {}.", formID, name, problem);
            }
        }
    }

    bool RecipeIndex::HasRecognizedMaterial(RE::TESBoundObject* a_object) const
    {
        return MaterialTier(a_object).has_value();
    }

    std::optional<SmithTier> RecipeIndex::MaterialTier(RE::TESBoundObject* a_object) const
    {
        if (!config_ || !a_object) {
            return std::nullopt;
        }

        const auto& resolver = FormResolver::GetSingleton();
        if (const auto configured = resolver.ItemTierFor(a_object)) {
            return configured;
        }
        if (resolver.IsNordHero(a_object->GetFormID()) ||
            resolver.IsAncientNordArmor(a_object->GetFormID()) ||
            IsDawnguardGear(*config_, a_object)) {
            return SmithTier::kMaster;
        }

        static const std::array<std::string, 5> masterKeywords{
            "ArmorMaterialDaedric",
            "WeapMaterialDaedric",
            "ArmorMaterialDragonplate",
            "ArmorMaterialDragonscale",
            "WeapMaterialDragonbone"
        };
        static const std::array<std::string, 8> expertKeywords{
            "ArmorMaterialSteelPlate",
            "ArmorMaterialOrcish",
            "WeapMaterialOrcish",
            "ArmorMaterialDwarven",
            "WeapMaterialDwarven",
            "ArmorMaterialElven",
            "WeapMaterialElven",
            "ArmorMaterialGlass"
        };
        static const std::array<std::string, 5> adeptKeywords{
            "ArmorMaterialIron",
            "WeapMaterialIron",
            "ArmorMaterialSteel",
            "WeapMaterialSteel",
            "ArmorMaterialLeather"
        };
        if (FormResolver::HasExactKeyword(a_object, masterKeywords)) {
            return SmithTier::kMaster;
        }
        if (FormResolver::HasExactKeyword(a_object, expertKeywords)) {
            return SmithTier::kExpert;
        }
        if (FormResolver::HasExactKeyword(a_object, adeptKeywords)) {
            return SmithTier::kAdept;
        }

        static const std::array<std::string, 3> steelNordicTokens{
            "steel nordic", "steelnordic", "nordic steel"
        };
        static const std::array<std::string, 2> steelPlateTokens{
            "steel plate", "steelplate"
        };

        if (FormResolver::HasTextToken(a_object, steelPlateTokens)) {
            return SmithTier::kExpert;
        }
        if (FormResolver::HasTextToken(a_object, steelNordicTokens)) {
            return SmithTier::kAdept;
        }
        if (FormResolver::HasTextToken(a_object, config_->masterMaterialTokens)) {
            return SmithTier::kMaster;
        }
        if (FormResolver::HasTextToken(a_object, config_->expertMaterialTokens)) {
            return SmithTier::kExpert;
        }
        if (FormResolver::HasTextToken(a_object, config_->adeptMaterialTokens)) {
            return SmithTier::kAdept;
        }
        return std::nullopt;
    }

    std::vector<CraftableItem> RecipeIndex::ListCraftable(const SmithContext& a_smith) const
    {
        std::vector<CraftableItem> result;
        std::unordered_set<RE::FormID> addedItems;
        if (!config_ || !a_smith.servicesUnlocked) {
            return result;
        }

        const auto& resolver = FormResolver::GetSingleton();
        for (const auto& [recipe, sourceObject] : forgeRecipes_) {
            auto* object = EorlundFinalOutput(
                sourceObject,
                a_smith.isEorlund,
                config_->enableSkyforgeEquipment);
            if (!recipe || !object || !object->GetPlayable()) {
                continue;
            }
            if (!IsAllowedEquipmentOrigin(
                    sourceObject,
                    config_->rules.includeModAddedEquipment)) {
                continue;
            }
            if (config_->orders.enabled &&
                !ClassifyCraftingOrderCategory(object)) {
                continue;
            }
            if (object->GetGoldValue() <= 0 && !resolver.ItemPriceFor(object->GetFormID())) {
                continue;  // Avoid free service exploits and non-market/hidden equipment.
            }
            // Items enchanted by default are offered only when a patch names them.
            if (resolver.IsBlocked(object->GetFormID()) ||
                (IsBaseEnchanted(object) && !resolver.IsAllowed(object->GetFormID()))) {
                continue;
            }

            auto requiredTier = MaterialTier(object);
            if (!requiredTier) {
                continue;  // Unknown mod material is never auto-crafted.
            }
            if (TierRank(*requiredTier) > TierRank(a_smith.tier)) {
                continue;
            }

            const bool stalhrim = FormResolver::IsStalhrim(object);
            if (stalhrim &&
                config_->rules.stalhrimOnlyBaldorForCrafting &&
                !a_smith.isBaldor) {
                continue;
            }
            if (!PassesProgressionRestrictions(*config_, a_smith, object)) {
                continue;
            }
            if (!addedItems.insert(object->GetFormID()).second) {
                continue;
            }

            auto outputCount = std::max<std::int32_t>(
                static_cast<std::int32_t>(recipe->data.numConstructed),
                1);
            std::int32_t price = 0;
            if (const auto patchPrice = resolver.ItemPriceFor(object->GetFormID())) {
                // Price from a patch; never below the resale value of the batch.
                if (patchPrice->count > 0) outputCount = patchPrice->count;
                price = FixedBatchPrice(
                    patchPrice->price,
                    object->GetGoldValue(),
                    outputCount,
                    AOS::PlayerServiceDiscount(config_->barterPricing));
            } else if (const auto fixed = fixedAmmoBatches_.find(object->GetFormID());
                fixed != fixedAmmoBatches_.end()) {
                outputCount = fixed->second.count;
                price = FixedBatchPrice(
                    fixed->second.price,
                    object->GetGoldValue(),
                    outputCount,
                    AOS::PlayerServiceDiscount(config_->barterPricing));
            } else {
                price = DiscountedServicePrice(
                    CraftingPrice(
                        object->GetGoldValue(),
                        ClassifyServiceComplexity(object),
                        config_->craftingPricing,
                        outputCount),
                    *config_);
            }

            result.push_back(CraftableItem{
                object,
                recipe,
                *requiredTier,
                stalhrim,
                outputCount,
                price
            });
        }

        if (a_smith.isEorlund &&
            IsCompleted(resolver.EorlundProvingHonorQuest())) {
            for (const auto object : manualWolfItems_) {
                if (!object || !object->GetPlayable() ||
                    object->GetGoldValue() <= 0 ||
                    !IsAllowedEquipmentOrigin(
                        object,
                        config_->rules.includeModAddedEquipment)) {
                    continue;
                }
                if (config_->orders.enabled &&
                    !ClassifyCraftingOrderCategory(object)) {
                    continue;
                }
                if (!IsAllowedCraftingCategory(object) ||
                    resolver.IsBlocked(object->GetFormID()) ||
                    !PassesProgressionRestrictions(*config_, a_smith, object) ||
                    !addedItems.insert(object->GetFormID()).second) {
                    continue;
                }
                const auto price = DiscountedServicePrice(
                    CraftingPrice(
                        object->GetGoldValue(),
                        ClassifyServiceComplexity(object),
                        config_->craftingPricing),
                    *config_);
                result.push_back(CraftableItem{
                    object,
                    nullptr,
                    SmithTier::kAdept,
                    false,
                    1,
                    price
                });
            }
        }


        for (const auto object : manualAdeptItems_) {
            if (!object || !object->GetPlayable() || object->GetGoldValue() <= 0 ||
                TierRank(a_smith.tier) < TierRank(SmithTier::kAdept)) {
                continue;
            }
            if (config_->orders.enabled &&
                !ClassifyCraftingOrderCategory(object)) {
                continue;
            }
            if (!IsAllowedCraftingCategory(object) ||
                resolver.IsBlocked(object->GetFormID()) ||
                !PassesProgressionRestrictions(*config_, a_smith, object) ||
                !addedItems.insert(object->GetFormID()).second) {
                continue;
            }
            const auto price = DiscountedServicePrice(
                CraftingPrice(
                    object->GetGoldValue(),
                    ClassifyServiceComplexity(object),
                    config_->craftingPricing),
                *config_);
            result.push_back(CraftableItem{
                object,
                nullptr,
                SmithTier::kAdept,
                false,
                1,
                price
            });
        }

        if (a_smith.isGunmar && a_smith.tier == SmithTier::kMaster) {
            for (const auto object : manualDawnguardItems_) {
                if (!object || !object->GetPlayable() || object->GetGoldValue() <= 0) {
                    continue;
                }
                if (!IsAllowedEquipmentOrigin(
                        object,
                        config_->rules.includeModAddedEquipment)) {
                    continue;
                }
                if (config_->orders.enabled &&
                    !ClassifyCraftingOrderCategory(object)) {
                    continue;
                }
                if (!IsAllowedCraftingCategory(object) ||
                    resolver.IsBlocked(object->GetFormID()) ||
                    !PassesProgressionRestrictions(*config_, a_smith, object)) {
                    continue;
                }
                if (!addedItems.insert(object->GetFormID()).second) {
                    continue;
                }

                const auto price = DiscountedServicePrice(
                    CraftingPrice(
                        object->GetGoldValue(),
                        ClassifyServiceComplexity(object),
                        config_->craftingPricing),
                    *config_);
                result.push_back(CraftableItem{
                    object,
                    nullptr,
                    SmithTier::kMaster,
                    false,
                    1,
                    price
                });
            }
        }

        // Items a patch names with a price but without a forge recipe: the
        // blacksmith makes them directly, at the patch price.
        for (const auto formID : resolver.NamedItems()) {
            const auto patchPrice = resolver.ItemPriceFor(formID);
            auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(formID);
            if (!patchPrice || !object || addedItems.contains(formID) || !object->GetPlayable()) {
                continue;
            }
            if (!IsAllowedCraftingCategory(object) ||
                resolver.IsBlocked(formID) ||
                !IsAllowedEquipmentOrigin(object, config_->rules.includeModAddedEquipment)) {
                continue;
            }
            if (config_->orders.enabled && !ClassifyCraftingOrderCategory(object)) {
                continue;
            }
            if (object->Is(RE::FormType::Ammo) && patchPrice->count <= 0) {
                continue;  // Arrows and bolts need a batch size.
            }
            const auto requiredTier = MaterialTier(object);
            if (!requiredTier || TierRank(*requiredTier) > TierRank(a_smith.tier)) {
                continue;
            }
            const bool stalhrim = FormResolver::IsStalhrim(object);
            if (stalhrim && config_->rules.stalhrimOnlyBaldorForCrafting && !a_smith.isBaldor) {
                continue;
            }
            if (!PassesProgressionRestrictions(*config_, a_smith, object)) {
                continue;
            }
            addedItems.insert(formID);
            const auto count = std::max<std::int32_t>(patchPrice->count, 1);
            result.push_back(CraftableItem{
                object,
                nullptr,
                *requiredTier,
                stalhrim,
                count,
                FixedBatchPrice(
                    patchPrice->price,
                    object->GetGoldValue(),
                    count,
                    AOS::PlayerServiceDiscount(config_->barterPricing))
            });
        }

        std::ranges::sort(result, {}, [](const CraftableItem& a_item) {
            return FormResolver::GetSingleton().MenuLabelFor(a_item.object);
        });
        return result;
    }

    bool RecipeIndex::IsAllowedImprovementCategory(RE::TESBoundObject* a_object) const
    {
        if (!a_object || !a_object->GetPlayable()) {
            return false;
        }

        if (const auto weapon = a_object->As<RE::TESObjectWEAP>()) {
            return weapon->GetWeaponType() != RE::WEAPON_TYPE::kStaff &&
                weapon->GetWeaponType() != RE::WEAPON_TYPE::kHandToHandMelee;
        }

        if (const auto armor = a_object->As<RE::TESObjectARMO>()) {
            const bool jewelry = FormResolver::HasKeywordToken(
                armor,
                std::array<std::string, 1>{ "jewelry" });
            const bool circlet = FormResolver::HasTextToken(
                armor,
                std::array<std::string, 1>{ "circlet" });
            if (jewelry || circlet) {
                return false;
            }
            return true;
        }

        return false;
    }

    bool RecipeIndex::IsAllowedCraftingCategory(RE::TESBoundObject* a_object) const
    {
        if (!config_ || !a_object || !a_object->GetPlayable()) {
            return false;
        }

        if (a_object->Is(RE::FormType::Weapon)) {
            return config_->craftingCategories.weapons &&
                IsAllowedImprovementCategory(a_object);
        }

        if (const auto armor = a_object->As<RE::TESObjectARMO>()) {
            if (!IsAllowedImprovementCategory(a_object)) {
                return false;
            }
            const bool shield =
                (armor->formFlags & RE::TESObjectARMO::RecordFlags::kShield) != 0;
            return shield ?
                config_->craftingCategories.shields :
                config_->craftingCategories.armor;
        }

        if (const auto ammo = a_object->As<RE::TESAmmo>()) {
            return ammo->IsBolt() ?
                config_->craftingCategories.bolts :
                config_->craftingCategories.arrows;
        }

        return false;
    }
}
