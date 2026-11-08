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

#include "ArtisansOfSkyrim/ServiceMenu.h"
#include "ArtisansOfSkyrim/InterfaceTexts.h"
#include "ArtisansOfSkyrim/Localization.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>

namespace
{
    std::optional<AOS::ServicePage> pendingPage;
    std::mutex pendingMutex;
    std::uint64_t generation = 0;
    // Set by the menu, read by the input sink.
    std::atomic<bool> g_previewActive{ false };
    std::atomic<bool> g_mouseRotation{ false };
    // Fallback rotation input (used only when the game's own rotation handler
    // refuses the event), collected by the input sink and consumed per frame.
    std::atomic<bool> g_rotationPathLogged{ false };
    std::atomic<float> g_mouseDX{ 0.0F };
    std::atomic<float> g_mouseDY{ 0.0F };
    std::atomic<float> g_stickX{ 0.0F };
    std::atomic<float> g_stickY{ 0.0F };

    void AddAtomic(std::atomic<float>& a_value, float a_delta)
    {
        float current = a_value.load();
        while (!a_value.compare_exchange_weak(current, current + a_delta)) {}
    }

    bool GameRotationAccepts(RE::InputEvent* a_event)
    {
        const auto manager = RE::Inventory3DManager::GetSingleton();
        const bool accepted = manager && manager->CanProcess(a_event);
        if (!g_rotationPathLogged.exchange(true)) {
            SKSE::log::info(
                "Service menu preview rotation: {}",
                accepted ? "game inventory handler" : "fallback (game handler refused input)");
        }
        return accepted;
    }

    // Rotation matrix for a turn of a_angle radians around a unit axis.
    RE::NiMatrix3 AxisRotation(const RE::NiPoint3& a_axis, float a_angle)
    {
        const float c = std::cos(a_angle);
        const float s = std::sin(a_angle);
        const float t = 1.0F - c;
        const float x = a_axis.x, y = a_axis.y, z = a_axis.z;
        RE::NiMatrix3 m;
        m.entry[0][0] = t * x * x + c;     m.entry[0][1] = t * x * y - s * z; m.entry[0][2] = t * x * z + s * y;
        m.entry[1][0] = t * x * y + s * z; m.entry[1][1] = t * y * y + c;     m.entry[1][2] = t * y * z - s * x;
        m.entry[2][0] = t * x * z - s * y; m.entry[2][1] = t * y * z + s * x; m.entry[2][2] = t * z * z + c;
        return m;
    }

    AOS::ServiceMenu* Current()
    {
        const auto ui = RE::UI::GetSingleton();
        const auto menu = ui ? ui->GetMenu(AOS::ServiceMenu::MENU_NAME) : nullptr;
        return menu ? static_cast<AOS::ServiceMenu*>(menu.get()) : nullptr;
    }

    class CommandHandler final : public RE::GFxFunctionHandler
    {
        void Call(Params& params) override
        {
            if (params.argCount < 3 || !params.args[0].IsString() ||
                !params.args[1].IsNumber() || !params.args[2].IsNumber()) return;
            const auto index = params.args[1].GetNumber();
            const auto revision = params.args[2].GetNumber();
            if (!std::isfinite(index) || !std::isfinite(revision) || index < 0 ||
                index > 1000000 || revision < 0 || revision > 9007199254740991.0) return;
            if (auto menu = Current()) {
                std::string value;
                if (params.argCount >= 4 && params.args[3].IsString()) {
                    value = params.args[3].GetString();
                }
                menu->Command(params.args[0].GetString(), static_cast<std::size_t>(index),
                    static_cast<std::uint64_t>(revision), std::move(value));
            }
        }
    };

    // Native keyboard/controller navigation avoids Scaleform cursor and key-repeat
    // differences across supported Skyrim runtimes.
    class ControllerInput final : public RE::BSTEventSink<RE::InputEvent*>
    {
        struct ButtonState
        {
            bool down{ false };
        };

        std::array<ButtonState, 21> states_{};
        int stickDirection_{ 0 };
        int mouseDistance_{ 0 };
        std::chrono::steady_clock::time_point mouseWindow_{};
        std::chrono::steady_clock::time_point stickRepeatAt_{};

        static constexpr auto kRepeatDelay = std::chrono::milliseconds(700);
        static constexpr auto kRepeatInterval = std::chrono::milliseconds(350);

        static void DispatchDirection(int direction)
        {
            switch (direction) {
            case 1: Dispatch("up"); break;
            case 2: Dispatch("down"); break;
            case 3: Dispatch("left"); break;
            case 4: Dispatch("right"); break;
            default: break;
            }
        }

        static int ButtonSlot(std::uint32_t id)
        {
            switch (id) {
            case 0x0001: return 0;
            case 0x0002: return 1;
            case 0x0004: return 2;
            case 0x0008: return 3;
            case 0x1000: return 4;
            case 0x2000: return 5;
            case 0x4000: return 6;
            case 0x8000: return 7;
            case 0x0100: return 8;
            case 0x0200: return 9;
            case 0x0010: return 10;
            default: return -1;
            }
        }

