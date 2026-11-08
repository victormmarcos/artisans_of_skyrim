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

#include "ArtisansOfSkyrim/ExclusiveEffectRules.h"

#include "RE/Skyrim.h"

#include <atomic>
#include <mutex>
#include <type_traits>
#include <unordered_set>

namespace AOS
{
    class ExclusiveEffects final :
        public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        using HitDataPopulate = void(*)(
            RE::HitData*,
            RE::Actor*,
            RE::Actor*,
            RE::InventoryEntryData*,
            bool);
        static_assert(std::is_same_v<
            decltype(&RE::HitData::Populate),
            void (RE::HitData::*)(RE::Actor*, RE::Actor*, RE::InventoryEntryData*, bool)>);

        static ExclusiveEffects& GetSingleton();

        void Initialize();
        void ActivateRuntime();
        void RefreshArmorEffects();
        void QueueArmorRefresh();
        void ClearForLoad();
        void ResetForLoadedGame();
        void RunWeaponHit(
            RE::HitData* a_hitData,
            RE::Actor* a_aggressor,
            RE::Actor* a_target,
            RE::InventoryEntryData* a_weapon,
            bool a_isLeftHand,
            HitDataPopulate a_original);

        [[nodiscard]] bool IsExclusive(
            const RE::EnchantmentItem* a_enchantment) const;
        [[nodiscard]] std::size_t ResolvedFormCount() const noexcept;
        [[nodiscard]] bool UnburdenActive() const noexcept
        {
            return unburdenStaminaReduction_.load() > 0.0F;
        }
        [[nodiscard]] float UnburdenStaminaReduction() const noexcept
        {
            return unburdenStaminaReduction_.load();
        }
        [[nodiscard]] float SprintDrainMultiplier() const noexcept
        {
            return sprintDrainMultiplier_;
        }
        [[nodiscard]] float SprintWeightBase() const noexcept
        {
            return sprintWeightBase_;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESEquipEvent* a_event,
            RE::BSTEventSource<RE::TESEquipEvent>* a_source) override;

    private:
        void ResolveForms();
        void SetTemporaryPerk(
            RE::PlayerCharacter* a_player,
            RE::BGSPerk* a_perk,
            bool a_enabled);
        void RemoveTemporaryPerkCompletely(
            RE::PlayerCharacter* a_player,
            RE::BGSPerk* a_perk);
        void ClearRuntimePerks(RE::PlayerCharacter* a_player);
        void SetTieredPerks(
            RE::PlayerCharacter* a_player,
            RE::BGSPerk* a_expert,
            RE::BGSPerk* a_master,
            EffectTier a_tier);
        [[nodiscard]] bool IsSilverBlessingTarget(
            RE::Actor* a_target) const;
        [[nodiscard]] float MarkerMagnitude(
            const RE::EnchantmentItem* a_enchantment,
            const RE::EffectSetting* a_marker) const;

        bool initialized_{ false };
        bool eventsRegistered_{ false };
        std::atomic_bool refreshQueued_{ false };
        std::atomic<float> unburdenStaminaReduction_{ 0.0F };
        float sprintDrainMultiplier_{ 0.0F };
        float sprintWeightBase_{ 0.0F };
        float armorScalingFactor_{ 0.12F };
        float maximumArmorReduction_{ 80.0F };
        mutable std::mutex mutex_;
        std::unordered_set<RE::BGSPerk*> activeTemporaryPerks_;

        RE::EffectSetting* silverExpertMarker_{ nullptr };
        RE::EffectSetting* silverMasterMarker_{ nullptr };
        RE::EffectSetting* elementalMarker_{ nullptr };
        RE::EffectSetting* berserkerMarker_{ nullptr };
        RE::EffectSetting* honedMarker_{ nullptr };
        RE::EffectSetting* unburdenMarker_{ nullptr };
        RE::EffectSetting* reinforcementMarker_{ nullptr };

        RE::BGSPerk* unburdenExpertPerk_{ nullptr };
        RE::BGSPerk* unburdenMasterPerk_{ nullptr };
        RE::BGSPerk* reinforcementExpertPerk_{ nullptr };
        RE::BGSPerk* reinforcementMasterPerk_{ nullptr };
        RE::BGSPerk* elementalExpertPerk_{ nullptr };
        RE::BGSPerk* elementalMasterPerk_{ nullptr };
        RE::BGSPerk* elementalCombinedPerk_{ nullptr };
        RE::BGSPerk* elementalMaximumPerk_{ nullptr };

        RE::TESRace* werewolfRace_{ nullptr };
        RE::TESRace* werebearRace_{ nullptr };
    };

    void InstallExclusiveDisenchantHooks();
    void InstallExclusiveRuntimeHooks();
}
