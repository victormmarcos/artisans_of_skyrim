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

#include "NPCSmithServices/Config.h"
#include "NPCSmithServices/SmithClassifier.h"

#include "RE/Skyrim.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NPCSS
{
    struct CraftableItem
    {
        RE::TESBoundObject* object{ nullptr };
        RE::BGSConstructibleObject* recipe{ nullptr };
        SmithTier requiredTier{ SmithTier::kAdept };
        bool stalhrim{ false };
        std::int32_t outputCount{ 1 };
        std::int32_t price{ 0 };
    };

    class RecipeIndex
    {
    public:
        static RecipeIndex& GetSingleton();

        void Build(const Config& a_config);

        [[nodiscard]] bool HasTemperingRecipe(RE::FormID a_formID) const;
        [[nodiscard]] bool HasRecognizedMaterial(RE::TESBoundObject* a_object) const;
        [[nodiscard]] bool IsAllowedImprovementCategory(RE::TESBoundObject* a_object) const;
        [[nodiscard]] std::optional<SmithTier> MaterialTier(RE::TESBoundObject* a_object) const;
        void LogUnknownMaterials() const;
        void LogNamedItems() const;
        [[nodiscard]] std::vector<CraftableItem> ListCraftable(const SmithContext& a_smith) const;
        // Unique or quest item (see the [rules] comment in the TOML).
        [[nodiscard]] bool IsUnique(const RE::TESBoundObject* a_object) const;

    private:
        struct FixedAmmoBatch
        {
            std::int32_t count{ 1 };
            std::int32_t price{ 0 };
        };

        [[nodiscard]] bool IsAllowedCraftingCategory(RE::TESBoundObject* a_object) const;

        const Config* config_{ nullptr };
        std::unordered_set<RE::FormID> temperableItems_;
        std::unordered_map<RE::FormID, FixedAmmoBatch> fixedAmmoBatches_;
        std::vector<std::pair<RE::BGSConstructibleObject*, RE::TESBoundObject*>> forgeRecipes_;
        std::vector<RE::TESBoundObject*> manualDawnguardItems_;
        std::vector<RE::TESBoundObject*> manualWolfItems_;
        std::vector<RE::TESBoundObject*> manualAdeptItems_;
        std::unordered_set<RE::FormID> commonItems_;  // in a leveled list, an outfit, or forged normally
    };
}