        static std::string CommandFor(std::uint32_t id)
        {
            switch (id) {
            case 0x0001: return "up";
            case 0x0002: return "down";
            case 0x0004: return "left";
            case 0x0008: return "right";
            case 0x1000: return "activate";
            case 0x2000: return "back";
            case 0x4000: return "craft";
            case 0x8000: return "filters";
            case 0x0100: return "previousMode";
            case 0x0200: return "nextMode";
            case 0x0010: return {};
            default: return {};
            }
        }

        static int KeyboardSlot(std::uint32_t id)
        {
            using Key = RE::BSKeyboardDevice::Keys;
            switch (id) {
            case Key::kUp: return 11;
            case Key::kDown: return 12;
            case Key::kLeft: return 13;
            case Key::kRight: return 14;
            case Key::kEnter:
            case Key::kKP_Enter: return 15;
            case Key::kEscape: return 16;
            case Key::kF: return 17;
            case Key::kTab: return 18;
            case Key::kC: return 19;
            case Key::kR: return 20;
            default: return -1;
            }
        }

        static std::string KeyboardCommand(std::uint32_t id)
        {
            using Key = RE::BSKeyboardDevice::Keys;
            switch (id) {
            case Key::kUp: return "up";
            case Key::kDown: return "down";
            case Key::kLeft: return "left";
            case Key::kRight: return "right";
            case Key::kEnter:
            case Key::kKP_Enter: return "activate";
            case Key::kEscape: return "back";
            case Key::kF: return "filters";
            case Key::kTab: return "alternate";
            case Key::kC: return "craft";
            case Key::kR: return "rename";
            default: return {};
            }
        }

