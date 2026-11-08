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

#include "NPCSmithServices/MenuController.h"
#include "NPCSmithServices/DurationFormatter.h"
#include "ArtisansOfSkyrim/Localization.h"
#include "NPCSmithServices/FormResolver.h"
#include "NPCSmithServices/OrderService.h"
#include "NPCSmithServices/OrderDialogueService.h"
#include "ArtisansOfSkyrim/ServiceMenu.h"
#include "ArtisansOfSkyrim/MenuSort.h"
namespace
{
    [[nodiscard]] std::string_view CategoryLabel(NPCSS::MenuItemCategory a_category) noexcept
    {
        switch (a_category) {
        case NPCSS::MenuItemCategory::kWeapons:
            return "Weapons";
        case NPCSS::MenuItemCategory::kArmor:
            return "Armor";
        case NPCSS::MenuItemCategory::kShields:
            return "Shields";
        case NPCSS::MenuItemCategory::kAmmo:
            return "Arrows/Bolts";
        }
        return "Items";
    }

    [[nodiscard]] bool LooksLikeShield(const RE::TESBoundObject* a_object)
    {
        if (!a_object || !a_object->Is(RE::FormType::Armor)) {
            return false;
        }

        // Keyword editor IDs vary across vanilla/modded records; the name fallback
        // keeps shields out of the generic Armor category even when custom mods omit
        // exact shield keywords.
        const std::array<std::string, 3> shieldTokens{ "shield", "armorShield", "armorshield" };
        if (NPCSS::FormResolver::HasKeywordToken(a_object, shieldTokens)) {
            return true;
        }

        const auto name = a_object->GetName();
        if (!name || name[0] == '\0') {
            return false;
        }
        return NPCSS::FormResolver::Lower(name).find("shield") != std::string::npos;
    }

    [[nodiscard]] NPCSS::MenuItemCategory CategoryFor(const RE::TESBoundObject* a_object)
    {
        if (!a_object) {
            return NPCSS::MenuItemCategory::kWeapons;
        }
        if (a_object->Is(RE::FormType::Ammo)) {
            return NPCSS::MenuItemCategory::kAmmo;
        }
        if (a_object->Is(RE::FormType::Armor)) {
            return LooksLikeShield(a_object) ?
                NPCSS::MenuItemCategory::kShields :
                NPCSS::MenuItemCategory::kArmor;
        }
        return NPCSS::MenuItemCategory::kWeapons;
    }

    [[nodiscard]] NPCSS::ArmorWeightClass ArmorWeightFor(
        const RE::TESObjectARMO* a_armor) noexcept
    {
        if (!a_armor) {
            return NPCSS::ArmorWeightClass::kNone;
        }
        if (a_armor->IsLightArmor()) {
            return NPCSS::ArmorWeightClass::kLight;
        }
        if (a_armor->IsHeavyArmor()) {
            return NPCSS::ArmorWeightClass::kHeavy;
        }
        return NPCSS::ArmorWeightClass::kNone;
    }

    [[nodiscard]] std::string CraftingItemTypeLabelForObject(
        RE::TESBoundObject* a_object,
        std::optional<NPCSS::CraftingOrderCategory> a_category)
    {
        if (!a_object) {
            return {};
        }

        if (a_category) {
            // Mod two-handers sharing the axe animation are not battleaxes.
            if (const auto weapon = a_object->As<RE::TESObjectWEAP>();
                weapon && weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe &&
                !NPCSS::FormResolver::HasExactKeyword(
                    weapon,
                    std::array<std::string, 2>{ "WeapTypeBattleaxe", "WeapTypeWarhammer" })) {
                return "Two-Handed Weapon";
            }
            const auto armor = a_object->As<RE::TESObjectARMO>();
            return NPCSS::CraftingItemTypeLabel(
                *a_category,
                ArmorWeightFor(armor));
        }

        if (a_object->Is(RE::FormType::Armor)) {
            if (const auto armor = a_object->As<RE::TESObjectARMO>()) {
                switch (ArmorWeightFor(armor)) {
                case NPCSS::ArmorWeightClass::kLight: return "Light Armor";
                case NPCSS::ArmorWeightClass::kHeavy: return "Heavy Armor";
                case NPCSS::ArmorWeightClass::kNone: break;
                }
            }
            return "Armor";
        }
        if (a_object->Is(RE::FormType::Weapon)) {
            return "Weapon";
        }
        if (a_object->Is(RE::FormType::Ammo)) {
            return "Ammunition";
        }
        return {};
    }

