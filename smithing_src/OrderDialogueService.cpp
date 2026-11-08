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

#include "NPCSmithServices/OrderDialogueService.h"

#include "NPCSmithServices/OrderService.h"
#include "NPCSmithServices/SmithClassifier.h"
#include "NPCEnchantingServices/EnchanterClassifier.h"
#include "NPCAlchemyServices/AlchemistClassifier.h"

namespace
{
    constexpr std::string_view kAvailabilityGlobal =
        "AOS_HasOrdersForCurrentSpeaker";
    constexpr std::string_view kServicesGlobal =
        "AOS_SmithServicesForCurrentSpeaker";
}

namespace NPCSS
{
    OrderDialogueService& OrderDialogueService::GetSingleton()
    {
        static OrderDialogueService singleton;
        return singleton;
    }

    void OrderDialogueService::Configure(bool a_ordersEnabled)
    {
        ordersEnabled_ = a_ordersEnabled;
    }

    void OrderDialogueService::Register()
    {
        if (registered_) {
            return;
        }
        const auto events = RE::ScriptEventSourceHolder::GetSingleton();
        const auto crosshair = SKSE::GetCrosshairRefEventSource();
        if (!events || !crosshair) {
            SKSE::log::error(
                "Could not register smithing-order dialogue availability events.");
            return;
        }
        events->AddEventSink<RE::TESActivateEvent>(this);
        crosshair->AddEventSink(this);
        registered_ = true;
    }

    void OrderDialogueService::RefreshForActor(RE::Actor* a_actor)
    {
        const auto ordersGlobal =
            RE::TESForm::LookupByEditorID<RE::TESGlobal>(
                kAvailabilityGlobal);
        const auto servicesGlobal =
            RE::TESForm::LookupByEditorID<RE::TESGlobal>(
                kServicesGlobal);
        if (!ordersGlobal || !servicesGlobal) {
            SKSE::log::error(
                "Smithing dialogue availability globals were not found.");
            return;
        }
        if (ordersEnabled_) {
            OrderService::GetSingleton().Update();
        }
        const auto context =
            SmithClassifier::GetSingleton().Classify(a_actor);
        servicesGlobal->value =
            context && context->servicesUnlocked ? 1.0F : 0.0F;
        // Only unlocked services are exposed in dialogue. Backend checks still
        // revalidate access when an existing menu command is executed.
        const auto publishAccess = [](std::string_view a_name, const auto& a_context) {
            if (const auto global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(a_name)) {
                global->value = a_context && a_context->servicesUnlocked ? 1.0F : 0.0F;
            }
        };
        publishAccess("AOS_SmithAccessForCurrentSpeaker", context);
        publishAccess("AOS_EnchantAccessForCurrentSpeaker",
            NPCES::EnchanterClassifier::GetSingleton().Classify(a_actor));
        publishAccess("AOS_AlchemyAccessForCurrentSpeaker",
            NPCAS::AlchemistClassifier::GetSingleton().Classify(a_actor));
        ordersGlobal->value = ordersEnabled_ && context && context->servicesUnlocked &&
                OrderService::GetSingleton().HasActiveOrdersForSmith(a_actor) ?
            1.0F :
            0.0F;
    }

    RE::BSEventNotifyControl OrderDialogueService::ProcessEvent(
        const RE::TESActivateEvent* a_event,
        RE::BSTEventSource<RE::TESActivateEvent>*)
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (a_event && player && a_event->actionRef.get() == player) {
            RefreshForActor(
                a_event->objectActivated ?
                    a_event->objectActivated->As<RE::Actor>() :
                    nullptr);
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl OrderDialogueService::ProcessEvent(
        const SKSE::CrosshairRefEvent* a_event,
        RE::BSTEventSource<SKSE::CrosshairRefEvent>*)
    {
        RefreshForActor(
            a_event && a_event->crosshairRef ?
                a_event->crosshairRef->As<RE::Actor>() :
                nullptr);
        return RE::BSEventNotifyControl::kContinue;
    }
}
