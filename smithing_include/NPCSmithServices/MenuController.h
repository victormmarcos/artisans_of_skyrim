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
#include "NPCSmithServices/InventoryService.h"
#include "NPCSmithServices/RecipeIndex.h"
#include "NPCSmithServices/SmithClassifier.h"
namespace NPCSS {
    enum class MenuItemCategory : std::uint8_t { kWeapons, kArmor, kShields, kAmmo };
    class MenuController {
    public:
        static MenuController& GetSingleton();
        void Configure(const Config& config);
        void OpenForTarget(RE::Actor* actor);
        void OpenCraftingForTarget(RE::Actor* actor);
        void OpenImprovementForTarget(RE::Actor* actor);
        void OpenCollectionForTarget(RE::Actor* actor);
    private:
        bool PrepareTarget(RE::Actor* actor);
        void ShowImprovementQualities(std::string notice = {});
        void ShowBrowser(bool improve, std::string notice = {});
        void ShowOrders(std::string notice = {});
        const Config* config_{ nullptr };
        std::optional<SmithContext> smith_;
        std::optional<TemperQuality> selectedImprovementQuality_;
    };
}
