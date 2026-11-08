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

#include "NPCAlchemyServices/MenuController.h"
#include "ArtisansOfSkyrim/Localization.h"
#include "NPCAlchemyServices/InventoryService.h"
#include "ArtisansOfSkyrim/ServiceMenu.h"
#include "ArtisansOfSkyrim/MenuSort.h"
namespace NPCAS
{
    namespace
    {
        [[nodiscard]] std::string InventoryEffectDescription(
            RE::TESBoundObject* a_object)
        {
            const auto item = a_object ? a_object->As<RE::AlchemyItem>() : nullptr;
            if (!item) {
                return {};
            }

            RE::BSString description;
            RE::MagicSystem::GetMagicItemDescription(
                description,
                item,
                "",
                "");
            return description.c_str() ? description.c_str() : "";
        }

        void ShowLockedServicesMessage(const AlchemistContext& alchemist)
        {
            const auto reason = alchemist.lockedReason.empty() ?
                "This alchemist is not willing to offer the service." :
                alchemist.lockedReason;
            const auto message = std::format(
                "{}\n{}\n\n{}",
                alchemist.displayName,
                AOS::Text::Format("Rank: {}", AOS::Text::Localize(ToString(alchemist.tier))),
                AOS::Text::LocalizeMessage(reason));
            SKSE::GetTaskInterface()->AddTask([message] {
                RE::DebugMessageBox(message.c_str());
            });
        }
    }

    MenuController& MenuController::GetSingleton() { static MenuController value; return value; }
    void MenuController::Configure(const Config& config) { config_ = &config; }
    void MenuController::OpenForTarget(RE::Actor* actor)
    {
        if (!config_ || !actor) return;
        alchemist_ = AlchemistClassifier::GetSingleton().Classify(actor);
        if (!alchemist_) return;
        if (!alchemist_->servicesUnlocked) { ShowLockedServicesMessage(*alchemist_); return; }
        ShowBrowser();
    }
    void MenuController::ShowBrowser(std::string notice)
    {
        if (!alchemist_) return;
        AOS::ServicePage page;
        page.key = std::format("alchemy-{}", alchemist_->baseFormID);
        page.artisan = alchemist_->displayName; page.tier = ToString(alchemist_->tier);
        page.profession = "Alchemist";
        page.layout = "alchemy";
        page.categoryLabel = "Type";
        page.groupLabel = "Effect";
        page.notice = std::move(notice);
        if (!alchemist_->servicesUnlocked) {
            ShowLockedServicesMessage(*alchemist_);
            AOS::ServiceMenu::Close();
            return;
        }
        else for (const auto item : InventoryService::GetSingleton().ListItems(*alchemist_)) {
            if (!item || !item->definition || !item->item) continue;
            const auto& definition = *item->definition;
            AOS::ServiceRow row;
            row.key = definition.id; row.name = definition.displayName;
            row.category = ToString(definition.itemType);
            row.typeValue = row.category;
            for (const auto& group : definition.groups) {
                if (!row.group.empty()) row.group += " / ";
                row.group += group;
            }
            row.effectValue = InventoryEffectDescription(item->item);
            if (row.effectValue.empty()) {
                row.effectValue = definition.effect;
                if (!definition.valueText.empty()) {
                    row.effectValue += " - " + definition.valueText;
                }
            }
            row.object = item->item; row.quantity = definition.batchSize;
            row.price = InventoryService::GetSingleton().Price(*item);
            row.action = "Craft"; row.confirm = true;
            row.costDefined = true;
            row.activate = [this, item] {
                const auto result = InventoryService::GetSingleton().Purchase(*alchemist_, *item);
                ShowBrowser(result.message);
            };
            page.rows.push_back(std::move(row));
        }
        if (page.rows.empty() && page.notice.empty()) page.notice = "No alchemy services are available.";
        AOS::StableSortByDisplayLabel(page.rows, [](const auto& row) { return row.name; });
        AOS::ServiceMenu::Present(std::move(page));
    }
}
