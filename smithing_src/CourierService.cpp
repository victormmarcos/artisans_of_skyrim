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

#include "NPCSmithServices/CourierService.h"

#include <MinHook.h>

#include <cstring>
#include <limits>

namespace
{
    constexpr std::string_view kPluginName = "ArtisansOfSkyrim.esp";
    constexpr RE::FormID kFirstBookLocalFormID = 0x0B00;
    constexpr std::uint16_t kBookCount = 1024;
    constexpr RE::FormID kCourierQuestFormID = 0x00039F82;
    constexpr RE::FormID kCourierContainerFormID = 0x00039FB9;
    constexpr RE::FormID kCourierItemCountFormID = 0x00039FBB;
    constexpr std::string_view kGenericTitle = "Smithing Order Notice";
    constexpr std::uint32_t kMaximumSerializedString = 16384;

    [[nodiscard]] bool WriteString(
        SKSE::SerializationInterface* a_interface,
        const std::string& a_value)
    {
        const auto length = static_cast<std::uint32_t>(a_value.size());
        return a_interface->WriteRecordData(length) &&
            (length == 0 ||
             a_interface->WriteRecordData(a_value.data(), length));
    }

    template <class T>
    [[nodiscard]] bool ReadValue(
        SKSE::SerializationInterface* a_interface,
        T& a_value)
    {
        return a_interface->ReadRecordData(a_value) == sizeof(T);
    }

    [[nodiscard]] bool ReadString(
        SKSE::SerializationInterface* a_interface,
        std::string& a_value)
    {
        std::uint32_t length = 0;
        if (!ReadValue(a_interface, length) ||
            length > kMaximumSerializedString) {
            return false;
        }
        a_value.resize(length);
        return length == 0 ||
            a_interface->ReadRecordData(a_value.data(), length) == length;
    }

    void RemoveLegacySignaturePrefix(std::string& a_body)
    {
        constexpr std::size_t prefixSize = 4;
        for (std::size_t index = 1; index + prefixSize <= a_body.size();) {
            const auto first = static_cast<std::uint8_t>(a_body[index]);
            const auto second = static_cast<std::uint8_t>(a_body[index + 1]);
            const auto third = static_cast<std::uint8_t>(a_body[index + 2]);
            if (a_body[index - 1] == '\n' &&
                first == 0xE2 && second == 0x80 && third == 0x94 &&
                a_body[index + 3] == ' ') {
                a_body.erase(index, prefixSize);
                continue;
            }
            ++index;
        }
    }
}

namespace NPCSS
{
    CourierService& CourierService::GetSingleton()
    {
        static CourierService singleton;
        return singleton;
    }

    void CourierService::InstallBookCallHooks()
    {
        std::scoped_lock lock{ mutex_ };
        if (bookHookInstalled_) {
            return;
        }

        REL::Relocation<std::uintptr_t> target{
            RELOCATION_ID(50122, 51053)
        };

        const auto initializeStatus = MH_Initialize();
        if (initializeStatus != MH_OK &&
            initializeStatus != MH_ERROR_ALREADY_INITIALIZED) {
            SKSE::log::critical(
                "Could not initialize the courier book detour: {}.",
                MH_StatusToString(initializeStatus));
            return;
        }
        const auto address = reinterpret_cast<void*>(target.address());
        void* original = nullptr;
        const auto createStatus = MH_CreateHook(
            address,
            reinterpret_cast<void*>(OpenBookMenuHook),
            std::addressof(original));
        if (createStatus != MH_OK) {
            SKSE::log::critical(
                "Could not create the courier book detour: {}. "
                "Courier letters will retain their fallback text.",
                MH_StatusToString(createStatus));
            return;
        }
        originalOpenBookMenu_ = reinterpret_cast<OpenBookMenu>(original);
        const auto enableStatus = MH_EnableHook(address);
        if (enableStatus != MH_OK) {
            MH_RemoveHook(address);
            originalOpenBookMenu_ = nullptr;
            SKSE::log::critical(
                "Could not enable the courier book detour: {}.",
                MH_StatusToString(enableStatus));
            return;
        }
        bookHookInstalled_ = true;
        SKSE::log::info(
            "Installed the courier-letter BookMenu entry detour.");
    }

