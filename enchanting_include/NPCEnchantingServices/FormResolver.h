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

#include "NPCEnchantingServices/Config.h"

#include "RE/Skyrim.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NPCES
{
    class FormResolver
    {
    public:
        static FormResolver& GetSingleton();

        void Build(const Config& a_config);

        [[nodiscard]] RE::TESForm* Resolve(std::string_view a_spec) const;
        [[nodiscard]] RE::FormID FarengarFormID() const noexcept;
        [[nodiscard]] RE::FormID CalcelmoFormID() const noexcept;
        [[nodiscard]] RE::FormID SergiusFormID() const noexcept;
        [[nodiscard]] RE::FormID SybilleFormID() const noexcept;
        [[nodiscard]] RE::FormID NelothFormID() const noexcept;
        [[nodiscard]] RE::FormID FeranSadriFormID() const noexcept;
        [[nodiscard]] RE::TESFaction* CollegeFaction() const noexcept;
        [[nodiscard]] RE::TESFaction* VolkiharFaction() const noexcept;
        [[nodiscard]] RE::TESFaction* DawnguardFaction() const noexcept;
        [[nodiscard]] RE::TESQuest* PotemaFirstQuest() const noexcept;
        [[nodiscard]] RE::TESQuest* NelothDwemerQuest() const noexcept;
        [[nodiscard]] bool IsBlockedItem(RE::FormID a_formID) const;

        [[nodiscard]] static std::string Lower(std::string_view a_value);
        [[nodiscard]] static std::vector<std::string> KeywordEditorIDs(
            const RE::TESBoundObject* a_object);
        [[nodiscard]] static bool HasKeywordToken(
            const RE::TESBoundObject* a_object,
            std::span<const std::string> a_tokens);
        [[nodiscard]] static bool HasTextToken(
            const RE::TESBoundObject* a_object,
            std::span<const std::string> a_tokens);

    private:
        RE::FormID farengarFormID_{ 0 };
        RE::FormID calcelmoFormID_{ 0 };
        RE::FormID sergiusFormID_{ 0 };
        RE::FormID sybilleFormID_{ 0 };
        RE::FormID nelothFormID_{ 0 };
        RE::FormID feranSadriFormID_{ 0 };
        RE::TESFaction* collegeFaction_{ nullptr };
        RE::TESFaction* volkiharFaction_{ nullptr };
        RE::TESFaction* dawnguardFaction_{ nullptr };
        RE::TESQuest* potemaFirstQuest_{ nullptr };
        RE::TESQuest* nelothDwemerQuest_{ nullptr };
        std::unordered_set<RE::FormID> blockedItems_;
    };
}