        template <class T>
        void ProcessLeftStick(const T& stick)
        {
            constexpr float deadzone = 0.65F;
            int direction = 0;
            if (std::abs(stick.xValue) >= std::abs(stick.yValue)) {
                if (stick.xValue <= -deadzone) direction = 3;
                else if (stick.xValue >= deadzone) direction = 4;
            } else {
                if (stick.yValue >= deadzone) direction = 1;
                else if (stick.yValue <= -deadzone) direction = 2;
            }
            if (direction == 0) {
                stickDirection_ = 0;
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            if (direction != stickDirection_) {
                stickDirection_ = direction;
                stickRepeatAt_ = now + kRepeatDelay;
                DispatchDirection(direction);
            }
        }

        void SetInputMode(bool gamepad)
        {
            SKSE::GetTaskInterface()->AddUITask([gamepad] {
                if (auto menu = Current()) menu->SetInputMode(gamepad);
            });
        }

        static void Dispatch(std::string command)
        {
            SKSE::GetTaskInterface()->AddUITask([command = std::move(command)] {
                if (auto menu = Current()) menu->Navigate(command);
            });
        }

    public:
        void Reset()
        {
            states_ = {};
            stickDirection_ = 0;
            mouseDistance_ = 0;
            g_stickX = 0.0F;
            g_stickY = 0.0F;
        }

        void Tick()
        {
            if (stickDirection_ == 0 || !AOS::ServiceMenu::IsOpen()) return;
            const auto now = std::chrono::steady_clock::now();
            if (now < stickRepeatAt_) return;
            stickRepeatAt_ = now + kRepeatInterval;
            DispatchDirection(stickDirection_);
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,
            RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (!events || !AOS::ServiceMenu::IsOpen()) {
                Reset();
                return RE::BSEventNotifyControl::kContinue;
            }
            for (auto event = *events; event; event = event->next) {
                if (event->GetDevice() == RE::INPUT_DEVICE::kMouse) {
                    bool intentional = false;
                    if (const auto motion = event->AsMouseMoveEvent()) {
                        if (g_mouseRotation && g_previewActive && !GameRotationAccepts(event)) {
                            AddAtomic(g_mouseDX, static_cast<float>(motion->mouseInputX));
                            AddAtomic(g_mouseDY, static_cast<float>(motion->mouseInputY));
                        }
                        const auto now = std::chrono::steady_clock::now();
                        if (now - mouseWindow_ > std::chrono::milliseconds(150)) {
                            mouseDistance_ = 0;
                            mouseWindow_ = now;
                        }
                        const auto distance = std::abs(motion->mouseInputX) + std::abs(motion->mouseInputY);
                        if (distance >= 2) mouseDistance_ += distance;
                        intentional = mouseDistance_ >= 6;
                    } else if (const auto mouseButton = event->AsButtonEvent(); mouseButton && mouseButton->IsDown()) {
                        intentional = true;
                        const auto id = mouseButton->GetIDCode();
                        if (id == 8 || id == 9) {
                            SetInputMode(false);
                            Dispatch(id == 8 ? "wheelUp" : "wheelDown");
                        }
                    }
                    if (intentional) {
                        stickDirection_ = 0;
                        SetInputMode(false);
                    }
                    continue;
                }

                if (const auto stick = event->AsThumbstickEvent();
                    stick && event->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                    if (!stick->IsRight()) {
                        ProcessLeftStick(*stick);
                    } else {
                        // Right stick turns the 3D preview: the game's own handler
                        // when it accepts the event, otherwise the fallback.
                        constexpr float rotateDeadzone = 0.15F;
                        const bool fallback = g_previewActive && !GameRotationAccepts(event);
                        g_stickX = fallback && std::abs(stick->xValue) >= rotateDeadzone ? stick->xValue : 0.0F;
                        g_stickY = fallback && std::abs(stick->yValue) >= rotateDeadzone ? stick->yValue : 0.0F;
                    }
                    // Ignore neutral polling so prompts change only after real input.
                    constexpr float inputThreshold = 0.20F;
                    if (std::abs(stick->xValue) >= inputThreshold ||
                        std::abs(stick->yValue) >= inputThreshold) {
                        mouseDistance_ = 0;
                        SetInputMode(true);
                    }
                    continue;
                }

                const auto button = event->AsButtonEvent();
                if (!button) continue;
                if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                    if (button->IsDown()) { stickDirection_ = 0; SetInputMode(false); }
                    const auto id = button->GetIDCode();
                    if (const auto menu = Current();
                        menu && menu->IsTextInputActive()) {
                        const auto textSlot = KeyboardSlot(id);
                        if (button->IsUp() && textSlot >= 0) {
                            states_[static_cast<std::size_t>(textSlot)] = {};
                        }
                        if (button->IsDown()) {
                            using Key = RE::BSKeyboardDevice::Keys;
                            if (id == Key::kEnter || id == Key::kKP_Enter) {
                                Dispatch("acceptText");
                            } else if (id == Key::kEscape) {
                                Dispatch("cancelText");
                            }
                        }
                        continue;
                    }
                    const auto slot = KeyboardSlot(id);
                    if (slot < 0) continue;
                    auto& state = states_[static_cast<std::size_t>(slot)];
                    if (button->IsUp()) {
                        state = {};
                        continue;
                    }
                    if (button->IsDown() && !state.down) {
                        state.down = true;
                        auto command = KeyboardCommand(id);
                        if (!command.empty()) Dispatch(std::move(command));
                    }
                    continue;
                }
                if (button->GetDevice() != RE::INPUT_DEVICE::kGamepad) continue;
                if (button->IsDown()) { mouseDistance_ = 0; SetInputMode(true); }
                const auto id = button->GetIDCode();
                const auto slot = ButtonSlot(id);
                if (slot < 0) continue;
                auto& state = states_[static_cast<std::size_t>(slot)];
                if (button->IsUp()) {
                    state = {};
                    continue;
                }

                bool fire = false;
                if (button->IsDown() && !state.down) {
                    state.down = true;
                    fire = true;
                }
                if (fire) {
                    auto command = CommandFor(id);
                    if (!command.empty()) Dispatch(std::move(command));
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    } controllerInput;

    RE::ExtraDataList* ValidExtra(const AOS::ServiceRow& row)
    {
        if (!row.object || !row.extra) return nullptr;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return nullptr;
        const auto inventory = player->GetInventory();
        const auto it = inventory.find(row.object);
        if (it == inventory.end() || it->second.first <= 0 || !it->second.second ||
            !it->second.second->extraLists) return nullptr;
        for (auto extra : *it->second.second->extraLists) if (extra == row.extra) return extra;
        return nullptr;
    }

    void PreparePage(AOS::ServicePage& page)
    {
        // Actor statistics and inventory queries belong to the game task queue,
        // not the Scaleform advance/render path used by Publish.
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
        page.gold = player && gold ? player->GetItemCount(gold) : 0;
        page.carry = player ? player->GetWeightInContainer() : 0;
        page.capacity = player ? player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) : 0;
        for (auto& row : page.rows) {
            if (!row.object) continue;
            RE::InventoryEntryData entry(row.object, 1);
            entry.AddExtraList(ValidExtra(row));
            row.weight = std::max(entry.GetWeight(), 0.0F);
            if (player && row.object->Is(RE::FormType::Weapon)) {
                row.statLabel = "Damage";
                row.stat = player->GetDamage(&entry);
            } else if (player && row.object->Is(RE::FormType::Armor)) {
                row.statLabel = "Armor";
                row.stat = player->GetArmorValue(&entry);
            } else if (const auto ammo = row.object->As<RE::TESAmmo>()) {
                row.statLabel = "Damage";
                row.stat = ammo->data.damage;
            }
            if (row.targetHealth > 0) {
                row.resultStat = row.stat * row.targetHealth / std::max(row.currentHealth, 0.01F);
            }
        }
        SKSE::log::debug("Service menu data prepared: key='{}', rows={}", page.key, page.rows.size());
    }
}

namespace AOS
{
    void ServiceMenu::Register()
    {
        // Called by the plugin's single SKSE listener on InputLoaded. A second
        // RegisterListener call for the same plugin can replace/prevent the
        // DataLoaded callback, leaving every service uninitialized.
        static bool registered = false;
        if (registered) return;
        const auto ui = RE::UI::GetSingleton();
        const auto input = RE::BSInputDeviceManager::GetSingleton();
        if (!ui || !input) {
            SKSE::log::error("InputLoaded arrived before the UI/input singletons were ready.");
            return;
        }
        ui->Register(MENU_NAME, Create);
        input->AddEventSink(&controllerInput);
        registered = true;
        SKSE::log::info("Artisans service menu registered.");
    }

