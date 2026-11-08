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

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NPCSS
{
    class CourierService
    {
    public:
        inline static constexpr std::uint32_t RecordType = 'CNOT';
        inline static constexpr std::uint32_t RecordVersion = 1;

        static CourierService& GetSingleton();

        void InstallBookCallHooks();
        void Initialize();
        [[nodiscard]] std::optional<std::uint32_t> QueueNotice(
            std::string a_title,
            std::string a_body);

        [[nodiscard]] bool SaveRecord(
            SKSE::SerializationInterface* a_interface) const;
        [[nodiscard]] bool LoadRecord(
            SKSE::SerializationInterface* a_interface,
            std::uint32_t a_version);
        void Revert();

    private:
        struct Notice
        {
            std::uint32_t noticeID{ 0 };
            std::uint16_t bookIndex{ 0 };
            std::string title;
            std::string body;
        };

        using OpenBookMenu = decltype(&RE::BookMenu::OpenBookMenu);

        [[nodiscard]] bool DispatchWithVanillaCourier(
            RE::TESObjectBOOK* a_book) const;
        void ReconcileVanillaCourierInventory() const;
        [[nodiscard]] std::optional<std::string> DescriptionFor(
            const RE::TESObjectBOOK* a_book) const;
        void RestoreNoticeTitles();
        [[nodiscard]] RE::TESObjectBOOK* BookAt(
            std::uint16_t a_index) const;

        static void OpenBookMenuHook(
            const RE::BSString& a_description,
            const RE::ExtraDataList* a_extraList,
            RE::TESObjectREFR* a_ref,
            RE::TESObjectBOOK* a_book,
            const RE::NiPoint3& a_pos,
            const RE::NiMatrix3& a_rot,
            float a_scale,
            bool a_useDefaultPos);

        static inline OpenBookMenu originalOpenBookMenu_{ nullptr };

        mutable std::recursive_mutex mutex_;
        RE::TESQuest* courierQuest_{ nullptr };
        RE::TESObjectREFR* courierContainer_{ nullptr };
        RE::TESGlobal* courierItemCount_{ nullptr };
        std::vector<RE::TESObjectBOOK*> books_;
        std::unordered_map<RE::FormID, Notice> noticesByFormID_;
        std::uint32_t nextNoticeID_{ 1 };
        std::uint16_t nextBookIndex_{ 0 };
        bool bookHookInstalled_{ false };
    };
}
