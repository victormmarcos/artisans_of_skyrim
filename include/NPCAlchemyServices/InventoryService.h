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

#include "NPCAlchemyServices/AlchemistClassifier.h"
#include "NPCAlchemyServices/AlchemyCatalog.h"
#include "NPCAlchemyServices/Config.h"

#include <string>
#include <vector>

namespace NPCAS
{
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
        [[nodiscard]] std::vector<const ResolvedAlchemyItem*> ListItems(
            const AlchemistContext& a_alchemist) const;
        [[nodiscard]] std::int32_t Price(
            const ResolvedAlchemyItem& a_item) const;
        [[nodiscard]] TransactionResult Purchase(
            const AlchemistContext& a_alchemist,
            const ResolvedAlchemyItem& a_item) const;
        [[nodiscard]] double PlayerServiceDiscount() const;

    private:
        const Config* config_{ nullptr };
    };
}
