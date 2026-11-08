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

#include "NPCEnchantingServices/InputHandler.h"

#include "NPCEnchantingServices/MenuController.h"

namespace NPCES
{
    InputHandler& InputHandler::GetSingleton()
    {
        static InputHandler singleton;
        return singleton;
    }

    void InputHandler::Configure(const Config& a_config)
    {
        enabled_ = a_config.ui.enableHotkey;
        interactionScanCode_ = a_config.ui.interactionScanCode;
    }

    void InputHandler::Register()
    {
        if (!enabled_) {
            SKSE::log::info(
                "Debug hotkey is disabled; dialogue is the main access path.");
            return;
        }
        if (registered_) {
            return;
        }
        if (const auto input = RE::BSInputDeviceManager::GetSingleton()) {
            input->AddEventSink(this);
            registered_ = true;
            SKSE::log::info(
                "Debug hotkey registered (scan code {}).",
                interactionScanCode_);
        } else {
            SKSE::log::error("BSInputDeviceManager is unavailable.");
        }
    }

    RE::BSEventNotifyControl InputHandler::ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>*)
    {
        if (!a_event || !*a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        const auto ui = RE::UI::GetSingleton();
        if (!ui || ui->GameIsPaused()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        for (auto event = *a_event; event; event = event->next) {
            const auto button = event->AsButtonEvent();
            if (!button ||
                button->GetDevice() != RE::INPUT_DEVICE::kKeyboard ||
                !button->IsDown() ||
                button->GetIDCode() != interactionScanCode_) {
                continue;
            }

            const auto pickData = RE::CrosshairPickData::GetSingleton();
            if (!pickData) {
                RE::SendHUDMessage::ShowHUDMessage("No crosshair target.");
                break;
            }

            auto target = pickData->targetActor.get();
            if (!target) {
                target = pickData->target.get();
            }
            const auto actor = target ? target->As<RE::Actor>() : nullptr;
            if (!actor) {
                RE::SendHUDMessage::ShowHUDMessage(
                    "Aim at a configured enchanter.");
                break;
            }

            MenuController::GetSingleton().OpenForTarget(actor);
            break;
        }
        return RE::BSEventNotifyControl::kContinue;
    }
}
