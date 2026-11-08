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
#include "NPCEnchantingServices/Config.h"
#include "NPCEnchantingServices/InventoryService.h"
#include "NPCEnchantingServices/EnchanterClassifier.h"
namespace NPCES {
    class MenuController {
    public:
        static MenuController& GetSingleton();
        void Configure(const Config& config);
        void OpenForTarget(RE::Actor* actor);
    private:
        void ShowItems(std::string notice = {});
        void ShowEffects(std::string notice = {});
        const Config* config_{ nullptr };
        std::optional<EnchanterContext> enchanter_;
        std::optional<EnchantableItem> selected_;
        const ResolvedEnchantment* first_{ nullptr };
        const ResolvedEnchantment* second_{ nullptr };
    };
}
