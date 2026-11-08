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

#include <mutex>
#include <string>
#include <vector>

namespace NPCSS
{
    struct CraftableItem;

    struct ImprovementItem
    {
        RE::TESBoundObject* object{ nullptr };
        RE::ExtraDataList* extraList{ nullptr };
        std::string displayName;
        double currentHealth{ 1.0 };
        double targetHealth{ 1.0 };
        TemperQuality currentQuality{ TemperQuality::kUnimproved };
        TemperQuality targetQuality{ TemperQuality::kUnimproved };
        std::int32_t price{ 0 };
        bool plainStackItem{ false };
    };

    struct TransactionResult
    {
        bool success{ false };
        std::string message;
    };

    class InventoryService
    {
    public:
        static InventoryService& GetSingleton();

        void Configure(const Config& a_config);

        [[nodiscard]] std::vector<ImprovementItem> ListImprovementItems(
            const SmithContext& a_smith,
            TemperQuality a_targetQuality) const;
        [[nodiscard]] TransactionResult ApplyImprovement(
            const SmithContext& a_smith,
            const ImprovementItem& a_item) const;
        [[nodiscard]] TransactionResult ApplyCrafting(
            const SmithContext& a_smith,
            const CraftableItem& a_item) const;

    private:
        [[nodiscard]] std::string BlockReason(
            const SmithContext& a_smith,
            RE::TESBoundObject* a_object,
            RE::ExtraDataList* a_extraList,
            double a_currentHealth,
            bool a_isQuestObject,
            double a_targetHealth) const;
        [[nodiscard]] static bool IsBaseEnchanted(const RE::TESBoundObject* a_object);
        [[nodiscard]] static bool IsInstanceEnchanted(
            RE::ExtraDataList* a_extraList);
        [[nodiscard]] static std::string DisplayName(
            RE::TESBoundObject* a_object,
            RE::ExtraDataList* a_extraList);

        const Config* config_{ nullptr };
        // Last improvement summary written to the log (avoids repeats).
        mutable std::string lastImprovementLog_;
        mutable std::mutex logMutex_;
    };
}
