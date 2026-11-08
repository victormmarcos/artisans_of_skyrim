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

#include "RE/Skyrim.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NPCSS
{
    // Price set by a patch for a named item ([[items.tiers]] price/count).
    struct ItemPrice
    {
        std::int32_t price{ 0 };
        std::int32_t count{ 0 };
    };

    class FormResolver
    {
    public:
        static FormResolver& GetSingleton();

        void Build(const Config& a_config);

        [[nodiscard]] RE::TESForm* Resolve(std::string_view a_spec) const;
        [[nodiscard]] const std::unordered_map<RE::FormID, SmithTier>& NpcTiers() const noexcept;
        [[nodiscard]] const std::unordered_map<RE::FormID, std::string>&
        CourierLocations() const noexcept;
        [[nodiscard]] RE::TESFaction* BlacksmithFaction() const noexcept;
        [[nodiscard]] RE::FormID EorlundFormID() const noexcept;
        [[nodiscard]] RE::FormID BaldorFormID() const noexcept;
        [[nodiscard]] RE::FormID GunmarFormID() const noexcept;
        [[nodiscard]] RE::FormID GloverFormID() const noexcept;
        [[nodiscard]] RE::TESQuest* EorlundProvingHonorQuest() const noexcept;
        [[nodiscard]] RE::TESQuest* EorlundMissingInActionQuest() const noexcept;
        [[nodiscard]] RE::TESQuest* GunmarRecruitmentQuest() const noexcept;
        [[nodiscard]] RE::TESQuest* BaldorQuest() const noexcept;
        [[nodiscard]] RE::TESQuest* WayOfVoiceQuest() const noexcept;
        [[nodiscard]] RE::TESQuest* SkyforgeUnlockQuest() const noexcept;
        [[nodiscard]] RE::TESQuest* AncientTechnologyQuest() const noexcept;

        // Named in a patch ([[items.tiers]] form or [items] allow): the mod's
        // default exclusions do not apply to it.
        [[nodiscard]] bool IsAllowed(RE::FormID a_formID) const;
        // Tier from [[items.tiers]] (item first, then material keyword).
        [[nodiscard]] std::optional<SmithTier> ItemTierFor(const RE::TESBoundObject* a_object) const;
        [[nodiscard]] std::optional<ItemPrice> ItemPriceFor(RE::FormID a_formID) const;
        // Items named by form in [[items.tiers]], in file order.
        [[nodiscard]] const std::vector<RE::FormID>& NamedItems() const noexcept;
        [[nodiscard]] bool IsBlocked(RE::FormID a_formID) const;
        [[nodiscard]] bool IsSpecial(RE::FormID a_formID) const;
        [[nodiscard]] bool IsNordHero(RE::FormID a_formID) const;
        [[nodiscard]] bool IsAncientNordArmor(RE::FormID a_formID) const;
        [[nodiscard]] bool IsAncientNordWeapon(const RE::TESBoundObject* a_object) const;
        [[nodiscard]] std::string MenuLabelFor(const RE::TESBoundObject* a_object) const;
        [[nodiscard]] bool PreserveFullMenuLabel(const RE::TESBoundObject* a_object) const;
        [[nodiscard]] std::optional<std::string_view> MaterialLabelFor(
            const RE::TESBoundObject* a_object) const;

        [[nodiscard]] static std::string Lower(std::string_view a_value);
        [[nodiscard]] static std::vector<std::string> KeywordEditorIDs(const RE::TESBoundObject* a_object);
        [[nodiscard]] static bool HasKeywordToken(
            const RE::TESBoundObject* a_object,
            std::span<const std::string> a_tokens);
        [[nodiscard]] static bool HasExactKeyword(
            const RE::TESBoundObject* a_object,
            std::span<const std::string> a_editorIDs);
        [[nodiscard]] static bool HasTextToken(
            const RE::TESBoundObject* a_object,
            std::span<const std::string> a_tokens);
        [[nodiscard]] static bool IsStalhrim(const RE::TESBoundObject* a_object);

    private:
        std::unordered_map<RE::FormID, SmithTier> npcTiers_;
        std::unordered_map<RE::FormID, std::string> courierLocations_;
        std::unordered_set<RE::FormID> allowedItems_;
        std::unordered_map<RE::FormID, SmithTier> itemTiers_;
        std::unordered_map<RE::FormID, ItemPrice> itemPrices_;
        std::vector<RE::FormID> namedItems_;
        std::vector<std::pair<std::string, SmithTier>> keywordTiers_;
        std::unordered_set<RE::FormID> blockedItems_;
        std::unordered_set<RE::FormID> defaultBlockedItems_;
        std::unordered_set<RE::FormID> specialItems_;
        std::unordered_set<RE::FormID> nordHeroWeapons_;
        std::unordered_set<RE::FormID> ancientNordArmor_;
        std::vector<std::string> ancientNordWeaponKeywords_;
        std::unordered_map<RE::FormID, std::string> menuLabelOverrides_;
        std::unordered_set<RE::FormID> preserveFullMenuLabels_;
        std::unordered_map<RE::FormID, std::string> materialLabelOverrides_;
        RE::TESFaction* blacksmithFaction_{ nullptr };
        RE::FormID eorlundFormID_{ 0 };
        RE::FormID baldorFormID_{ 0 };
        RE::FormID gunmarFormID_{ 0 };
        RE::FormID gloverFormID_{ 0 };
        RE::TESQuest* eorlundProvingHonorQuest_{ nullptr };
        RE::TESQuest* eorlundMissingInActionQuest_{ nullptr };
        RE::TESQuest* gunmarRecruitmentQuest_{ nullptr };
        RE::TESQuest* baldorQuest_{ nullptr };
        RE::TESQuest* wayOfVoiceQuest_{ nullptr };
        RE::TESQuest* skyforgeUnlockQuest_{ nullptr };
        RE::TESQuest* ancientTechnologyQuest_{ nullptr };
    };
}
