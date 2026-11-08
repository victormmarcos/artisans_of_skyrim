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

#include "ArtisansOfSkyrim/ServiceDiscount.h"

#include <algorithm>
#include <array>
#include <utility>

namespace AOS
{
    double PlayerServiceDiscount(const BarterPricing& a_pricing)
    {
        if (!a_pricing.enabled) {
            return 0.0;
        }

        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return 0.0;
        }

        double discount = 0.0;
        const auto actorValueOwner = player->AsActorValueOwner();

        if (actorValueOwner && a_pricing.includeSpeechSkill) {
            const auto speech = static_cast<double>(
                actorValueOwner->GetActorValue(RE::ActorValue::kSpeech));
            discount += std::max(0.0, speech - a_pricing.speechBaseline) *
                a_pricing.speechSkillDiscountPerPoint;
        }

        if (actorValueOwner && a_pricing.includeSpeechcraftModifiers) {
            // Fortify Barter/Fortify Speech effects generally land on the speech
            // modifier/power actor values. Treat their positive value as a percent
            // discount, then clamp the total below.
            const auto modifier = static_cast<double>(
                actorValueOwner->GetActorValue(RE::ActorValue::kSpeechcraftModifier));
            const auto powerModifier = static_cast<double>(
                actorValueOwner->GetActorValue(RE::ActorValue::kSpeechcraftPowerModifier));
            discount += std::max(0.0, modifier) / 100.0;
            discount += std::max(0.0, powerModifier) / 100.0;
        }

        if (a_pricing.includeHagglingPerks) {
            // Vanilla Haggling ranks 1-5; only the highest owned rank counts.
            static constexpr std::array<std::pair<RE::FormID, double>, 5> hagglingPerks{ {
                { 0x000BE128, 0.10 },
                { 0x000C07CE, 0.15 },
                { 0x000C07CF, 0.20 },
                { 0x000C07D0, 0.25 },
                { 0x000C07D1, 0.30 },
            } };

            double hagglingDiscount = 0.0;
            for (const auto& [formID, perkDiscount] : hagglingPerks) {
                const auto perk = RE::TESForm::LookupByID<RE::BGSPerk>(formID);
                if (perk && player->HasPerk(perk)) {
                    hagglingDiscount = std::max(hagglingDiscount, perkDiscount);
                }
            }
            discount += hagglingDiscount;
        }

        return std::clamp(discount, 0.0, a_pricing.maxDiscount);
    }
}