    bool ServiceMenu::IsOpen()
    {
        auto ui = RE::UI::GetSingleton();
        return ui && ui->IsMenuOpen(MENU_NAME);
    }

    void ServiceMenu::Present(ServicePage page)
    {
        SKSE::log::debug("Menu request queued: key='{}', layout='{}', rows={}",
            page.key, page.layout, page.rows.size());
        SKSE::GetTaskInterface()->AddTask([page = std::move(page)]() mutable {
            PreparePage(page);
            SKSE::GetTaskInterface()->AddUITask([page = std::move(page)]() mutable {
                if (auto menu = Current(); menu && IsOpen()) {
                    menu->SetPage(std::move(page));
                    return;
                }
                {
                    std::scoped_lock lock(pendingMutex);
                    pendingPage = std::move(page);
                }
                if (auto queue = RE::UIMessageQueue::GetSingleton())
                    queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
            });
        });
    }

    void ServiceMenu::Close()
    {
        { std::scoped_lock lock(pendingMutex); pendingPage.reset(); }
        if (auto queue = RE::UIMessageQueue::GetSingleton())
            queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
    }

    void ServiceMenu::Status(std::string message)
    {
        RE::SendHUDMessage::ShowHUDMessage(message.c_str());
    }

    RE::IMenu* ServiceMenu::Create() { return new ServiceMenu(); }

    ServiceMenu::ServiceMenu()
    {
        depthPriority = 5;
        inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode;
        menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame,
            RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kModal,
            RE::UI_MENU_FLAGS::kInventoryItemMenu, RE::UI_MENU_FLAGS::kUsesBlurredBackground,
            RE::UI_MENU_FLAGS::kDisablePauseMenu);
        const auto manager = RE::BSScaleformManager::GetSingleton();
        if (!manager || !manager->LoadMovie(this, uiMovie, "ArtisansOfSkyrim/ServiceMenu")) {
            SKSE::log::error("Could not load Interface/ArtisansOfSkyrim/ServiceMenu.swf.");
            return;
        }
        RE::GFxValue native;
        RE::GPtr<CommandHandler> handler(new CommandHandler());
        uiMovie->CreateFunction(&native, handler.get());
        uiMovie->SetVariable("_root.aosCommand", native);
    }

    ServiceMenu::~ServiceMenu()
    {
        ReleasePreview();
        if (cursorCaptured_) {
            if (auto cursor = RE::MenuCursor::GetSingleton()) cursor->SetCursorVisibility(previousCursorVisible_);
        }
    }

