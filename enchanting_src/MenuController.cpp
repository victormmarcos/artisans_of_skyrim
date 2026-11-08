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

#include "NPCEnchantingServices/MenuController.h"
#include "ArtisansOfSkyrim/ServiceMenu.h"
#include "ArtisansOfSkyrim/MenuSort.h"
#include "ArtisansOfSkyrim/Localization.h"
#include <algorithm>
#include <cctype>
#include <cmath>
namespace {
    void ReplaceAll(
        std::string& a_text,
        std::string_view a_token,
        std::string_view a_value)
    {
        for (auto position = a_text.find(a_token);
             position != std::string::npos;
             position = a_text.find(a_token, position + a_value.size())) {
            a_text.replace(position, a_token.size(), a_value);
        }
    }

    [[nodiscard]] std::string DisplayNumber(float a_value)
    {
        const auto rounded = std::lround(a_value);
        if (std::abs(a_value - static_cast<float>(rounded)) < 0.05F) {
            return std::to_string(rounded);
        }
        auto value = std::format("{:.1f}", a_value);
        while (value.ends_with('0')) {
            value.pop_back();
        }
        if (value.ends_with('.')) {
            value.pop_back();
        }
        return value;
    }

    [[nodiscard]] std::string EffectDescription(
        const NPCES::ResolvedEnchantment& a_enchantment,
        NPCES::ServiceTier a_tier)
    {
        const std::array<const NPCES::ResolvedEnchantment*, 1> selected{
            std::addressof(a_enchantment)
        };
        RE::BSTArray<RE::Effect> effects;
        if (!NPCES::EnchantmentCatalog::GetSingleton().BuildEffects(
                { selected.data(), selected.size() },
                a_tier,
                effects)) {
            return {};
        }

        std::string result;
        for (const auto& effect : effects) {
            if (!effect.baseEffect ||
                effect.baseEffect->data.flags.all(
                    RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) {
                continue;
            }

            const auto rawDescription =
                effect.baseEffect->magicItemDescription.c_str();
            if (!rawDescription) {
                continue;
            }
            std::string description = rawDescription;
            if (description.empty()) {
                continue;
            }
            ReplaceAll(description, "<mag>", DisplayNumber(effect.GetMagnitude()));
            ReplaceAll(description, "<dur>", std::to_string(effect.GetDuration()));
            ReplaceAll(description, "<area>", std::to_string(effect.GetArea()));
            if (result.find(description) != std::string::npos) {
                continue;
            }
            if (!result.empty()) {
                result += '\n';
            }
            result += description;
        }
        return result;
    }

    [[nodiscard]] bool IsNumericToken(std::string_view a_value)
    {
        if (a_value.empty()) {
            return false;
        }
        bool sawDigit = false;
        for (const auto ch : a_value) {
            if (std::isdigit(static_cast<unsigned char>(ch))) {
                sawDigit = true;
                continue;
            }
            if (ch == '.' || ch == ',') {
                continue;
            }
            return false;
        }
        return sawDigit;
    }

    [[nodiscard]] std::string Lower(std::string_view a_value)
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

    [[nodiscard]] std::string StripTrailingValueText(std::string a_name)
    {
        const auto parenthetical = a_name.find(" (");
        if (parenthetical != std::string::npos) {
            const auto tail = Lower(std::string_view{ a_name }.substr(parenthetical));
            if (tail.find("point") != std::string::npos ||
                tail.find("second") != std::string::npos ||
                tail.find("level") != std::string::npos ||
                tail.find('%') != std::string::npos ||
                std::ranges::any_of(tail, [](unsigned char a_char) {
                    return std::isdigit(a_char);
                })) {
                a_name.erase(parenthetical);
            }
        }

        std::vector<std::size_t> tokenStarts;
        for (std::size_t index = 0; index < a_name.size();) {
            while (index < a_name.size() &&
                   std::isspace(static_cast<unsigned char>(a_name[index]))) {
                ++index;
            }
            if (index < a_name.size()) {
                tokenStarts.push_back(index);
            }
            while (index < a_name.size() &&
                   !std::isspace(static_cast<unsigned char>(a_name[index]))) {
                ++index;
            }
        }

        if (tokenStarts.empty()) {
            return a_name;
        }

        const auto token = [&](std::size_t a_tokenIndex) {
            const auto start = tokenStarts[a_tokenIndex];
            const auto end = a_tokenIndex + 1 < tokenStarts.size() ?
                tokenStarts[a_tokenIndex + 1] :
                a_name.size();
            auto value = std::string_view{ a_name }.substr(start, end - start);
            while (!value.empty() &&
                   std::isspace(static_cast<unsigned char>(value.back()))) {
                value.remove_suffix(1);
            }
            return value;
        };

        const auto last = token(tokenStarts.size() - 1);
        const auto lastLower = Lower(last);
        if (lastLower.ends_with('%') &&
            IsNumericToken(lastLower.substr(0, lastLower.size() - 1))) {
            a_name.erase(tokenStarts.back());
            return a_name;
        }

        if (tokenStarts.size() >= 2) {
            const auto previous = token(tokenStarts.size() - 2);
            const auto previousLower = Lower(previous);
            if ((lastLower == "point" || lastLower == "points" ||
                 lastLower == "second" || lastLower == "seconds") &&
                IsNumericToken(previous)) {
                a_name.erase(tokenStarts[tokenStarts.size() - 2]);
                return a_name;
            }
            if (previousLower == "level" && IsNumericToken(last)) {
                a_name.erase(tokenStarts[tokenStarts.size() - 2]);
                return a_name;
            }
        }

        return a_name;
    }

    [[nodiscard]] std::string EffectValueText(
        const NPCES::ResolvedEnchantment& a_enchantment,
        NPCES::ServiceTier a_tier)
    {
        const auto& value =
            NPCES::EnchantmentCatalog::GetSingleton().ValueFor(
                a_enchantment,
                a_tier);
        if (!value.valueText.empty()) {
            return value.valueText;
        }

        std::string text;
        if (value.magnitude) {
            text += std::to_string(static_cast<int>(*value.magnitude));
        }
        if (value.duration) {
            if (!text.empty()) {
                text += " / ";
            }
            text += std::to_string(*value.duration) + " sec";
        }
        return text;
    }

    [[nodiscard]] std::string EffectDetail(
        const NPCES::ResolvedEnchantment& a_enchantment,
        NPCES::ServiceTier a_tier)
    {
        auto text = StripTrailingValueText(a_enchantment.definition->displayName);
        const auto valueText = EffectValueText(a_enchantment, a_tier);
        if (!valueText.empty()) {
            text += " (" + valueText + ")";
        }
        return text;
    }

}
namespace NPCES
{
    namespace
    {
        void ShowLockedServicesMessage(const EnchanterContext& enchanter)
        {
            const auto reason = enchanter.lockedReason.empty() ?
                "This enchanter is not willing to offer the service." :
                enchanter.lockedReason;
            const auto message = std::format(
                "{}\n{}\n\n{}",
                enchanter.displayName,
                AOS::Text::Format("Rank: {}", AOS::Text::Localize(ToString(enchanter.tier))),
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
        enchanter_ = EnchanterClassifier::GetSingleton().Classify(actor);
        selected_.reset(); first_ = nullptr; second_ = nullptr;
        if (!enchanter_) return;
        if (!enchanter_->servicesUnlocked) { ShowLockedServicesMessage(*enchanter_); return; }
        ShowItems();
    }
    void MenuController::ShowItems(std::string notice)
    {
        if (!enchanter_) return;
        AOS::ServicePage page;
        page.key = std::format("enchant-items-{}", enchanter_->baseFormID);
        page.artisan = enchanter_->displayName; page.tier = ToString(enchanter_->tier);
        page.profession = "Enchanter";
        page.layout = "enchanting-items";
        page.categoryLabel = "Type";
        page.groupLabel = "";
        page.showGroupFilter = false;
        page.notice = std::move(notice);
        if (!enchanter_->servicesUnlocked) {
            ShowLockedServicesMessage(*enchanter_);
            AOS::ServiceMenu::Close();
            return;
        }
        else for (const auto& item : InventoryService::GetSingleton().ListItems(*enchanter_)) {
            AOS::ServiceRow row;
            row.key = std::format("{}-{}", item.object->GetFormID(), reinterpret_cast<std::uintptr_t>(item.extraList));
            row.name = item.displayName; row.category = ToString(item.category);
            row.typeValue = row.category;
            row.object = item.object; row.extra = item.extraList;
            row.action = "Select";
            row.activate = [this, item] {
                selected_ = item;
                first_ = nullptr;
                second_ = nullptr;
                ShowEffects();
            };
            page.rows.push_back(std::move(row));
        }
        if (page.rows.empty() && page.notice.empty()) page.notice = "No eligible items in your inventory.";
        AOS::StableSortByDisplayLabel(page.rows, [](const auto& row) { return row.name; });
        AOS::ServiceMenu::Present(std::move(page));
    }
    void MenuController::ShowEffects(std::string notice)
    {
        if (!enchanter_ || !selected_) { ShowItems(); return; }
        const auto allowsTwo = CanUseDual(enchanter_->tier);
        AOS::ServicePage page;
        page.key = std::format(
            "enchant-effects-{}-{}",
            enchanter_->baseFormID,
            selected_->object->GetFormID());
        page.artisan = enchanter_->displayName; page.tier = ToString(enchanter_->tier);
        page.profession = "Enchanter";
        page.layout = "enchanting-effects";
        page.categoryLabel = "Effect group";
        page.groupLabel = "";
        page.showGroupFilter = false;
        page.notice = std::move(notice);
        page.back = [this] {
            if (second_) {
                second_ = nullptr;
                ShowEffects();
            } else if (first_) {
                first_ = nullptr;
                ShowEffects();
            } else {
                ShowItems();
            }
        };
        for (const auto effect : InventoryService::GetSingleton().ListEnchantments(*enchanter_, *selected_)) {
            if (!effect || !effect->definition) continue;
            AOS::ServiceRow row;
            row.key = effect->definition->id; row.name = StripTrailingValueText(effect->definition->displayName);
            row.category = effect->definition->group;
            row.object = selected_->object; row.extra = selected_->extraList;
            row.effectValue = EffectDescription(*effect, enchanter_->tier);
            if (row.effectValue.empty()) {
                row.effectValue = EffectDetail(*effect, enchanter_->tier);
            }
            const std::array<const ResolvedEnchantment*, 1> individual{ effect };
            row.price = InventoryService::GetSingleton().Price(
                *enchanter_,
                { individual.data(), individual.size() });
            row.costDefined = true;
            row.confirm = false;
            row.picked = effect == first_ || effect == second_;
            row.enabled = !row.picked && !second_ && (!first_ || allowsTwo);
            row.action = row.picked ? "Selected" : "Select";
            row.activate = [this, effect, allowsTwo] {
                if (!first_) {
                    first_ = effect;
                } else if (allowsTwo && !second_ && effect != first_) {
                    second_ = effect;
                }
                ShowEffects();
            };
            page.rows.push_back(std::move(row));
        }
        if (first_) {
            const auto describe = [this](const ResolvedEnchantment& effect) {
                auto description = EffectDescription(effect, enchanter_->tier);
                return description.empty() ?
                    EffectDetail(effect, enchanter_->tier) :
                    description;
            };
            const auto individualPrice = [this](const ResolvedEnchantment* effect) {
                const std::array<const ResolvedEnchantment*, 1> chosen{ effect };
                return InventoryService::GetSingleton().Price(
                    *enchanter_,
                    { chosen.data(), chosen.size() });
            };

            page.finalizeLabel = "Craft";
            page.finalizeItem = selected_->currentName;
            page.selectionOneName = StripTrailingValueText(first_->definition->displayName);
            page.selectionOneEffect = describe(*first_);
            page.selectionOnePrice = individualPrice(first_);
            if (second_) {
                page.selectionTwoName = StripTrailingValueText(second_->definition->displayName);
                page.selectionTwoEffect = describe(*second_);
                page.selectionTwoPrice = individualPrice(second_);
            }

            const std::array<const ResolvedEnchantment*, 2> chosen{
                first_,
                second_
            };
            const auto count = second_ ? std::size_t{ 2 } : std::size_t{ 1 };
            page.finalizePrice = InventoryService::GetSingleton().Price(
                *enchanter_,
                { chosen.data(), count });
            page.canFinalize = page.finalizePrice > 0;
            page.finalize = [this, chosen, count](std::string customName) {
                const auto result = InventoryService::GetSingleton().ApplyEnchanting(
                    *enchanter_,
                    *selected_,
                    { chosen.data(), count },
                    customName);
                if (result.success) {
                    selected_.reset();
                    first_ = nullptr;
                    second_ = nullptr;
                    ShowItems(result.message);
                } else {
                    ShowEffects(result.message);
                }
            };
        }
        if (page.rows.empty() && page.notice.empty()) page.notice = "No compatible effects.";
        AOS::StableSortByDisplayLabel(page.rows, [](const auto& row) { return row.name; });
        AOS::ServiceMenu::Present(std::move(page));
    }
}