    struct MaterialGroup
    {
        std::string label;
        std::vector<std::string> tokens;
    };

    [[nodiscard]] const std::vector<MaterialGroup>& MaterialGroups()
    {
        static const std::vector<MaterialGroup> groups{
            { "Dawnguard", { "dawnguard", "crossbow", "bolt" } },
            { "Nord Hero", { "nord hero" } },
            { "Ancient Nord", { "ancient nord", "draugr" } },
            { "Stalhrim", { "stalhrim" } },
            { "Dragon", { "dragonbone", "dragonplate", "dragonscale" } },
            { "Daedric", { "daedric" } },
            { "Ebony", { "ebony" } },
            { "Glass", { "glass" } },
            { "Nordic", { "nordic" } },
            { "Elven", { "elven" } },
            { "Orcish", { "orcish" } },
            { "Dwarven", { "dwarven", "dwemer", "dwarf" } },
            { "Steel Plate", { "steelplate", "steel plate" } },
            { "Scaled", { "scaled" } },
            { "Imperial", { "imperial" } },
            { "Silver", { "silver" } },
            { "Steel", { "steel" } },
            { "Studded", { "studded" } },
            { "Leather", { "leather" } },
            { "Hide", { "hide" } },
            { "Iron", { "iron" } },
            { "Bonemold", { "bonemold" } },
            { "Chitin", { "chitin" } },
        };
        return groups;
    }

