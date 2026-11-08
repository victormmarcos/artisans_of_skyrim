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

#include "ArtisansOfSkyrim/PapyrusBridge.h"

#include "ArtisansOfSkyrim/UserSettings.h"

#include "NPCAlchemyServices/MenuController.h"
#include "NPCEnchantingServices/MenuController.h"
#include "NPCSmithServices/MenuController.h"
#include "NPCSmithServices/OrderDialogueService.h"
#include "NPCSmithServices/OrderService.h"

namespace
{
    constexpr const char* kScriptName = "ArtisansOfSkyrim";

    void OpenSmithCraftMenu(
        RE::StaticFunctionTag*,
        RE::Actor* a_smith)
    {
        if (!a_smith) {
            RE::SendHUDMessage::ShowHUDMessage("Artisans of Skyrim: missing smith actor.");
            return;
        }
        const auto handle = a_smith->GetHandle();
        SKSE::log::info("Dialogue requested smithing craft menu for {:08X}.", a_smith->GetFormID());
        SKSE::GetTaskInterface()->AddTask([handle] {
            const auto reference = handle.get();
            if (const auto actor = reference ? reference->As<RE::Actor>() : nullptr)
                NPCSS::MenuController::GetSingleton().OpenCraftingForTarget(actor);
        });
    }

    void OpenSmithImproveMenu(
        RE::StaticFunctionTag*,
        RE::Actor* a_smith)
    {
        if (!a_smith) {
            RE::SendHUDMessage::ShowHUDMessage("Artisans of Skyrim: missing smith actor.");
            return;
        }
        const auto handle = a_smith->GetHandle();
        SKSE::log::info("Dialogue requested smithing improvement menu for {:08X}.", a_smith->GetFormID());
        SKSE::GetTaskInterface()->AddTask([handle] {
            const auto reference = handle.get();
            if (const auto actor = reference ? reference->As<RE::Actor>() : nullptr)
                NPCSS::MenuController::GetSingleton().OpenImprovementForTarget(actor);
        });
    }

    void OpenEnchantMenu(
        RE::StaticFunctionTag*,
        RE::Actor* a_enchanter)
    {
        if (!a_enchanter) {
            RE::SendHUDMessage::ShowHUDMessage("Artisans of Skyrim: missing enchanter actor.");
            return;
        }
        const auto handle = a_enchanter->GetHandle();
        SKSE::log::info("Dialogue requested enchanting menu for {:08X}.", a_enchanter->GetFormID());
        SKSE::GetTaskInterface()->AddTask([handle] {
            const auto reference = handle.get();
            if (const auto actor = reference ? reference->As<RE::Actor>() : nullptr)
                NPCES::MenuController::GetSingleton().OpenForTarget(actor);
        });
    }

    void OpenAlchemyMenu(
        RE::StaticFunctionTag*,
        RE::Actor* a_alchemist)
    {
        if (!a_alchemist) {
            RE::SendHUDMessage::ShowHUDMessage("Artisans of Skyrim: missing alchemist actor.");
            return;
        }
        const auto handle = a_alchemist->GetHandle();
        SKSE::log::info("Dialogue requested alchemy menu for {:08X}.", a_alchemist->GetFormID());
        SKSE::GetTaskInterface()->AddTask([handle] {
            const auto reference = handle.get();
            if (const auto actor = reference ? reference->As<RE::Actor>() : nullptr)
                NPCAS::MenuController::GetSingleton().OpenForTarget(actor);
        });
    }

    void CollectSmithOrders(
        RE::StaticFunctionTag*,
        RE::Actor* a_smith)
    {
        if (!a_smith) {
            RE::SendHUDMessage::ShowHUDMessage("Artisans of Skyrim: missing smith actor.");
            return;
        }
        const auto handle = a_smith->GetHandle();
        SKSE::log::info("Dialogue requested smithing order collection for {:08X}.", a_smith->GetFormID());
        SKSE::GetTaskInterface()->AddTask([handle] {
            const auto reference = handle.get();
            if (const auto actor = reference ? reference->As<RE::Actor>() : nullptr)
                NPCSS::MenuController::GetSingleton().OpenCollectionForTarget(actor);
        });
    }

    // Called by AOS_MCM when MCM Helper reports a changed setting. Both typed
    // values are passed; the setting id prefix (f/i) decides which one is used.
    void ApplyMcmSetting(
        RE::StaticFunctionTag*,
        RE::BSFixedString a_settingId,
        float a_floatValue,
        std::int32_t a_intValue)
    {
        const std::string_view settingId{ a_settingId.c_str() ? a_settingId.c_str() : "" };
        if (AOS::UserSettings::RecordMcmChange(settingId, a_floatValue, a_intValue)) {
            AOS::UserSettings::QueueReload();
        }
    }

}

namespace AOS::PapyrusBridge
{
    bool Register(RE::BSScript::IVirtualMachine* a_vm)
    {
        if (!a_vm) {
            return false;
        }

        a_vm->RegisterFunction(
            "OpenSmithCraftMenu",
            kScriptName,
            OpenSmithCraftMenu);
        a_vm->RegisterFunction(
            "OpenSmithImproveMenu",
            kScriptName,
            OpenSmithImproveMenu);
        a_vm->RegisterFunction(
            "OpenEnchantMenu",
            kScriptName,
            OpenEnchantMenu);
        a_vm->RegisterFunction(
            "OpenAlchemyMenu",
            kScriptName,
            OpenAlchemyMenu);
        a_vm->RegisterFunction(
            "CollectSmithOrders",
            kScriptName,
            CollectSmithOrders);
        a_vm->RegisterFunction(
            "ApplyMcmSetting",
            kScriptName,
            ApplyMcmSetting);
        SKSE::log::info("Papyrus bridge registered for {}.", kScriptName);
        return true;
    }
}