    RE::UI_MESSAGE_RESULTS ServiceMenu::ProcessMessage(RE::UIMessage& message)
    {
        if (message.type == RE::UI_MESSAGE_TYPE::kShow) {
            SKSE::log::debug("Service menu show message received.");
            closing_ = false;
            if (!uiMovie) {
                Status("Artisans menu is missing. Reinstall the complete Artisans of Skyrim package.");
                Close();
                return RE::UI_MESSAGE_RESULTS::kHandled;
            }
            std::scoped_lock lock(pendingMutex);
            if (pendingPage) {
                page_ = std::move(*pendingPage);
                pendingPage.reset();
                revision_ = ++generation;
                ready_ = false;
            }
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        if (message.type == RE::UI_MESSAGE_TYPE::kHide || message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
            closing_ = true;
            controllerInput.Reset();
            ++generation;
            ReleasePreview();
            page_ = {};
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return IMenu::ProcessMessage(message);
    }

    void ServiceMenu::SetPage(ServicePage page)
    {
        if (textInput_) {
            if (auto controls = RE::ControlMap::GetSingleton()) controls->AllowTextInput(false);
            textInput_ = false;
        }
        ClearPreviewItem();
        page_ = std::move(page);
        revision_ = ++generation;
        commandPending_ = false;
        ready_ = false;
    }

    void ServiceMenu::AdvanceMovie(float interval, std::uint32_t)
    {
        if (!uiMovie || closing_) return;
        frameSeconds_ = std::clamp(interval, 0.001F, 0.1F);
        controllerInput.Tick();
        uiMovie->Advance(interval); // seconds, not the absolute game timer
        if (!ready_ && uiMovie->IsAvailable("_root.setPage")) Publish();
    }

    void ServiceMenu::PostDisplay()
    {
        IMenu::PostDisplay();
        if (previewActive_ && !closing_) {
            UpdatePreviewModel();
            if (auto manager = RE::Inventory3DManager::GetSingleton()) manager->Render();
        }
    }

    void ServiceMenu::ClearPreviewItem()
    {
        if (previewActive_) {
            if (auto manager = RE::Inventory3DManager::GetSingleton()) manager->UnloadInventoryItem();
        }
        previewIndex_ = static_cast<std::size_t>(-1);
    }

    void ServiceMenu::ReleasePreview()
    {
        if (textInput_) {
            if (auto controls = RE::ControlMap::GetSingleton()) controls->AllowTextInput(false);
            textInput_ = false;
        }
        SetMouseRotation(false);
        if (previewActive_) {
            if (auto manager = RE::Inventory3DManager::GetSingleton()) {
                manager->UnloadInventoryItem();
                // Leave the shared manager exactly as the inventory expects it.
                if (handlerAdded_) {
                    if (auto controls = RE::MenuControls::GetSingleton()) controls->RemoveHandler(manager);
                }
                if (itemPosSaved_) {
                    manager->itemPos = savedItemPos_;
                    manager->GetRuntimeData().enableUserInput = savedUserInput_;
                }
                manager->End3D();
            }
        }
        handlerAdded_ = false;
        itemPosSaved_ = false;
        previewActive_ = false;
        g_previewActive = false;
        g_rotationPathLogged = false;
        previewIndex_ = static_cast<std::size_t>(-1);
    }

    void ServiceMenu::Preview(std::size_t index)
    {
        if (index >= page_.rows.size() || index == previewIndex_) return;
        const auto manager = RE::Inventory3DManager::GetSingleton();
        if (!manager) return;
        if (!previewActive_) {
            manager->Begin3D(RE::INTERFACE_LIGHT_SCHEME::kInventory);
            previewActive_ = true;
            g_previewActive = true;
            savedItemPos_ = manager->itemPos;
            itemPosSaved_ = true;
            // Same input path as the inventory: the manager handles mouse and
            // right-stick rotation itself while it is a menu input handler.
            auto& runtime = manager->GetRuntimeData();
            savedUserInput_ = runtime.enableUserInput;
            runtime.enableUserInput = true;
            if (auto controls = RE::MenuControls::GetSingleton()) {
                const auto handler = static_cast<RE::MenuEventHandler*>(manager);
                const bool present = std::ranges::find(controls->handlers, handler) != controls->handlers.end();
                if (!present) {
                    controls->AddHandler(handler);
                    handlerAdded_ = true;
                }
                SKSE::log::debug("Service menu preview: game rotation handler {}.",
                    present ? "already registered" : "registered for this menu");
            }
            PlacePreview();
        }
        manager->UnloadInventoryItem();
        const auto& row = page_.rows[index];
        SKSE::log::debug("Service menu preview: key='{}', row={}, form={:08X}",
            page_.key, index, row.object ? row.object->GetFormID() : 0);
        if (row.object) manager->LoadInventoryItem(row.object, ValidExtra(row));
        previewIndex_ = index;
        placementLogged_ = false;
    }

    // Mouse rotation is active only while the menu reports a held button over
    // the preview (as the inventory does), so plain mouse movement never turns
    // the model.
    void ServiceMenu::SetMouseRotation(bool a_active)
    {
        if (a_active == mouseRotation_) return;
        mouseRotation_ = a_active;
        g_mouseRotation = a_active;
        g_mouseDX = 0.0F;
        g_mouseDY = 0.0F;
        RE::Inventory3DManager::SetMouseRotation(a_active);
    }

    // Moves the vanilla item anchor once so that it lands on the centre of the
    // menu's preview area. The game then places, centres and scales every item
    // around that anchor itself, exactly as in the inventory.
    void ServiceMenu::PlacePreview()
    {
        if (!previewActive_ || !previewTargetSet_ || !itemPosSaved_) return;
        const auto manager = RE::Inventory3DManager::GetSingleton();
        const auto scene = RE::UI3DSceneManager::GetSingleton();
        if (!manager || !scene || !scene->camera) return;
        auto* camera = scene->camera.get();

        // WorldPtToScreenPt3 measures y from the bottom of the screen.
        const float targetX = previewTargetX_;
        const float targetY = 1.0F - previewTargetY_;
        const std::array<RE::NiPoint3, 3> directions{ {
            { 1.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F }, { 0.0F, 0.0F, 1.0F },
        } };
        auto position = savedItemPos_;
        for (int iteration = 0; iteration < 4; ++iteration) {
            float sx = 0.0F, sy = 0.0F, sz = 0.0F;
            if (!camera->WorldPtToScreenPt3(position, sx, sy, sz, 1e-5F)) return;
            const float ex = targetX - sx;
            const float ey = targetY - sy;
            if (std::abs(ex) < 0.001F && std::abs(ey) < 0.001F) break;
            std::array<float, 3> dx{}, dy{};
            for (std::size_t i = 0; i < directions.size(); ++i) {
                float ax = 0.0F, ay = 0.0F, az = 0.0F;
                if (camera->WorldPtToScreenPt3(position + directions[i], ax, ay, az, 1e-5F)) {
                    dx[i] = ax - sx;
                    dy[i] = ay - sy;
                }
            }
            std::size_t first = 0, second = 1;
            float bestDet = 0.0F;
            for (std::size_t i = 0; i < directions.size(); ++i) {
                for (std::size_t j = i + 1; j < directions.size(); ++j) {
                    const float det = dx[i] * dy[j] - dx[j] * dy[i];
                    if (std::abs(det) > std::abs(bestDet)) { bestDet = det; first = i; second = j; }
                }
            }
            if (std::abs(bestDet) < 1e-9F) return;
            const float u = (ex * dy[second] - dx[second] * ey) / bestDet;
            const float v = (dx[first] * ey - ex * dy[first]) / bestDet;
            position += directions[first] * u + directions[second] * v;
        }
        manager->itemPos = position;
        SKSE::log::debug(
            "Service menu preview anchor: vanilla ({:.1f}, {:.1f}, {:.1f}) -> ({:.1f}, {:.1f}, {:.1f}) for screen ({:.3f}, {:.3f}).",
            savedItemPos_.x, savedItemPos_.y, savedItemPos_.z,
            position.x, position.y, position.z,
            previewTargetX_, previewTargetY_);
    }

    // Fallback rotation (only when the game's handler refused the input):
    // turns the item around its own centre, without moving its anchor.
    void ServiceMenu::UpdatePreviewModel()
    {
        const auto manager = RE::Inventory3DManager::GetSingleton();
        const auto scene = RE::UI3DSceneManager::GetSingleton();
        if (!manager || !scene || !scene->camera) return;
        auto& models = manager->GetRuntimeData().loadedModels;
        if (models.empty() || !models[0].spModel) return;
        auto* camera = scene->camera.get();
        const auto center = models[0].spModel->worldBound.center;

        float sx = 0.0F, sy = 0.0F, sz = 0.0F;
        if (!camera->WorldPtToScreenPt3(center, sx, sy, sz, 1e-5F)) return;

        if (!placementLogged_ && models[0].spModel->worldBound.radius > 0.0F) {
            placementLogged_ = true;
            SKSE::log::debug(
                "Service menu preview placement: item centre at screen ({:.3f}, {:.3f}), target ({:.3f}, {:.3f}).",
                sx, 1.0F - sy, previewTargetX_, previewTargetY_);
        }

        const float mouseX = g_mouseDX.exchange(0.0F);
        const float mouseY = g_mouseDY.exchange(0.0F);
        const float stickX = g_stickX.load();
        const float stickY = g_stickY.load();
        constexpr float mouseRadiansPerUnit = 0.01F;
        constexpr float stickRadiansPerSecond = 3.0F;
        const float yaw = mouseX * mouseRadiansPerUnit + stickX * stickRadiansPerSecond * frameSeconds_;
        const float pitch = mouseY * mouseRadiansPerUnit - stickY * stickRadiansPerSecond * frameSeconds_;
        if (yaw == 0.0F && pitch == 0.0F) return;

        // Screen axes in world space: the world axis that moves most vertically
        // on screen (pointing up) and the one that moves most horizontally
        // (pointing right).
        struct Axis { RE::NiPoint3 dir; float dx; float dy; };
        std::array<Axis, 3> axes{ {
            { { 1.0F, 0.0F, 0.0F }, 0.0F, 0.0F },
            { { 0.0F, 1.0F, 0.0F }, 0.0F, 0.0F },
            { { 0.0F, 0.0F, 1.0F }, 0.0F, 0.0F },
        } };
        for (auto& axis : axes) {
            float ax = 0.0F, ay = 0.0F, az = 0.0F;
            if (camera->WorldPtToScreenPt3(center + axis.dir, ax, ay, az, 1e-5F)) {
                axis.dx = ax - sx;
                axis.dy = ay - sy;
            }
        }
        const auto share = [](const Axis& a_axis, bool a_vertical) {
            const float primary = a_vertical ? std::abs(a_axis.dy) : std::abs(a_axis.dx);
            return primary / (std::abs(a_axis.dx) + std::abs(a_axis.dy) + 1e-9F);
        };
        std::size_t up = 0;
        for (std::size_t i = 1; i < axes.size(); ++i) {
            if (share(axes[i], true) > share(axes[up], true)) up = i;
        }
        std::size_t right = up == 0 ? 1 : 0;
        for (std::size_t i = 0; i < axes.size(); ++i) {
            if (i != up && share(axes[i], false) > share(axes[right], false)) right = i;
        }
        auto upAxis = axes[up].dir;
        if (axes[up].dy < 0.0F) upAxis = upAxis * -1.0F;
        auto rightAxis = axes[right].dir;
        if (axes[right].dx < 0.0F) rightAxis = rightAxis * -1.0F;

        RE::NiUpdateData updateData{};
        for (auto& model : models) {
            auto* node = model.spModel.get();
            if (!node) continue;
            // Work in the parent's space so the pivot is the item's centre.
            RE::NiTransform parentWorld{};
            if (node->parent) parentWorld = node->parent->world;
            const auto toParent = parentWorld.Invert();
            const auto pivot = toParent * center;
            const auto localUp = toParent.rotate * upAxis;
            const auto localRight = toParent.rotate * rightAxis;
            const auto rotation = AxisRotation(localRight, pitch) * AxisRotation(localUp, yaw);
            node->local.rotate = rotation * node->local.rotate;
            node->local.translate = pivot + rotation * (node->local.translate - pivot);
            node->Update(updateData);
        }
    }

    void ServiceMenu::SetInputMode(bool gamepad)
    {
        if (!uiMovie || closing_) return;
        if (inputModeReady_ && gamepadInput_ == gamepad) return;
        inputModeReady_ = true;
        gamepadInput_ = gamepad;
        if (auto cursor = RE::MenuCursor::GetSingleton()) {
            if (!cursorCaptured_) {
                previousCursorVisible_ = cursor->GetRuntimeData().showCursorCount >= 0;
                cursorCaptured_ = true;
            }
            // CursorMenu renders Skyrim's cursor. Keep the Windows cursor
            // hidden in both modes to avoid a second desktop arrow.
            cursor->SetCursorVisibility(false);
        }
        if (gamepad) menuFlags.reset(RE::UI_MENU_FLAGS::kUsesCursor);
        else menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor);
        // Follow IMenu::RefreshPlatform's CursorMenu protocol, using the last
        // intentional input rather than whether a controller is connected.
        if (auto queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::CursorMenu::MENU_NAME,
                gamepad ? RE::UI_MESSAGE_TYPE::kHide : RE::UI_MESSAGE_TYPE::kShow, nullptr);
        }
        RE::GFxValue argument(gamepad);
        uiMovie->Invoke("_root.setInputMode", nullptr, &argument, 1);
    }

