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

#include "NPCSmithServices/Config.h"
#include "NPCSmithServices/InventoryService.h"
#include "NPCSmithServices/OrderDomain.h"
#include "NPCSmithServices/RecipeIndex.h"
#include "NPCSmithServices/SmithClassifier.h"

#include "RE/Skyrim.h"

#include <optional>
#include <string>
#include <vector>

namespace NPCSS
{
    struct CraftingOrderRecord
    {
        std::uint32_t schemaVersion{ 3 };
        std::uint64_t uniqueOrderID{ 0 };
        RE::FormID smithFormID{ 0 };
        RE::FormID smithBaseFormID{ 0 };
        RE::FormID workshopKey{ 0 };
        RE::FormID itemFormID{ 0 };
        std::string itemPlugin;
        RE::FormID itemLocalFormID{ 0 };
        std::int32_t quantity{ 1 };
        CraftingOrderCategory category{ CraftingOrderCategory::kDagger };
        std::string displayName;
        std::int32_t price{ 0 };
        double placedGameHour{ 0.0 };
        double startGameHour{ 0.0 };
        double readyGameHour{ 0.0 };
        RE::FormID pickupCellFormID{ 0 };
        RE::FormID pickupWorldspaceFormID{ 0 };
        float pickupX{ 0.0F };
        float pickupY{ 0.0F };
        float pickupZ{ 0.0F };
        CraftingOrderStatus status{ CraftingOrderStatus::kPending };
        std::uint32_t courierGeneration{ 0 };
        bool courierNotified{ false };
        bool lossNotified{ false };
        std::string smithDisplayName;
        std::string pickupLocation;
    };

    struct RuntimeOrderPreview
    {
        bool valid{ false };
        std::string error;
        CraftingOrderCategory category{ CraftingOrderCategory::kDagger };
        double durationHours{ 0.0 };
        OrderSchedulePreview schedule;
    };

    [[nodiscard]] std::optional<CraftingOrderCategory>
    ClassifyCraftingOrderCategory(RE::TESBoundObject* a_object);
    [[nodiscard]] ServiceComplexity ClassifyServiceComplexity(
        RE::TESBoundObject* a_object);
    [[nodiscard]] double CraftingDurationHours(
        CraftingOrderCategory a_category,
        const SmithingOrderDurations& a_durations) noexcept;

    class OrderService final :
        public RE::BSTEventSink<RE::TESWaitStopEvent>,
        public RE::BSTEventSink<RE::TESSleepStopEvent>,
        public RE::BSTEventSink<RE::TESFastTravelEndEvent>,
        public RE::BSTEventSink<RE::TESDeathEvent>
    {
    public:
        static OrderService& GetSingleton();

        void Configure(const Config& a_config);
        void RegisterSerialization();
        void RegisterEvents();
        void Update(bool a_dispatchCouriers = true);

        [[nodiscard]] RuntimeOrderPreview Preview(
            const SmithContext& a_smith,
            const CraftableItem& a_item) const;
        [[nodiscard]] TransactionResult PlaceOrder(
            const SmithContext& a_smith,
            const CraftableItem& a_item);
        [[nodiscard]] bool HasActiveOrdersForSmith(
            const RE::Actor* a_smith) const;
        [[nodiscard]] std::string CollectOrders(RE::Actor* a_smith);
        [[nodiscard]] const std::vector<CraftingOrderRecord>& Orders() const noexcept
        {
            return orders_;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESWaitStopEvent*,
            RE::BSTEventSource<RE::TESWaitStopEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESSleepStopEvent*,
            RE::BSTEventSource<RE::TESSleepStopEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESFastTravelEndEvent*,
            RE::BSTEventSource<RE::TESFastTravelEndEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESDeathEvent*,
            RE::BSTEventSource<RE::TESDeathEvent>*) override;

    private:
        [[nodiscard]] static double CurrentGameHour();
        [[nodiscard]] std::vector<OrderTimelineEntry> Timeline() const;
        void DispatchCourierNotices();

        static void Save(SKSE::SerializationInterface* a_interface);
        static void Load(SKSE::SerializationInterface* a_interface);
        static void Revert(SKSE::SerializationInterface* a_interface);
        void SaveImpl(SKSE::SerializationInterface* a_interface) const;
        void LoadImpl(SKSE::SerializationInterface* a_interface);
        void RevertImpl();

        const Config* config_{ nullptr };
        bool serializationRegistered_{ false };
        bool eventsRegistered_{ false };
        std::uint64_t nextOrderID_{ 1 };
        std::vector<CraftingOrderRecord> orders_;
    };
}
