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

#include "NPCEnchantingServices/FormResolver.h"

#include <charconv>

namespace
{
    [[nodiscard]] const RE::BGSKeywordForm* GetKeywordForm(
        const RE::TESBoundObject* a_object)
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
        return nullptr;
    }
}

namespace NPCES
{
    FormResolver& FormResolver::GetSingleton()
    {
        static FormResolver singleton;
        return singleton;
    }

    void FormResolver::Build(const Config& a_config)
    {
        blockedItems_.clear();

        const auto resolveID =
            [this](std::string_view a_spec, std::string_view a_label) {
                if (const auto form = Resolve(a_spec)) {
                    SKSE::log::info(
                        "Resolved {} to {:08X}",
                        a_label,
                        form->GetFormID());
                    return form->GetFormID();
                }
                SKSE::log::warn(
                    "Could not resolve {} form '{}'",
                    a_label,
                    a_spec);
                return RE::FormID{ 0 };
            };

        farengarFormID_ = resolveID(a_config.forms.farengar, "Farengar");
        calcelmoFormID_ = resolveID(a_config.forms.calcelmo, "Calcelmo");
        sergiusFormID_ = resolveID(a_config.forms.sergius, "Sergius");
        sybilleFormID_ = resolveID(a_config.forms.sybille, "Sybille");
        nelothFormID_ = resolveID(a_config.forms.neloth, "Neloth");
        feranSadriFormID_ =
            resolveID(a_config.forms.feranSadri, "Feran Sadri");

        collegeFaction_ = nullptr;
        if (const auto form = Resolve(a_config.forms.collegeFaction)) {
            collegeFaction_ = form->As<RE::TESFaction>();
        }
        volkiharFaction_ = nullptr;
        if (const auto form = Resolve(a_config.forms.volkiharFaction)) {
            volkiharFaction_ = form->As<RE::TESFaction>();
        }
        dawnguardFaction_ = nullptr;
        if (const auto form = Resolve(a_config.forms.dawnguardFaction)) {
            dawnguardFaction_ = form->As<RE::TESFaction>();
        }
        potemaFirstQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.forms.potemaFirstQuestEditorID);
        nelothDwemerQuest_ = RE::TESForm::LookupByEditorID<RE::TESQuest>(
            a_config.forms.nelothDwemerQuestEditorID);

        if (!collegeFaction_) {
            SKSE::log::warn("College faction could not be resolved.");
        }
        if (!volkiharFaction_) {
            SKSE::log::warn("Volkihar faction could not be resolved.");
        }
        if (!dawnguardFaction_) {
            SKSE::log::warn("Dawnguard faction could not be resolved.");
        }
        if (!potemaFirstQuest_) {
            SKSE::log::warn(
                "Potema quest '{}' could not be resolved.",
                a_config.forms.potemaFirstQuestEditorID);
        }
        if (!nelothDwemerQuest_) {
            SKSE::log::warn(
                "Neloth Dwemer quest '{}' could not be resolved.",
                a_config.forms.nelothDwemerQuestEditorID);
        }

        for (const auto& spec : a_config.blockItems) {
            if (const auto form = Resolve(spec)) {
                blockedItems_.insert(form->GetFormID());
            } else {
                SKSE::log::warn(
                    "Could not resolve blocked item form '{}'",
                    spec);
            }
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
        if (error != std::errc{} ||
            end != idText.data() + idText.size()) {
            return nullptr;
        }

        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler ? dataHandler->LookupForm(localID, plugin) : nullptr;
    }

    RE::FormID FormResolver::FarengarFormID() const noexcept
    {
        return farengarFormID_;
    }

    RE::FormID FormResolver::CalcelmoFormID() const noexcept
    {
        return calcelmoFormID_;
    }

    RE::FormID FormResolver::SergiusFormID() const noexcept
    {
        return sergiusFormID_;
    }

    RE::FormID FormResolver::SybilleFormID() const noexcept
    {
        return sybilleFormID_;
    }

    RE::FormID FormResolver::NelothFormID() const noexcept
    {
        return nelothFormID_;
    }

    RE::FormID FormResolver::FeranSadriFormID() const noexcept
    {
        return feranSadriFormID_;
    }

    RE::TESFaction* FormResolver::CollegeFaction() const noexcept
    {
        return collegeFaction_;
    }

    RE::TESFaction* FormResolver::VolkiharFaction() const noexcept
    {
        return volkiharFaction_;
    }

    RE::TESFaction* FormResolver::DawnguardFaction() const noexcept
    {
        return dawnguardFaction_;
    }

    RE::TESQuest* FormResolver::PotemaFirstQuest() const noexcept
    {
        return potemaFirstQuest_;
    }

    RE::TESQuest* FormResolver::NelothDwemerQuest() const noexcept
    {
        return nelothDwemerQuest_;
    }

    bool FormResolver::IsBlockedItem(RE::FormID a_formID) const
    {
        return blockedItems_.contains(a_formID);
    }

    std::string FormResolver::Lower(std::string_view a_value)
    {
        std::string result{ a_value };
        std::ranges::transform(
            result,
            result.begin(),
            [](unsigned char a_char) {
                return static_cast<char>(std::tolower(a_char));
            });
        return result;
    }

    std::vector<std::string> FormResolver::KeywordEditorIDs(
        const RE::TESBoundObject* a_object)
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
                result.push_back(Lower(editorID));
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
                if (keyword.find(Lower(token)) != std::string::npos) {
                    return true;
                }
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

        std::string haystack;
        if (const auto editorID = a_object->GetFormEditorID();
            editorID && editorID[0] != '\0') {
            haystack += Lower(editorID);
            haystack += ' ';
        }
        if (const auto name = a_object->GetName();
            name && name[0] != '\0') {
            haystack += Lower(name);
            haystack += ' ';
        }
        for (const auto& keyword : KeywordEditorIDs(a_object)) {
            haystack += keyword;
            haystack += ' ';
        }

        for (const auto& token : a_tokens) {
            if (haystack.find(Lower(token)) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
}