    [[nodiscard]] std::string MaterialLabel(const RE::TESBoundObject* a_object)
    {
        if (!a_object) {
            return "Other";
        }
        const auto& resolver = NPCSS::FormResolver::GetSingleton();
        if (const auto label = resolver.MaterialLabelFor(a_object)) {
            return std::string{ *label };
        }
        if (a_object->GetFormID() == 0x0003B562) {
            return "Iron";
        }
        static constexpr std::array<RE::FormID, 4> wolfArmor{
            0x000CAE15,  // Wolf Armor
            0x000CEE7C,  // Wolf Boots
            0x000CEE7E,  // Wolf Gauntlets
            0x0004C3D0,  // Wolf Helmet
        };
        const auto sourceFile = a_object->GetFile(0);
        if (sourceFile &&
            std::string_view{ sourceFile->GetFilename() }.ends_with("Skyrim.esm") &&
            std::ranges::find(wolfArmor, a_object->GetLocalFormID()) !=
                wolfArmor.end()) {
            return "Wolf";
        }
        static constexpr std::array<RE::FormID, 5> vanillaSkyforgeSteel{
            0x0009F25C,
            0x0009F25D,
            0x0009F25E,
            0x0009F25F,
            0x0009F260,
        };
        if (std::ranges::find(vanillaSkyforgeSteel, a_object->GetFormID()) !=
            vanillaSkyforgeSteel.end()) {
            return "Steel";
        }
        if (const auto editorID = a_object->GetFormEditorID();
            editorID && editorID[0] != '\0') {
            const auto normalized = NPCSS::FormResolver::Lower(editorID);
            if (normalized.starts_with("aos_armorskyforgeplate")) {
                return "Steel Plate";
            }
            if (normalized.starts_with("aos_weapskyforge") ||
                normalized == "aos_armorskyforgesteelshield") {
                return "Steel";
            }
        }
        if (resolver.IsNordHero(a_object->GetFormID())) {
            return "Nord Hero";
        }
        if (resolver.IsAncientNordArmor(a_object->GetFormID())) {
            return "Ancient Nord";
        }
        if (NPCSS::FormResolver::HasTextToken(
                a_object,
                std::array<std::string, 3>{ "dawnguard", "crossbow", "bolt" })) {
            return "Dawnguard";
        }

        // Dragonborn's heavy Chitin gauntlets also carry PerkFistsSteelPlate.
        // Honor their actual material keywords before the broader text fallback
        // can mistake that auxiliary perk keyword for Steel Plate equipment.
        if (NPCSS::FormResolver::HasExactKeyword(
                a_object,
                std::array<std::string, 2>{
                    "DLC2ArmorMaterialChitinHeavy",
                    "DLC2ArmorMaterialChitinLight" })) {
            return "Chitin";
        }

        // Composite steel groups need explicit UI handling. Some records expose
        // generic Steel keywords even when their display name/editor ID belongs
        // to a specific set.
        if (NPCSS::FormResolver::HasExactKeyword(
                a_object,
                std::array<std::string, 1>{ "ArmorMaterialSteelPlate" }) ||
            NPCSS::FormResolver::HasTextToken(
                a_object,
                std::array<std::string, 2>{ "steel plate", "steelplate" })) {
            return "Steel Plate";
        }

        // Creation/modded variants such as "Steel Nordic Gauntlets" should be
        // grouped with Steel for menu readability, not with true Nordic/Nordic
        // Carved equipment.
        if (NPCSS::FormResolver::HasTextToken(
                a_object,
                std::array<std::string, 3>{ "steel nordic", "steelnordic", "nordic steel" })) {
            return "Steel";
        }

        if (NPCSS::FormResolver::HasTextToken(a_object, std::array<std::string, 1>{ "imperial" })) {
            return "Imperial";
        }
        if (NPCSS::FormResolver::HasExactKeyword(
                a_object,
                std::array<std::string, 2>{ "ArmorMaterialOrcish", "WeapMaterialOrcish" })) {
            return "Orcish";
        }
        if (NPCSS::FormResolver::HasExactKeyword(
                a_object,
                std::array<std::string, 2>{ "ArmorMaterialSteel", "WeapMaterialSteel" })) {
            return "Steel";
        }
        if (NPCSS::FormResolver::HasExactKeyword(
                a_object,
                std::array<std::string, 2>{ "ArmorMaterialDwarven", "WeapMaterialDwarven" }) ||
            NPCSS::FormResolver::HasTextToken(
                a_object,
                std::array<std::string, 3>{ "dwarven", "dwemer", "dwarf" })) {
            return "Dwarven";
        }
        if (NPCSS::FormResolver::HasTextToken(a_object, std::array<std::string, 1>{ "silver" })) {
            return "Silver";
        }

        for (const auto& group : MaterialGroups()) {
            if (NPCSS::FormResolver::HasTextToken(a_object, group.tokens)) {
                return group.label;
            }
        }
        return "Other";
    }

}
namespace NPCSS
{
    namespace
    {
        void ShowLockedServicesMessage(const SmithContext& smith)
        {
            const auto reason = smith.lockedReason.empty() ?
                "This master blacksmith does not offer full services yet." :
                smith.lockedReason;
            const auto message = std::format(
                "{}\n{}\n\n{}",
                smith.displayName,
                AOS::Text::Format("Rank: {}", AOS::Text::Localize(ToString(smith.tier))),
                AOS::Text::LocalizeMessage(reason));
            SKSE::GetTaskInterface()->AddTask([message] {
                RE::DebugMessageBox(message.c_str());
            });
        }
    }

