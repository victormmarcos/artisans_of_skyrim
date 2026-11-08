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

#include "RE/Skyrim.h"

#include <optional>
#include <string>

namespace NPCES
{
    struct EnchanterContext
    {
        RE::ActorHandle actor;
        RE::FormID baseFormID{ 0 };
        ServiceTier tier{ ServiceTier::kAdept };
        bool servicesUnlocked{ true };
        std::string lockedReason;
        std::string displayName;
    };

    class EnchanterClassifier
    {
    public:
        static EnchanterClassifier& GetSingleton();

        void Configure(const Config& a_config);
        [[nodiscard]] std::optional<EnchanterContext> Classify(
            RE::Actor* a_actor) const;

    private:
        const Config* config_{ nullptr };
    };
}
