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

#include "NPCAlchemyServices/Config.h"

#include "RE/Skyrim.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace NPCAS
{
    struct ResolvedNpcService
    {
        ServiceTier tier{ ServiceTier::kAdept };
        RE::TESQuest* requiredQuest{ nullptr };
        bool questRequired{ false };
        std::string lockedReason;
    };

    class FormResolver
    {
    public:
        static FormResolver& GetSingleton();

        void Build(const Config& a_config);

        [[nodiscard]] RE::TESForm* Resolve(std::string_view a_spec) const;
        [[nodiscard]] RE::TESFaction* AlchemyFaction() const noexcept;
        [[nodiscard]] const ResolvedNpcService* ServiceForNPC(
            RE::FormID a_formID,
            std::string_view a_displayName) const;

        [[nodiscard]] static std::string Lower(std::string_view a_value);

    private:
        bool allowDisplayNameFallback_{ false };
        RE::TESFaction* alchemyFaction_{ nullptr };
        std::unordered_map<RE::FormID, ResolvedNpcService> npcServicesByFormID_;
        std::unordered_map<std::string, ResolvedNpcService> npcServicesByName_;
    };
}