    void CourierService::Initialize()
    {
        std::scoped_lock lock{ mutex_ };
        courierQuest_ = RE::TESForm::LookupByID<RE::TESQuest>(
            kCourierQuestFormID);
        courierContainer_ = RE::TESForm::LookupByID<RE::TESObjectREFR>(
            kCourierContainerFormID);
        courierItemCount_ = RE::TESForm::LookupByID<RE::TESGlobal>(
            kCourierItemCountFormID);
        books_.clear();
        books_.reserve(kBookCount);
        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        for (std::uint16_t index = 0; index < kBookCount; ++index) {
            books_.push_back(
                dataHandler ?
                    dataHandler->LookupForm<RE::TESObjectBOOK>(
                        kFirstBookLocalFormID + index,
                        kPluginName) :
                    nullptr);
        }
        const auto resolved = std::ranges::count_if(
            books_,
            [](const RE::TESObjectBOOK* a_book) { return a_book != nullptr; });
        if (!courierQuest_) {
            SKSE::log::error("Vanilla WICourier quest 00039F82 was not found.");
        }
        if (!courierContainer_) {
            SKSE::log::error(
                "Vanilla WICourier container 00039FB9 was not found.");
        }
        if (!courierItemCount_) {
            SKSE::log::error(
                "Vanilla WICourier item-count global 00039FBB was not found.");
        }
        if (resolved != kBookCount) {
            SKSE::log::error(
                "Resolved {}/{} smithing courier-letter forms.",
                resolved,
                kBookCount);
        } else {
            SKSE::log::info(
                "Smithing courier service initialized with {} unique letters.",
                kBookCount);
        }
        ReconcileVanillaCourierInventory();
        RestoreNoticeTitles();
    }

    std::optional<std::uint32_t> CourierService::QueueNotice(
        std::string a_title,
        std::string a_body)
    {
        std::scoped_lock lock{ mutex_ };
        RemoveLegacySignaturePrefix(a_body);
        if (nextBookIndex_ >= kBookCount) {
            SKSE::log::error(
                "Smithing courier-letter pool is exhausted; collection remains available.");
            return std::nullopt;
        }
        auto* book = BookAt(nextBookIndex_);
        if (!book || !courierQuest_) {
            SKSE::log::error(
                "Smithing courier notice could not resolve its book or courier quest.");
            return std::nullopt;
        }

        Notice notice;
        notice.noticeID = nextNoticeID_;
        notice.bookIndex = nextBookIndex_;
        notice.title = std::move(a_title);
        notice.body = std::move(a_body);
        book->fullName = notice.title.c_str();
        noticesByFormID_.insert_or_assign(book->GetFormID(), notice);

        if (!DispatchWithVanillaCourier(book)) {
            noticesByFormID_.erase(book->GetFormID());
            book->fullName = kGenericTitle.data();
            SKSE::log::error(
                "Vanilla courier rejected smithing notice {}.",
                notice.noticeID);
            return std::nullopt;
        }

        ++nextNoticeID_;
        ++nextBookIndex_;
        SKSE::log::info(
            "Queued smithing notice {} as book {:08X} through WICourier.",
            notice.noticeID,
            book->GetFormID());
        return notice.noticeID;
    }

    bool CourierService::DispatchWithVanillaCourier(
        RE::TESObjectBOOK* a_book) const
    {
        if (!courierQuest_ || !courierContainer_ ||
            !courierItemCount_ || !a_book) {
            return false;
        }

        const auto before = courierContainer_->GetInventoryCount();
        courierContainer_->AddObjectToContainer(
            a_book,
            nullptr,
            1,
            nullptr);
        const auto after = courierContainer_->GetInventoryCount();
        if (after < before + 1) {
            SKSE::log::error(
                "WICourier container rejected smithing book {:08X}.",
                a_book->GetFormID());
            return false;
        }

        // This is the same invariant maintained by WICourierScript:
        // the global mirrors the number of objects waiting in its container.
        // Updating both synchronously avoids treating an accepted VM dispatch
        // as proof that the Papyrus method actually ran.
        courierItemCount_->value = static_cast<float>(after);
        SKSE::log::info(
            "Added smithing book {:08X} directly to the WICourier container; "
            "vanilla pending-item count is {}.",
            a_book->GetFormID(),
            after);
        return true;
    }

    void CourierService::ReconcileVanillaCourierInventory() const
    {
        if (!courierContainer_ || !courierItemCount_) {
            return;
        }
        const auto actual = std::max(
            courierContainer_->GetInventoryCount(),
            std::int32_t{ 0 });
        const auto recorded = static_cast<std::int32_t>(
            std::max(courierItemCount_->value, 0.0F));
        if (recorded == actual) {
            return;
        }
        courierItemCount_->value = static_cast<float>(actual);
        SKSE::log::warn(
            "Reconciled WICourier pending-item count from {} to {}.",
            recorded,
            actual);
    }