    void ServiceMenu::Navigate(std::string_view command)
    {
        if (!uiMovie || closing_) return;
        RE::GFxValue argument;
        uiMovie->CreateString(&argument, std::string(command).c_str());
        uiMovie->Invoke("_root.navigate", nullptr, &argument, 1);
    }

    void ServiceMenu::Command(std::string command, std::size_t index,
        std::uint64_t revision, std::string value)
    {
        if (closing_ || revision != revision_) return;
        if (command == "uiStage") {
            SKSE::log::debug("Service menu UI stage: key='{}', stage='{}'", page_.key, value);
            return;
        }
        if (command == "preview") {
            const auto expected = revision_;
            SKSE::GetTaskInterface()->AddUITask([index, expected] {
                auto menu = Current();
                if (!menu || !IsOpen() || menu->closing_ || menu->revision_ != expected) return;
                menu->Preview(index);
            });
            return;
        }
        if (command == "clearPreview") { ClearPreviewItem(); return; }
        if (command == "rotateStart" || command == "rotateStop") {
            SetMouseRotation(command == "rotateStart" && previewActive_);
            return;
        }
        if (command == "previewArea") {
            // "x,y": centre of the preview area as screen fractions.
            const auto comma = value.find(',');
            float x = 0.0F, y = 0.0F;
            if (comma != std::string::npos &&
                std::from_chars(value.data(), value.data() + comma, x).ec == std::errc{} &&
                std::from_chars(value.data() + comma + 1, value.data() + value.size(), y).ec == std::errc{} &&
                x > 0.0F && x < 1.0F && y > 0.0F && y < 1.0F) {
                previewTargetX_ = x;
                previewTargetY_ = y;
                previewTargetSet_ = true;
                PlacePreview();
            }
            return;
        }
        if (command == "textOn" || command == "textOff") {
            const bool enable = command == "textOn";
            if (enable != textInput_) {
                if (auto controls = RE::ControlMap::GetSingleton()) controls->AllowTextInput(enable);
                textInput_ = enable;
            }
            return;
        }
        if (command == "close") { closing_ = true; Close(); return; }
        if (commandPending_) return;
        std::function<void()> action;
        if (command == "back") action = page_.back;
        else if (command == "alternate") action = page_.alternate;
        else if (command == "finalize" && page_.canFinalize && page_.finalize) {
            action = [callback = page_.finalize, value = std::move(value)]() mutable {
                callback(std::move(value));
            };
        }
        else if (command == "activate" && index < page_.rows.size() && page_.rows[index].enabled)
            action = page_.rows[index].activate;
        if (!action) return;
        commandPending_ = true;
        // Invalidate duplicate clicks before the inventory task runs.
        ClearPreviewItem();
        const auto expected = revision_;
        SKSE::GetTaskInterface()->AddTask([expected, action = std::move(action)] {
            auto menu = Current();
            if (!menu || !IsOpen() || menu->closing_ || menu->revision_ != expected) return;
            action();
        });
    }

