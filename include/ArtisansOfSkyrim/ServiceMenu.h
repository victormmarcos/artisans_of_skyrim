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
#include <functional>
#include <string>
#include <vector>

namespace AOS
{
    struct ServiceRow
    {
        std::string key;
        std::string name;
        std::string category;
        std::string group;
        // Structured presentation fields consumed directly by the movie.
        std::string typeValue;
        std::string effectValue;
        std::string currentQuality;
        std::string resultQuality;
        std::string deliveryValue;
        std::string statusValue;
        std::string timeValue;
        std::string pickupValue;
        std::string action{ "Select" };
        RE::TESBoundObject* object{ nullptr };
        RE::ExtraDataList* extra{ nullptr };
        std::int32_t price{ 0 };
        std::int32_t quantity{ 1 };
        float currentHealth{ 1 };
        float targetHealth{ 0 };
        // Captured on the game task queue; Publish only serializes these values.
        std::string statLabel;
        float weight{ 0 };
        float stat{ 0 };
        float resultStat{ 0 };
        bool confirm{ false };
        bool costDefined{ false };
        bool enabled{ true };
        bool picked{ false };
        std::string blockedReason;
        std::function<void()> activate;
    };

    struct ServicePage
    {
        std::string key;
        std::string artisan;
        std::string tier;
        std::string profession;
        // UI profile used by the shared Scaleform movie. Gameplay remains in
        // the service modules; only presentation/filter semantics vary.
        std::string layout{ "generic" };
        std::string categoryLabel{ "Type" };
        std::string groupLabel{ "Group" };
        bool showCategoryFilter{ true };
        bool showGroupFilter{ true };
        std::string notice;
        std::string alternateLabel;
        std::function<void()> alternate;
        // Optional page-level checkout used by multi-selection services.
        // Row activation only changes the selection; finalize performs the
        // transaction after the movie has shown its itemized confirmation.
        std::string finalizeLabel;
        std::string finalizeItem;
        std::string selectionOneName;
        std::string selectionOneEffect;
        std::int32_t selectionOnePrice{ 0 };
        std::string selectionTwoName;
        std::string selectionTwoEffect;
        std::int32_t selectionTwoPrice{ 0 };
        std::int32_t finalizePrice{ 0 };
        bool canFinalize{ false };
        std::int32_t gold{ 0 };
        float carry{ 0 };
        float capacity{ 0 };
        std::function<void(std::string)> finalize;
        std::function<void()> back;
        std::vector<ServiceRow> rows;
    };

    // One isolated SWF for all services. Gameplay rules stay in the service modules.
    class ServiceMenu final : public RE::IMenu
    {
    public:
        inline static constexpr std::string_view MENU_NAME = "ArtisansServiceMenu";
        static void Register();
        static void Present(ServicePage page);
        static void Close();
        static void Status(std::string message);
        static bool IsOpen();
        static RE::IMenu* Create();
        ServiceMenu();
        ~ServiceMenu() override;
        RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& message) override;
        void AdvanceMovie(float interval, std::uint32_t time) override;
        void PostDisplay() override;
        void SetPage(ServicePage page);
        void Command(std::string command, std::size_t index,
            std::uint64_t revision, std::string value = {});
        void Navigate(std::string_view command);
        void SetInputMode(bool gamepad);
        [[nodiscard]] bool IsTextInputActive() const noexcept { return textInput_; }

    private:
        void Publish();
        void Preview(std::size_t index);
        void ReleasePreview();
        void ClearPreviewItem();
        void UpdatePreviewModel();
        void PlacePreview();
        void SetMouseRotation(bool a_active);
        ServicePage page_;
        std::uint64_t revision_{ 0 };
        bool ready_{ false };
        bool textInput_{ false };
        bool previewActive_{ false };
        bool gamepadInput_{ false };
        bool inputModeReady_{ false };
        bool previousCursorVisible_{ false };
        bool cursorCaptured_{ false };
        bool closing_{ false };
        bool commandPending_{ false };
        std::size_t previewIndex_{ static_cast<std::size_t>(-1) };
        // 3D preview placement and rotation. The target is the centre of the
        // menu's preview area as a screen fraction (0,0 = top left).
        bool mouseRotation_{ false };
        bool previewTargetSet_{ false };
        float previewTargetX_{ 0.0F };
        float previewTargetY_{ 0.0F };
        float frameSeconds_{ 1.0F / 60.0F };
        bool itemPosSaved_{ false };
        bool savedUserInput_{ false };
        bool handlerAdded_{ false };
        bool placementLogged_{ false };
        RE::NiPoint3 savedItemPos_{};
    };
}
