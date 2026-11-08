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

#include <vector>

namespace NPCAS
{
    struct ResolvedAlchemyItem
    {
        const AlchemyDefinition* definition{ nullptr };
        RE::TESBoundObject* item{ nullptr };
    };

    class AlchemyCatalog
    {
    public:
        static AlchemyCatalog& GetSingleton();

        void Build(const Config& a_config);
        [[nodiscard]] std::vector<const ResolvedAlchemyItem*> List(
            ServiceTier a_tier) const;
        [[nodiscard]] const ResolvedAlchemyItem* Find(
            std::string_view a_id) const;

    private:
        std::vector<ResolvedAlchemyItem> items_;
    };
}
