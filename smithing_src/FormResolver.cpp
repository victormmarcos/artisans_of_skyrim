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

#include "NPCSmithServices/FormResolver.h"

#include <charconv>

namespace
{
    [[nodiscard]] const RE::BGSKeywordForm* GetKeywordForm(const RE::TESBoundObject* a_object)
    {
        if (!a_object) {
            return nullptr;
        }
        if (const auto weapon = a_object->As<RE::TESObjectWEAP>()) {
            return static_cast<const RE::BGSKeywordForm*>(weapon);
        }
        if (const auto armor = a_object->As<RE::TESObjectARMO>()) {
            return static_cast<const RE::BGSKeywordForm*>(armor);
        }
#ifndef ENABLE_SKYRIM_VR
        if (const auto ammo = a_object->As<RE::TESAmmo>()) {
            return static_cast<const RE::BGSKeywordForm*>(ammo);
        }
#endif
        return nullptr;
    }
}

namespace NPCSS
{
    FormResolver& FormResolver::GetSingleton()
    {
        static FormResolver singleton;
        return singleton;
    }

    void FormResolver::Build(const Config& a_config)
    {
        npcTiers_.clear();
        courierLocations_.clear();
        allowedItems_.clear();
        blockedItems_.clear();
        defaultBlockedItems_.clear();
        specialItems_.clear();
        nordHeroWeapons_.clear();
        ancientNordArmor_.clear();
        ancientNordWeaponKeywords_ = a_config.ancientNordWeaponKeywords;
        menuLabelOverrides_.clear();
        preserveFullMenuLabels_.clear();
        materialLabelOverrides_.clear();
        if (const auto form = Resolve(a_config.blacksmithFactionForm)) {
            blacksmithFaction_ = form->As<RE::TESFaction>();
        } else {
            blacksmithFaction_ = nullptr;
        }
        eorlundProvingHonorQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.eorlundProvingHonorQuestEditorID);
        eorlundMissingInActionQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.eorlundMissingInActionQuestEditorID);
        gunmarRecruitmentQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.gunmarRecruitmentQuestEditorID);
        baldorQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.baldorQuestEditorID);
        wayOfVoiceQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.wayOfVoiceQuestEditorID);
        skyforgeUnlockQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.skyforgeUnlockQuestEditorID);
        ancientTechnologyQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.ancientTechnologyQuestEditorID);

        if (const auto form = Resolve(a_config.eorlundForm)) {
            eorlundFormID_ = form->GetFormID();
        } else {
            eorlundFormID_ = 0;
            SKSE::log::warn("Could not resolve Eorlund form '{}'", a_config.eorlundForm);
        }

        if (const auto form = Resolve(a_config.baldorForm)) {
            baldorFormID_ = form->GetFormID();
        } else {
            baldorFormID_ = 0;
            SKSE::log::warn("Could not resolve Baldor form '{}'", a_config.baldorForm);
        }

        if (const auto form = Resolve(a_config.gunmarForm)) {
            gunmarFormID_ = form->GetFormID();
        } else {
            gunmarFormID_ = 0;
            SKSE::log::warn("Could not resolve Gunmar form '{}'", a_config.gunmarForm);
        }

        if (const auto form = Resolve(a_config.gloverForm)) {
            gloverFormID_ = form->GetFormID();
        } else {
            gloverFormID_ = 0;
            SKSE::log::warn("Could not resolve Glover Mallory form '{}'", a_config.gloverForm);
        }

        for (const auto& entry : a_config.npcOverrides) {
            if (const auto form = Resolve(entry.form)) {
                // First entry wins: Artisans' own artisans come before patches,
                // so their rank and exclusives can't be changed by a patch.
                if (npcTiers_.contains(form->GetFormID())) {
                    SKSE::log::warn(
                        "NPC '{}' ({:08X}) is one of Artisans' own artisans or already set; this entry is ignored.",
                        entry.name,
                        form->GetFormID());
                    continue;
                }
                npcTiers_.insert_or_assign(form->GetFormID(), entry.tier);
                if (!entry.courierLocation.empty()) {
                    courierLocations_.insert_or_assign(
                        form->GetFormID(),
                        entry.courierLocation);
                }
                SKSE::log::info(
                    "NPC tier: {} ({:08X}) -> {}",
                    entry.name,
                    form->GetFormID(),
                    ToString(entry.tier));
            } else {
                SKSE::log::warn("Could not resolve NPC override '{}' ({})", entry.name, entry.form);
            }
        }
        const auto resolveItems = [this](const std::vector<std::string>& a_specs, auto& a_destination) {
            for (const auto& spec : a_specs) {
                if (const auto form = Resolve(spec)) {
                    a_destination.insert(form->GetFormID());
                } else {
                    SKSE::log::warn("Could not resolve item form '{}'", spec);
                }
            }
        };
        resolveItems(a_config.allowItems, allowedItems_);
        itemTiers_.clear();
        itemPrices_.clear();
        namedItems_.clear();
        keywordTiers_.clear();
        for (const auto& entry : a_config.itemTiers) {
            if (!entry.keyword.empty()) {
                keywordTiers_.emplace_back(entry.keyword, entry.tier);
                SKSE::log::info("Item tier: keyword {} -> {}", entry.keyword, ToString(entry.tier));
            }
            if (entry.form.empty()) continue;
            if (const auto form = Resolve(entry.form)) {
                // Naming an item is also an explicit "allow" for it.
                itemTiers_.insert_or_assign(form->GetFormID(), entry.tier);
                allowedItems_.insert(form->GetFormID());
                namedItems_.push_back(form->GetFormID());
                if (entry.price > 0) {
                    itemPrices_.insert_or_assign(form->GetFormID(), ItemPrice{ entry.price, entry.count });
                }
                SKSE::log::info("Item tier: {} ({:08X}) -> {}", entry.form, form->GetFormID(), ToString(entry.tier));
            } else {
                SKSE::log::warn("Could not resolve item tier form '{}'", entry.form);
            }
        }
        resolveItems(a_config.blockItems, blockedItems_);
        resolveItems(a_config.defaultBlockItems, defaultBlockedItems_);
        // Named items that come enchanted are offered with their enchantment;
        // say so, since the mod never offers them on its own.
        for (const auto formID : allowedItems_) {
            const auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(formID);
            const auto* enchantable = object ? object->As<RE::TESEnchantableForm>() : nullptr;
            if (enchantable && enchantable->formEnchanting) {
                SKSE::log::info(
                    "Item {:08X} '{}' is enchanted by default; because a patch names it, "
                    "blacksmiths offer it with its enchantment.",
                    formID,
                    object->GetName());
            }
            if (blockedItems_.contains(formID)) {
                SKSE::log::info("Item {:08X} is named in a patch but also blocked; the block wins.", formID);
            }
        }
        resolveItems(a_config.specialItems, specialItems_);
        resolveItems(a_config.nordHeroWeapons, nordHeroWeapons_);
        resolveItems(a_config.ancientNordArmor, ancientNordArmor_);

        for (const auto& entry : a_config.menuLabelOverrides) {
            if (const auto form = Resolve(entry.form)) {
                menuLabelOverrides_.insert_or_assign(form->GetFormID(), entry.label);
                if (entry.preserveFull) {
                    preserveFullMenuLabels_.insert(form->GetFormID());
                }
            } else {
                SKSE::log::warn("Could not resolve menu label override form '{}'", entry.form);
            }
        }
        for (const auto& entry : a_config.materialLabelOverrides) {
            if (const auto form = Resolve(entry.form)) {
                materialLabelOverrides_.insert_or_assign(form->GetFormID(), entry.label);
            } else {
                SKSE::log::warn("Could not resolve material label override form '{}'", entry.form);
            }
        }
        if (!blacksmithFaction_) {
            SKSE::log::warn(
                "Blacksmith faction '{}' was not found; only explicit NPC overrides will be valid",
                a_config.blacksmithFactionForm);
        }
        if (!eorlundProvingHonorQuest_) {
            SKSE::log::warn(
                "Eorlund Proving Honor quest '{}' was not found; this unlock path will stay locked",
                a_config.eorlundProvingHonorQuestEditorID);
        }
        if (!eorlundMissingInActionQuest_) {
            SKSE::log::warn(
                "Eorlund Missing in Action quest '{}' was not found; this unlock path will stay locked",
                a_config.eorlundMissingInActionQuestEditorID);
        }
        if (!gunmarRecruitmentQuest_) {
            SKSE::log::warn(
                "Gunmar recruitment quest '{}' was not found; Gunmar services will stay locked",
                a_config.gunmarRecruitmentQuestEditorID);
        }
        if (!baldorQuest_) {
            SKSE::log::warn(
                "Baldor quest '{}' was not found; quest-gated services will stay locked",
                a_config.baldorQuestEditorID);
        }
        if (!wayOfVoiceQuest_) {
            SKSE::log::warn(
                "The Way of the Voice quest '{}' was not found; Dragon gear can stay locked if the rule is enabled",
                a_config.wayOfVoiceQuestEditorID);
        }
        if (!skyforgeUnlockQuest_) {
            SKSE::log::warn(
                "Skyforge unlock quest '{}' was not found; Nord Hero and Ancient Nord gear will stay locked",
                a_config.skyforgeUnlockQuestEditorID);
        }
        if (!ancientTechnologyQuest_) {
            SKSE::log::warn(
                "Ancient Technology quest '{}' was not found; enhanced crossbow gear can stay locked if the rule is enabled",
                a_config.ancientTechnologyQuestEditorID);
        }
    }

    RE::TESForm* FormResolver::Resolve(std::string_view a_spec) const
    {
        const auto separator = a_spec.find('|');
        if (separator == std::string_view::npos) {
            return RE::TESForm::LookupByEditorID(a_spec);
        }

        const auto plugin = a_spec.substr(0, separator);
        auto idText = a_spec.substr(separator + 1);
        if (idText.starts_with("0x") || idText.starts_with("0X")) {
            idText.remove_prefix(2);
        }

        RE::FormID localID = 0;
        const auto [end, error] = std::from_chars(
            idText.data(),
            idText.data() + idText.size(),
            localID,
            16);
        if (error != std::errc{} || end != idText.data() + idText.size()) {
            return nullptr;
        }

        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler ? dataHandler->LookupForm(localID, plugin) : nullptr;
    }

    const std::unordered_map<RE::FormID, SmithTier>& FormResolver::NpcTiers() const noexcept
    {
        return npcTiers_;
    }

    const std::unordered_map<RE::FormID, std::string>&
    FormResolver::CourierLocations() const noexcept
    {
        return courierLocations_;
    }

    RE::TESFaction* FormResolver::BlacksmithFaction() const noexcept
    {
        return blacksmithFaction_;
    }

    RE::FormID FormResolver::EorlundFormID() const noexcept
    {
        return eorlundFormID_;
    }

    RE::FormID FormResolver::BaldorFormID() const noexcept
    {
        return baldorFormID_;
    }

    RE::FormID FormResolver::GunmarFormID() const noexcept
    {
        return gunmarFormID_;
    }

    RE::FormID FormResolver::GloverFormID() const noexcept
    {
        return gloverFormID_;
    }

    RE::TESQuest* FormResolver::EorlundProvingHonorQuest() const noexcept
    {
        return eorlundProvingHonorQuest_;
    }

    RE::TESQuest* FormResolver::EorlundMissingInActionQuest() const noexcept
    {
        return eorlundMissingInActionQuest_;
    }

    RE::TESQuest* FormResolver::GunmarRecruitmentQuest() const noexcept
    {
        return gunmarRecruitmentQuest_;
    }

    RE::TESQuest* FormResolver::BaldorQuest() const noexcept
    {
        return baldorQuest_;
    }

    RE::TESQuest* FormResolver::WayOfVoiceQuest() const noexcept
    {
        return wayOfVoiceQuest_;
    }

    RE::TESQuest* FormResolver::SkyforgeUnlockQuest() const noexcept
    {
        return skyforgeUnlockQuest_;
    }

    RE::TESQuest* FormResolver::AncientTechnologyQuest() const noexcept
    {
        return ancientTechnologyQuest_;
    }

    bool FormResolver::IsAllowed(RE::FormID a_formID) const
    {
        return allowedItems_.contains(a_formID);
    }

    std::optional<SmithTier> FormResolver::ItemTierFor(const RE::TESBoundObject* a_object) const
    {
        if (!a_object) return std::nullopt;
        if (const auto found = itemTiers_.find(a_object->GetFormID()); found != itemTiers_.end()) {
            return found->second;
        }
        for (const auto& [keyword, tier] : keywordTiers_) {
            const std::array<std::string, 1> editorIDs{ keyword };
            if (HasExactKeyword(a_object, editorIDs)) return tier;
        }
        return std::nullopt;
    }

    std::optional<ItemPrice> FormResolver::ItemPriceFor(RE::FormID a_formID) const
    {
        if (const auto found = itemPrices_.find(a_formID); found != itemPrices_.end()) {
            return found->second;
        }
        return std::nullopt;
    }

    const std::vector<RE::FormID>& FormResolver::NamedItems() const noexcept
    {
        return namedItems_;
    }

    bool FormResolver::IsBlocked(RE::FormID a_formID) const
    {
        // A block from the player or a patch always wins; the mod's own
        // default blocks give way to an item a patch names.
        return blockedItems_.contains(a_formID) ||
            (defaultBlockedItems_.contains(a_formID) && !allowedItems_.contains(a_formID));
    }

    bool FormResolver::IsSpecial(RE::FormID a_formID) const
    {
        return specialItems_.contains(a_formID);
    }

    bool FormResolver::IsNordHero(RE::FormID a_formID) const
    {
        return nordHeroWeapons_.contains(a_formID);
    }

    bool FormResolver::IsAncientNordArmor(RE::FormID a_formID) const
    {
        return ancientNordArmor_.contains(a_formID);
    }

    bool FormResolver::IsAncientNordWeapon(const RE::TESBoundObject* a_object) const
    {
        return a_object && a_object->Is(RE::FormType::Weapon) &&
            HasExactKeyword(a_object, ancientNordWeaponKeywords_);
    }

    std::string FormResolver::MenuLabelFor(const RE::TESBoundObject* a_object) const
    {
        if (!a_object) {
            return "<unnamed item>";
        }
        if (const auto found = menuLabelOverrides_.find(a_object->GetFormID());
            found != menuLabelOverrides_.end()) {
            return found->second;
        }
        const auto name = a_object->GetName();
        return name && name[0] != '\0' ? name : "<unnamed item>";
    }

    bool FormResolver::PreserveFullMenuLabel(const RE::TESBoundObject* a_object) const
    {
        return a_object && preserveFullMenuLabels_.contains(a_object->GetFormID());
    }

    std::optional<std::string_view> FormResolver::MaterialLabelFor(
        const RE::TESBoundObject* a_object) const
    {
        if (!a_object) {
            return std::nullopt;
        }
        if (const auto found = materialLabelOverrides_.find(a_object->GetFormID());
            found != materialLabelOverrides_.end()) {
            return found->second;
        }
        return std::nullopt;
    }

    std::string FormResolver::Lower(std::string_view a_value)
    {
        std::string result{ a_value };
        std::ranges::transform(result, result.begin(), [](unsigned char a_char) {
            return static_cast<char>(std::tolower(a_char));
        });
        return result;
    }

    std::vector<std::string> FormResolver::KeywordEditorIDs(const RE::TESBoundObject* a_object)
    {
        std::vector<std::string> result;
        const auto keywordForm = GetKeywordForm(a_object);
        if (!keywordForm) {
            return result;
        }

        result.reserve(keywordForm->GetNumKeywords());
        for (const auto keyword : keywordForm->GetKeywords()) {
            if (!keyword) {
                continue;
            }
            const auto editorID = keyword->GetFormEditorID();
            if (editorID && editorID[0] != '\0') {
                result.emplace_back(editorID);
            }
        }
        return result;
    }

    bool FormResolver::HasKeywordToken(
        const RE::TESBoundObject* a_object,
        std::span<const std::string> a_tokens)
    {
        const auto keywords = KeywordEditorIDs(a_object);
        for (const auto& keyword : keywords) {
            for (const auto& token : a_tokens) {
                if (ContainsWholeToken(keyword, token)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool FormResolver::HasExactKeyword(
        const RE::TESBoundObject* a_object,
        std::span<const std::string> a_editorIDs)
    {
        const auto keywords = KeywordEditorIDs(a_object);
        for (const auto& keyword : keywords) {
            const auto normalized = Lower(keyword);
            if (std::ranges::any_of(a_editorIDs, [&](const std::string& a_editorID) {
                    return normalized == Lower(a_editorID);
                })) {
                return true;
            }
        }
        return false;
    }

    bool FormResolver::HasTextToken(
        const RE::TESBoundObject* a_object,
        std::span<const std::string> a_tokens)
    {
        if (!a_object) {
            return false;
        }

        if (const auto editorID = a_object->GetFormEditorID(); editorID && editorID[0] != '\0') {
            for (const auto& token : a_tokens) {
                if (ContainsWholeToken(editorID, token)) {
                    return true;
                }
            }
        }
        if (const auto name = a_object->GetName(); name && name[0] != '\0') {
            for (const auto& token : a_tokens) {
                if (ContainsWholeToken(name, token)) {
                    return true;
                }
            }
        }
        return HasKeywordToken(a_object, a_tokens);
    }

    bool FormResolver::IsStalhrim(const RE::TESBoundObject* a_object)
    {
        static const std::array<std::string, 1> tokens{ "stalhrim" };
        return HasTextToken(a_object, tokens);
    }
}