    std::optional<std::string> CourierService::DescriptionFor(
        const RE::TESObjectBOOK* a_book) const
    {
        std::scoped_lock lock{ mutex_ };
        if (!a_book) {
            return std::nullopt;
        }
        const auto found = noticesByFormID_.find(a_book->GetFormID());
        return found != noticesByFormID_.end() ?
            std::optional<std::string>{ found->second.body } :
            std::nullopt;
    }

    void CourierService::OpenBookMenuHook(
        const RE::BSString& a_description,
        const RE::ExtraDataList* a_extraList,
        RE::TESObjectREFR* a_ref,
        RE::TESObjectBOOK* a_book,
        const RE::NiPoint3& a_pos,
        const RE::NiMatrix3& a_rot,
        float a_scale,
        bool a_useDefaultPos)
    {
        const auto custom = GetSingleton().DescriptionFor(a_book);
        if (custom) {
            const RE::BSString description{ *custom };
            originalOpenBookMenu_(
                description,
                a_extraList,
                a_ref,
                a_book,
                a_pos,
                a_rot,
                a_scale,
                a_useDefaultPos);
            return;
        }
        originalOpenBookMenu_(
            a_description,
            a_extraList,
            a_ref,
            a_book,
            a_pos,
            a_rot,
            a_scale,
            a_useDefaultPos);
    }

    bool CourierService::SaveRecord(
        SKSE::SerializationInterface* a_interface) const
    {
        std::scoped_lock lock{ mutex_ };
        if (!a_interface ||
            !a_interface->OpenRecord(RecordType, RecordVersion)) {
            return false;
        }
        const auto count = static_cast<std::uint32_t>(
            noticesByFormID_.size());
        if (!a_interface->WriteRecordData(nextNoticeID_) ||
            !a_interface->WriteRecordData(nextBookIndex_) ||
            !a_interface->WriteRecordData(count)) {
            return false;
        }
        for (const auto& [formID, notice] : noticesByFormID_) {
            (void)formID;
            if (!a_interface->WriteRecordData(notice.noticeID) ||
                !a_interface->WriteRecordData(notice.bookIndex) ||
                !WriteString(a_interface, notice.title) ||
                !WriteString(a_interface, notice.body)) {
                return false;
            }
        }
        return true;
    }

    bool CourierService::LoadRecord(
        SKSE::SerializationInterface* a_interface,
        std::uint32_t a_version)
    {
        std::scoped_lock lock{ mutex_ };
        if (!a_interface || a_version != RecordVersion) {
            return false;
        }
        std::uint32_t count = 0;
        if (!ReadValue(a_interface, nextNoticeID_) ||
            !ReadValue(a_interface, nextBookIndex_) ||
            !ReadValue(a_interface, count) ||
            nextBookIndex_ > kBookCount || count > kBookCount) {
            return false;
        }
        for (std::uint32_t index = 0; index < count; ++index) {
            Notice notice;
            if (!ReadValue(a_interface, notice.noticeID) ||
                !ReadValue(a_interface, notice.bookIndex) ||
                notice.bookIndex >= kBookCount ||
                !ReadString(a_interface, notice.title) ||
                !ReadString(a_interface, notice.body)) {
                return false;
            }
            RemoveLegacySignaturePrefix(notice.body);
            if (auto* book = BookAt(notice.bookIndex)) {
                noticesByFormID_.insert_or_assign(
                    book->GetFormID(),
                    std::move(notice));
            } else {
                SKSE::log::error(
                    "Saved smithing courier letter {} could not resolve its BOOK form.",
                    notice.noticeID);
                return false;
            }
        }
        RestoreNoticeTitles();
        return true;
    }

    void CourierService::Revert()
    {
        std::scoped_lock lock{ mutex_ };
        for (const auto& [formID, notice] : noticesByFormID_) {
            (void)formID;
            if (auto* book = BookAt(notice.bookIndex)) {
                book->fullName = kGenericTitle.data();
            }
        }
        noticesByFormID_.clear();
        nextNoticeID_ = 1;
        nextBookIndex_ = 0;
    }

    void CourierService::RestoreNoticeTitles()
    {
        for (const auto& [formID, notice] : noticesByFormID_) {
            (void)formID;
            if (auto* book = BookAt(notice.bookIndex)) {
                book->fullName = notice.title.c_str();
            }
        }
    }

    RE::TESObjectBOOK* CourierService::BookAt(
        std::uint16_t a_index) const
    {
        return a_index < books_.size() ? books_[a_index] : nullptr;
    }
}
