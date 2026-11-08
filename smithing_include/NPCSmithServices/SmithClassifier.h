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

#include <mutex>
#include <unordered_set>

#include "NPCSmithServices/Config.h"

#include "RE/Skyrim.h"

#include <optional>
#include <string>

namespace NPCSS
{
    struct SmithContext
    {
        RE::ActorHandle actor;
        RE::FormID baseFormID{ 0 };
        RE::FormID workshopKey{ 0 };
        RE::FormID artisanReferenceFormID{ 0 };
        SmithTier tier{ SmithTier::kAdept };
        bool isEorlund{ false };
        bool isBaldor{ false };
        bool isGunmar{ false };
        bool isGlover{ false };
        bool isSolstheimSmith{ false };
        bool servicesUnlocked{ true };
        std::string lockedReason;
        std::string displayName;
        std::string artisanDisplayName;
        std::string courierLocation;
    };

    struct WorkshopArtisan
    {
        RE::FormID referenceFormID{ 0 };
        RE::FormID baseFormID{ 0 };
        std::string displayName;
        std::string courierLocation;
    };

    class SmithClassifier
    {
    public:
        static SmithClassifier& GetSingleton();

        void Configure(const Config& a_config);
        [[nodiscard]] std::optional<SmithContext> Classify(RE::Actor* a_actor) const;
        [[nodiscard]] RE::FormID WorkshopKeyForBase(
            RE::FormID a_baseFormID) const noexcept;
        [[nodiscard]] bool OrderOwnerIsDead(
            RE::FormID a_workshopKey,
            RE::FormID a_originalSmithReference) const;
        [[nodiscard]] std::optional<WorkshopArtisan> ArtisanForWorkshop(
            RE::FormID a_workshopKey,
            RE::FormID a_preferredSmithReference) const;

    private:
        const Config* config_{ nullptr };
        // Mod-added artisans already written to the log this session.
        mutable std::unordered_set<RE::FormID> loggedModArtisans_;
        mutable std::mutex loggedMutex_;
    };
}