    void ServiceMenu::Publish()
    {
        SKSE::log::debug("Publishing service menu: key='{}', layout='{}', rows={}",
            page_.key, page_.layout, page_.rows.size());
        auto movie = uiMovie.get();
        RE::GFxValue data, rows;
        movie->CreateObject(&data);
        movie->CreateArray(&rows);
        const auto str = [movie](RE::GFxValue& value, const char* key, const std::string& text) {
            RE::GFxValue field;
            movie->CreateString(&field, text.c_str());
            value.SetMember(key, field);
        };
        const auto num = [](RE::GFxValue& value, const char* key, double n) {
            value.SetMember(key, RE::GFxValue(n));
        };
        // Display text goes through the translation file; keys and names do not.
        const auto txt = [&str](RE::GFxValue& value, const char* key, const std::string& text) {
            str(value, key, AOS::Text::Localize(text));
        };
        str(data, "key", page_.key); str(data, "artisan", page_.artisan);
        txt(data, "tier", page_.tier); txt(data, "profession", page_.profession);
        str(data, "layout", page_.layout);
        txt(data, "categoryLabel", page_.categoryLabel);
        txt(data, "groupLabel", page_.groupLabel);
        data.SetMember("showCategoryFilter", RE::GFxValue(page_.showCategoryFilter));
        data.SetMember("showGroupFilter", RE::GFxValue(page_.showGroupFilter));
        str(data, "notice", AOS::Text::LocalizeMessage(page_.notice));
        txt(data, "alternate", page_.alternateLabel);
        txt(data, "finalizeLabel", page_.finalizeLabel);
        str(data, "finalizeItem", page_.finalizeItem);
        txt(data, "selectionOneName", page_.selectionOneName);
        txt(data, "selectionOneEffect", page_.selectionOneEffect);
        txt(data, "selectionTwoName", page_.selectionTwoName);
        txt(data, "selectionTwoEffect", page_.selectionTwoEffect);
        num(data, "selectionOnePrice", page_.selectionOnePrice);
        num(data, "selectionTwoPrice", page_.selectionTwoPrice);
        num(data, "finalizePrice", page_.finalizePrice);
        data.SetMember("canFinalize", RE::GFxValue(page_.canFinalize));
        data.SetMember("hasBack", RE::GFxValue(static_cast<bool>(page_.back)));
        num(data, "revision", static_cast<double>(revision_));
        num(data, "gold", page_.gold);
        num(data, "carry", page_.carry);
        num(data, "capacity", page_.capacity);
        for (std::size_t index = 0; index < page_.rows.size(); ++index) {
            const auto& row = page_.rows[index];
            RE::GFxValue value;
            movie->CreateObject(&value);
            num(value, "id", static_cast<double>(index));
            str(value, "key", row.key); txt(value, "name", row.name);
            txt(value, "category", row.category); txt(value, "group", row.group);
            txt(value, "typeValue", row.typeValue); txt(value, "effectValue", row.effectValue);
            txt(value, "currentQuality", row.currentQuality);
            txt(value, "resultQuality", row.resultQuality);
            txt(value, "deliveryValue", row.deliveryValue);
            txt(value, "statusValue", row.statusValue); txt(value, "timeValue", row.timeValue);
            txt(value, "pickupValue", row.pickupValue);
            txt(value, "action", row.action);
            txt(value, "blocked", row.blockedReason);
            num(value, "price", row.price); num(value, "quantity", row.quantity);
            value.SetMember("confirm", RE::GFxValue(row.confirm));
            value.SetMember("costDefined", RE::GFxValue(row.costDefined));
            value.SetMember("enabled", RE::GFxValue(row.enabled));
            value.SetMember("picked", RE::GFxValue(row.picked));
            if (row.object) {
                num(value, "weight", row.weight);
                txt(value, "statLabel", row.statLabel);
                num(value, "stat", row.stat);
                if (row.targetHealth > 0) num(value, "resultStat", row.resultStat);
            }
            rows.PushBack(value);
        }
        data.SetMember("rows", rows);
        // Fixed menu texts, translated; the menu keeps English for missing ones.
        RE::GFxValue labels;
        movie->CreateObject(&labels);
        for (const auto english : AOS::Text::kInterfaceTexts) {
            const std::string source{ english };
            const auto translated = AOS::Text::Localize(source);
            if (translated != source) str(labels, source.c_str(), translated);
        }
        data.SetMember("labels", labels);
        if (!inputModeReady_) {
            const auto input = RE::BSInputDeviceManager::GetSingleton();
            SetInputMode(input && input->IsGamepadEnabled());
        }
        SKSE::log::debug("Service menu values serialized: key='{}'", page_.key);
        ready_ = true; // model requests are deferred until the next movie frame
        movie->Invoke("_root.setPage", nullptr, &data, 1);
        SKSE::log::info("Service menu page shown: key='{}', rows={}", page_.key, page_.rows.size());
    }
}
