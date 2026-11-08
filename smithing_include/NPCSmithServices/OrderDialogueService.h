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

#include "RE/Skyrim.h"
#include "SKSE/Events.h"

namespace NPCSS
{
    class OrderDialogueService final :
        public RE::BSTEventSink<RE::TESActivateEvent>,
        public RE::BSTEventSink<SKSE::CrosshairRefEvent>
    {
    public:
        static OrderDialogueService& GetSingleton();

        void Configure(bool a_ordersEnabled);
        void Register();
        void RefreshForActor(RE::Actor* a_actor);

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESActivateEvent*,
            RE::BSTEventSource<RE::TESActivateEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(
            const SKSE::CrosshairRefEvent*,
            RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override;

    private:
        bool registered_{ false };
        bool ordersEnabled_{ false };
    };
}
