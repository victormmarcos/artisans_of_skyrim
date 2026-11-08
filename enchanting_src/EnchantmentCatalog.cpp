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

#include "NPCEnchantingServices/EnchantmentCatalog.h"

#include "NPCEnchantingServices/FormResolver.h"

namespace
{
    [[nodiscard]] bool ApparelRestrictionAllows(
        const RE::TESObjectARMO* a_armor,
        const RE::EnchantmentItem* a_source)
    {
        if (!a_armor || !a_source || !a_source->data.wornRestrictions) {
            return false;
        }

        const auto restrictions = a_source->data.wornRestrictions;
        for (const auto keyword : a_armor->GetKeywords()) {
            if (keyword && restrictions->HasForm(keyword)) {
                return true;
            }
        }
        return false;
    }
}

namespace NPCES
{
    EnchantmentCatalog& EnchantmentCatalog::GetSingleton()
    {
        static EnchantmentCatalog singleton;
        return singleton;
    }

    void EnchantmentCatalog::Build(const Config& a_config)
    {
        enchantments_.clear();
        enchantments_.reserve(a_config.enchantments.size());
        const auto& resolver = FormResolver::GetSingleton();

        for (const auto& definition : a_config.enchantments) {
            if (definition.exclusive &&
                !a_config.enableCustomEnchantments) {
                continue;
            }
            if (definition.blocked) {
                SKSE::log::info(
                    "Enchantment '{}' is blocked by the balance table.",
                    definition.displayName);
                continue;
            }

            const auto form = resolver.Resolve(definition.sourceForm);
            const auto source = form ? form->As<RE::EnchantmentItem>() : nullptr;
            if (!source || source->effects.empty()) {
                SKSE::log::warn(
                    "Could not resolve usable source enchantment '{}' ({})",
                    definition.displayName,
                    definition.sourceForm);
                continue;
            }
            enchantments_.push_back({ std::addressof(definition), source });
        }

        SKSE::log::info(
            "Loaded {} usable enchantment definitions.",
            enchantments_.size());
    }

    std::vector<const ResolvedEnchantment*> EnchantmentCatalog::ListValid(
        RE::TESBoundObject* a_object,
        ItemCategory a_category,
        ServiceTier a_tier) const
    {
        std::vector<const ResolvedEnchantment*> result;
        for (const auto& enchantment : enchantments_) {
            if (IsValid(a_object, a_category, a_tier, enchantment)) {
                result.push_back(std::addressof(enchantment));
            }
        }
        return result;
    }

    bool EnchantmentCatalog::IsValid(
        RE::TESBoundObject* a_object,
        ItemCategory a_category,
        ServiceTier a_tier,
        const ResolvedEnchantment& a_enchantment) const
    {
        const auto definition = a_enchantment.definition;
        if (!a_object || !definition || !a_enchantment.source ||
            definition->blocked ||
            !TierAllows(a_tier, definition->minimumTier) ||
            !ValueFor(a_enchantment, a_tier).Available()) {
            return false;
        }

        if (a_category == ItemCategory::kWeapons) {
            if (definition->type != EnchantmentType::kWeapon) {
                return false;
            }
            const auto weapon = a_object->As<RE::TESObjectWEAP>();
            return weapon && !weapon->IsStaff() &&
                !weapon->IsHandToHandMelee();
        }

        if (definition->type != EnchantmentType::kApparel) {
            return false;
        }
        const auto armor = a_object->As<RE::TESObjectARMO>();
        if (!armor) {
            return false;
        }
        if (definition->id == "elemental_burst") {
            return armor->HasPartOf(
                       RE::BGSBipedObjectForm::BipedObjectSlot::kRing) ||
                armor->HasPartOf(
                       RE::BGSBipedObjectForm::BipedObjectSlot::kAmulet);
        }
        if (definition->id == "unburden" ||
            definition->id == "reinforcement") {
            return armor->HasPartOf(
                RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
        }
        return ApparelRestrictionAllows(armor, a_enchantment.source);
    }

    bool EnchantmentCatalog::BuildEffects(
        std::span<const ResolvedEnchantment* const> a_enchantments,
        ServiceTier a_tier,
        RE::BSTArray<RE::Effect>& a_effects) const
    {
        a_effects.clear();
        for (const auto enchantment : a_enchantments) {
            if (!enchantment || !enchantment->source ||
                !enchantment->definition) {
                return false;
            }

            const auto& value = ValueFor(*enchantment, a_tier);
            if (!value.Available()) {
                return false;
            }

            std::size_t effectIndex = 0;
            for (const auto sourceEffect : enchantment->source->effects) {
                if (!sourceEffect || !sourceEffect->baseEffect) {
                    SKSE::log::warn(
                        "Source enchantment '{}' has an unsupported effect.",
                        enchantment->definition->displayName);
                    return false;
                }

                // A vanilla ENCH may contain several internal effects and
                // conditions while still representing one selectable
                // enchantment. Frost Damage is the canonical example: its
                // damage and slowdown remain a single service choice. The
                // Master-only rule is therefore based on the number of
                // selected ENCH definitions, not this internal effect count.

                RE::Effect copy;
                copy.effectItem = sourceEffect->effectItem;
                copy.baseEffect = sourceEffect->baseEffect;
                copy.cost = sourceEffect->cost;
                if (effectIndex == 0) {
                    if (enchantment->definition->id == "silver_blessing" &&
                        a_tier == ServiceTier::kMaster) {
                        const auto dataHandler =
                            RE::TESDataHandler::GetSingleton();
                        const auto masterMarker = dataHandler ?
                            dataHandler->LookupForm<RE::EffectSetting>(
                                0x000F18,
                                "ArtisansOfSkyrim.esp") :
                            nullptr;
                        if (!masterMarker) {
                            SKSE::log::error(
                                "Silver Blessing Master marker could not be resolved.");
                            return false;
                        }
                        copy.baseEffect = masterMarker;
                    }
                    if (value.magnitude) {
                        copy.effectItem.magnitude = *value.magnitude;
                    }
                    if (value.duration) {
                        copy.effectItem.duration = *value.duration;
                    }
                    if (value.chargeCost) {
                        copy.cost = *value.chargeCost;
                    }
                }
                a_effects.push_back(std::move(copy));
                ++effectIndex;
            }
        }
        return !a_effects.empty();
    }

    const TierValue& EnchantmentCatalog::ValueFor(
        const ResolvedEnchantment& a_enchantment,
        ServiceTier a_tier) const
    {
        return a_enchantment.definition->values[TierIndex(a_tier)];
    }
}
