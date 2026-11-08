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

#include "NPCAlchemyServices/FormResolver.h"

#include <algorithm>
#include <charconv>
#include <cctype>

namespace NPCAS
{
    FormResolver& FormResolver::GetSingleton()
    {
        static FormResolver singleton;
        return singleton;
    }

    void FormResolver::Build(const Config& a_config)
    {
        npcServicesByFormID_.clear();
        npcServicesByName_.clear();
        allowDisplayNameFallback_ = a_config.allowDisplayNameFallback;

        alchemyFaction_ = nullptr;
        if (const auto form = Resolve(a_config.forms.alchemyFaction)) {
            alchemyFaction_ = form->As<RE::TESFaction>();
        }
        if (!alchemyFaction_) {
            SKSE::log::warn(
                "Alchemy faction '{}' could not be resolved. "
                "Only explicitly configured alchemists will be eligible.",
                a_config.forms.alchemyFaction);
        }

        for (const auto& entry : a_config.npcOverrides) {
            ResolvedNpcService service;
            service.tier = entry.tier;
            service.questRequired = !entry.requiredQuest.empty();
            service.lockedReason = entry.lockedReason;
            if (service.questRequired) {
                if (const auto form = Resolve(entry.requiredQuest)) {
                    service.requiredQuest = form->As<RE::TESQuest>();
                }
                if (!service.requiredQuest) {
                    SKSE::log::warn(
                        "Could not resolve required alchemy quest '{}' for '{}'. "
                        "The service will remain locked.",
                        entry.requiredQuest,
                        entry.name);
                }
            }

            if (!entry.form.empty()) {
                if (const auto form = Resolve(entry.form);
                    form && npcServicesByFormID_.contains(form->GetFormID())) {
                    // First entry wins: Artisans' own alchemists come before patches.
                    SKSE::log::warn(
                        "Alchemist '{}' ({:08X}) is one of Artisans' own artisans or already set; this entry is ignored.",
                        entry.name,
                        form->GetFormID());
                    continue;
                }
            }

            if (!entry.name.empty() && allowDisplayNameFallback_) {
                npcServicesByName_.try_emplace(Lower(entry.name), service);
            }

            bool resolved = false;
            if (!entry.form.empty()) {
                if (const auto form = Resolve(entry.form)) {
                    npcServicesByFormID_.insert_or_assign(
                        form->GetFormID(),
                        service);
                    resolved = true;
                } else {
                    SKSE::log::warn(
                        "Could not resolve alchemy NPC override '{}' ({})",
                        entry.name,
                        entry.form);
                }
            }
            if (!entry.editorID.empty()) {
                if (const auto form = Resolve(entry.editorID)) {
                    npcServicesByFormID_.insert_or_assign(
                        form->GetFormID(),
                        service);
                    resolved = true;
                } else {
                    SKSE::log::warn(
                        "Could not resolve alchemy NPC override '{}' by EditorID '{}'",
                        entry.name,
                        entry.editorID);
                }
            }
            if (!resolved && !allowDisplayNameFallback_) {
                SKSE::log::warn(
                    "Alchemy NPC override '{}' has no resolved FormID/EditorID. "
                    "Display-name fallback is disabled.",
                    entry.name);
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

    RE::TESFaction* FormResolver::AlchemyFaction() const noexcept
    {
        return alchemyFaction_;
    }

    const ResolvedNpcService* FormResolver::ServiceForNPC(
        RE::FormID a_formID,
        std::string_view a_displayName) const
    {
        if (const auto exact = npcServicesByFormID_.find(a_formID);
            exact != npcServicesByFormID_.end()) {
            return &exact->second;
        }
        if (allowDisplayNameFallback_) {
            if (const auto byName = npcServicesByName_.find(Lower(a_displayName));
                byName != npcServicesByName_.end()) {
                return &byName->second;
            }
        }
        return nullptr;
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
}