    MenuController& MenuController::GetSingleton() { static MenuController value; return value; }
    void MenuController::Configure(const Config& config) { config_ = &config; }
    bool MenuController::PrepareTarget(RE::Actor* actor)
    {
        if (!config_ || !actor) return false;
        smith_ = SmithClassifier::GetSingleton().Classify(actor);
        if (!smith_) return false;
        return true;
    }
    void MenuController::OpenForTarget(RE::Actor* actor) { OpenCraftingForTarget(actor); }
    void MenuController::OpenCraftingForTarget(RE::Actor* actor)
    {
        if (!PrepareTarget(actor)) return;
        if (!smith_->servicesUnlocked) { ShowLockedServicesMessage(*smith_); return; }
        selectedImprovementQuality_.reset();
        ShowBrowser(false);
    }
    void MenuController::OpenImprovementForTarget(RE::Actor* actor)
    {
        if (!PrepareTarget(actor)) return;
        if (!smith_->servicesUnlocked) { ShowLockedServicesMessage(*smith_); return; }
        selectedImprovementQuality_.reset();
        ShowImprovementQualities();
    }
    void MenuController::OpenCollectionForTarget(RE::Actor* actor)
    {
        if (!PrepareTarget(actor)) return;
        if (!smith_->servicesUnlocked) { ShowLockedServicesMessage(*smith_); return; }
        ShowOrders();
    }
    void MenuController::ShowImprovementQualities(std::string notice)
    {
        if (!smith_ || !config_) return;
        if (!smith_->servicesUnlocked) {
            ShowLockedServicesMessage(*smith_);
            AOS::ServiceMenu::Close();
            return;
        }

        selectedImprovementQuality_.reset();
        AOS::ServicePage page;
        page.key = std::format("smith-{}-quality", smith_->baseFormID);
        page.artisan = smith_->displayName;
        page.tier = ToString(smith_->tier);
        page.profession = "Blacksmith";
        // Own layout so the menu keeps the Craft/Improve tabs and strength order.
        page.layout = "smithing-quality";
        page.showCategoryFilter = false;
        page.showGroupFilter = false;
        page.notice = std::move(notice);
        page.alternateLabel = "Craft equipment";
        page.alternate = [this] { ShowBrowser(false); };

        constexpr std::array qualities{
            TemperQuality::kFine,
            TemperQuality::kSuperior,
            TemperQuality::kExquisite,
            TemperQuality::kFlawless,
            TemperQuality::kEpic,
            TemperQuality::kLegendary,
        };
        for (const auto quality : qualities) {
            if (!CanOfferQuality(smith_->tier, quality)) continue;
            if (InventoryService::GetSingleton().ListImprovementItems(
                    *smith_,
                    quality).empty()) {
                continue;
            }
            AOS::ServiceRow row;
            row.key = std::string{ ToString(quality) };
            row.name = std::string{ ToString(quality) };
            row.resultQuality = row.name;
            row.action = "Select";
            row.activate = [this, quality] {
                selectedImprovementQuality_ = quality;
                ShowBrowser(true);
            };
            page.rows.push_back(std::move(row));
        }
        if (page.rows.empty() && page.notice.empty()) {
            page.notice = "No eligible equipment is available for improvement.";
        }
        AOS::ServiceMenu::Present(std::move(page));
    }
    void MenuController::ShowBrowser(bool improve, std::string notice)
    {
        if (!smith_ || !config_) return;
        if (improve && !selectedImprovementQuality_) {
            ShowImprovementQualities(std::move(notice));
            return;
        }
        AOS::ServicePage page;
        page.key = improve ?
            std::format(
                "smith-{}-improve-{}",
                smith_->baseFormID,
                static_cast<std::uint8_t>(*selectedImprovementQuality_)) :
            std::format("smith-{}-craft", smith_->baseFormID);
        page.artisan = smith_->displayName;
        page.tier = ToString(smith_->tier);
        page.profession = "Blacksmith";
        page.layout = improve ? "smithing-improve" : "smithing-craft";
        page.categoryLabel = "Type";
        page.groupLabel = "Material";
        page.notice = std::move(notice);
        page.alternateLabel = improve ? "Craft equipment" : "Improve equipment";
        page.alternate = [this, improve] {
            if (improve) {
                ShowBrowser(false);
            } else {
                ShowImprovementQualities();
            }
        };
        if (!smith_->servicesUnlocked) {
            ShowLockedServicesMessage(*smith_);
            AOS::ServiceMenu::Close();
            return;
        }
        if (improve) {
            page.back = [this] { ShowImprovementQualities(); };
            const auto items = InventoryService::GetSingleton().ListImprovementItems(
                *smith_,
                *selectedImprovementQuality_);
            for (const auto& item : items) {
                AOS::ServiceRow row;
                row.key = std::format("{}-{}", item.object->GetFormID(), reinterpret_cast<std::uintptr_t>(item.extraList));
                row.name = item.displayName;
                row.object = item.object; row.extra = item.extraList;
                row.category = CategoryLabel(CategoryFor(item.object));
                row.group = MaterialLabel(item.object);
                row.currentQuality = ToString(item.currentQuality);
                row.resultQuality = ToString(item.targetQuality);
                row.currentHealth = static_cast<float>(item.currentHealth);
                row.targetHealth = static_cast<float>(item.targetHealth);
                row.deliveryValue = "Immediate";
                row.action = "Improve"; row.price = item.price; row.confirm = true;
                row.costDefined = true;
                row.activate = [this, item] {
                    const auto result = InventoryService::GetSingleton().ApplyImprovement(*smith_, item);
                    ShowBrowser(true, result.message);
                };
                page.rows.push_back(std::move(row));
            }
        } else {
            const auto items = RecipeIndex::GetSingleton().ListCraftable(*smith_);
            for (const auto& item : items) {
                AOS::ServiceRow row;
                row.key = std::to_string(item.object->GetFormID());
                row.name = FormResolver::GetSingleton().MenuLabelFor(item.object);
                row.object = item.object;
                row.category = CategoryLabel(CategoryFor(item.object));
                row.group = MaterialLabel(item.object);
                row.price = item.price; row.quantity = item.outputCount; row.confirm = true;
                row.action = "Craft";
                row.typeValue = CraftingItemTypeLabelForObject(item.object, ClassifyCraftingOrderCategory(item.object));
                row.deliveryValue = "Immediate";
                if (config_->orders.enabled) {
                    const auto preview = OrderService::GetSingleton().Preview(*smith_, item);
                    if (!preview.valid) {
                        row.enabled = false; row.blockedReason = preview.error;
                    } else if (!IsInstantOrderCategory(preview.category)) {
                        row.action = "Craft";
                        const auto calendar = RE::Calendar::GetSingleton();
                        const double now = calendar ? calendar->GetCurrentGameTime() * 24.0 : preview.schedule.startGameHour;
                        const auto readyIn = FormatDurationHours(std::max(0.0, preview.schedule.readyGameHour - now));
                        row.deliveryValue = readyIn;
                    }
                }
                row.activate = [this, item] {
                    const auto result = InventoryService::GetSingleton().ApplyCrafting(*smith_, item);
                    ShowBrowser(false, result.message);
                };
                row.costDefined = true;
                page.rows.push_back(std::move(row));
            }
        }
        if (page.rows.empty() && page.notice.empty()) page.notice = "No eligible equipment is available for this service.";
        AOS::StableSortByDisplayLabel(page.rows, [](const auto& row) { return row.name; });
        AOS::ServiceMenu::Present(std::move(page));
    }
    void MenuController::ShowOrders(std::string notice)
    {
        if (!smith_ || !config_ || !config_->orders.enabled) { ShowBrowser(false); return; }
        auto& service = OrderService::GetSingleton();
        service.Update();
        AOS::ServicePage page;
        page.key = std::format("orders-{}", smith_->workshopKey);
        page.artisan = smith_->displayName; page.tier = ToString(smith_->tier);
        page.profession = "Blacksmith";
        page.layout = "orders";
        page.categoryLabel = "Status";
        page.groupLabel = "";
        page.showGroupFilter = false;
        page.notice = std::move(notice);
        for (const auto& order : service.Orders()) {
            if (order.workshopKey != smith_->workshopKey) continue;
            AOS::ServiceRow row;
            row.key = std::to_string(order.uniqueOrderID); row.name = order.displayName;
            row.object = RE::TESForm::LookupByID<RE::TESBoundObject>(order.itemFormID);
            row.quantity = order.quantity; row.confirm = false;
            row.enabled = order.status == CraftingOrderStatus::kReady;
            row.category = row.enabled ? "Ready" : "In progress";
            row.statusValue = row.category;
            row.pickupValue = order.pickupLocation;
            row.action = "Collect";
            if (!row.enabled) {
                const auto calendar = RE::Calendar::GetSingleton();
                const double now = calendar ? calendar->GetCurrentGameTime() * 24.0 : order.startGameHour;
                row.timeValue = FormatDurationHours(std::max(0.0, order.readyGameHour - now));
                row.blockedReason = AOS::Text::Format("Ready in {}", AOS::Text::Localize(row.timeValue));
            } else {
                row.timeValue = "Ready";
            }
            row.activate = [this] {
                const auto actor = smith_->actor.get();
                const auto summary = OrderService::GetSingleton().CollectOrders(actor.get());
                OrderDialogueService::GetSingleton().RefreshForActor(actor.get());
                ShowOrders(summary);
            };
            page.rows.push_back(std::move(row));
        }
        if (page.rows.empty() && page.notice.empty()) page.notice = "This workshop has no active orders.";
        AOS::ServiceMenu::Present(std::move(page));
    }
}
