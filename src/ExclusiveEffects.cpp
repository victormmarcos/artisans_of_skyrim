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

#include "ArtisansOfSkyrim/ExclusiveEffects.h"

#include <MinHook.h>

#ifdef max
#    undef max
#endif
#ifdef min
#    undef min
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace
{
    constexpr std::string_view kPlugin = "ArtisansOfSkyrim.esp";

    using AdvanceMovie = void(*)(
        RE::CraftingMenu*,
        float,
        std::uint32_t);
    using DisenchantRun = void(*)(
        RE::CraftingSubMenus::EnchantConstructMenu::EnchantMenuDisenchantCallback*,
        std::uint8_t);
    using PlayerUpdate = void(*)(RE::PlayerCharacter*, float);
    using HitDataPopulate = AOS::ExclusiveEffects::HitDataPopulate;

    REL::Relocation<AdvanceMovie> g_originalAdvanceMovie;
    REL::Relocation<DisenchantRun> g_originalDisenchantRun;
    REL::Relocation<PlayerUpdate> g_originalPlayerUpdate;
    HitDataPopulate g_originalHitDataPopulate{ nullptr };
    std::atomic_bool g_loggedWeaponPipeline{ false };
    std::atomic_bool g_loggedExclusiveWeaponDamage{ false };

    [[nodiscard]] RE::EnchantmentItem* EntryEnchantment(
        RE::InventoryEntryData* a_entry)
    {
        return a_entry ? a_entry->GetEnchantment() : nullptr;
    }

    [[nodiscard]] bool IsProtectedEntry(RE::InventoryEntryData* a_entry)
    {
        return AOS::ExclusiveEffects::GetSingleton().IsExclusive(
            EntryEnchantment(a_entry));
    }

    [[nodiscard]] bool FilterDisenchantList(
        RE::CraftingSubMenus::EnchantConstructMenu* a_menu)
    {
        using Menu = RE::CraftingSubMenus::EnchantConstructMenu;
        if (!a_menu || a_menu->currentCategory != Menu::Category::Disenchant) {
            return false;
        }

        bool hidProtectedEntry = false;
        for (auto& entry : a_menu->listEntries) {
            auto* raw = entry.get();
            const bool disenchantEntry = raw &&
                raw->filterFlag.any(
                    Menu::FilterFlag::DisenchantWeapon,
                    Menu::FilterFlag::DisenchantArmor);
            if (disenchantEntry) {
                const auto* item = static_cast<const Menu::ItemChangeEntry*>(raw);
                if (item && IsProtectedEntry(item->data)) {
                    // listEntries is the master array used by Scaleform's
                    // parallel inventory lists. Preserve its positions and
                    // remove only this entry's disenchant eligibility; erasing
                    // the entry would shift row metadata away from its name.
                    raw->filterFlag.reset(
                        Menu::FilterFlag::DisenchantWeapon,
                        Menu::FilterFlag::DisenchantArmor);
                    hidProtectedEntry = true;
                }
            }
        }
        return hidProtectedEntry;
    }

    void AdvanceMovieHook(
        RE::CraftingMenu* a_menu,
        float a_interval,
        std::uint32_t a_currentTime)
    {
        // Skyrim can populate and publish the initial list during
        // CraftingMenu::AdvanceMovie. Let that work finish before filtering;
        // otherwise the native array changes without its Scaleform view being
        // redrawn, leaving protected items visible until the player changes
        // category.
        g_originalAdvanceMovie(a_menu, a_interval, a_currentTime);

        if (a_menu) {
            const auto subMenu = a_menu->GetCraftingSubMenu();
            const auto enchantMenu = subMenu ?
                skyrim_cast<RE::CraftingSubMenus::EnchantConstructMenu*>(subMenu) :
                nullptr;
            if (FilterDisenchantList(enchantMenu)) {
                // Update only after eligibility changed. Subsequent frames do
                // no extra Scaleform work because the protected entries no
                // longer carry either disenchant filter flag.
                enchantMenu->UpdateInterface();
            }
        }
    }

    void DisenchantRunHook(
        RE::CraftingSubMenus::EnchantConstructMenu::EnchantMenuDisenchantCallback* a_callback,
        std::uint8_t a_message)
    {
        const auto menu = a_callback ? a_callback->subMenu : nullptr;
        const auto selected = menu && menu->selected.item ?
            menu->selected.item->data :
            nullptr;
        if (a_message == 0 &&
            IsProtectedEntry(selected)) {
            RE::SendHUDMessage::ShowHUDMessage(
                "This Artisans of Skyrim enchantment cannot be disenchanted.");
            SKSE::log::warn(
                "Blocked a final disenchant action for an exclusive item instance.");
            return;
        }
        g_originalDisenchantRun(a_callback, a_message);
    }

    void PlayerUpdateHook(RE::PlayerCharacter* a_player, float a_delta)
    {
        auto& exclusiveEffects = AOS::ExclusiveEffects::GetSingleton();
        const auto values = a_player ? a_player->AsActorValueOwner() : nullptr;
        const auto state = a_player ? a_player->AsActorState() : nullptr;
        const bool refundSprint = values && state &&
            exclusiveEffects.UnburdenActive() &&
            state->IsSprinting() &&
            state->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone;
        const float before = refundSprint ?
            values->GetActorValue(RE::ActorValue::kStamina) :
            0.0F;

        g_originalPlayerUpdate(a_player, a_delta);

        if (refundSprint && state->IsSprinting() &&
            state->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone) {
            const float after =
                values->GetActorValue(RE::ActorValue::kStamina);
            if (after < before) {
                const auto refund = AOS::SprintStaminaRefund(
                    before - after,
                    a_delta,
                    exclusiveEffects.UnburdenStaminaReduction(),
                    exclusiveEffects.SprintDrainMultiplier(),
                    exclusiveEffects.SprintWeightBase());
                if (refund > 0.0F) {
                    values->RestoreActorValue(
                        RE::ActorValue::kStamina,
                        refund);
                }
            }
        }
    }

    void HitDataPopulateHook(
        RE::HitData* a_hitData,
        RE::Actor* a_aggressor,
        RE::Actor* a_target,
        RE::InventoryEntryData* a_weapon,
        bool a_isLeftHand)
    {
        AOS::ExclusiveEffects::GetSingleton().RunWeaponHit(
            a_hitData,
            a_aggressor,
            a_target,
            a_weapon,
            a_isLeftHand,
            g_originalHitDataPopulate);
    }

    [[nodiscard]] bool IsWorn(const RE::ExtraDataList* a_extra)
    {
        return a_extra &&
            (a_extra->HasType<RE::ExtraWorn>() ||
             a_extra->HasType<RE::ExtraWornLeft>());
    }

}

namespace AOS
{
    ExclusiveEffects& ExclusiveEffects::GetSingleton()
    {
        static ExclusiveEffects singleton;
        return singleton;
    }

    void ExclusiveEffects::ResolveForms()
    {
        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        const auto lookupMgef = [&](RE::FormID a_local) {
            const auto form = dataHandler ?
                dataHandler->LookupForm<RE::EffectSetting>(a_local, kPlugin) :
                nullptr;
            if (!form) {
                SKSE::log::error(
                    "Exclusive MGEF {}|{:06X} could not be resolved.",
                    kPlugin,
                    a_local);
            }
            return form;
        };
        const auto lookupPerk = [&](RE::FormID a_local) {
            const auto form = dataHandler ?
                dataHandler->LookupForm<RE::BGSPerk>(a_local, kPlugin) :
                nullptr;
            if (!form) {
                SKSE::log::error(
                    "Exclusive PERK {}|{:06X} could not be resolved.",
                    kPlugin,
                    a_local);
            }
            return form;
        };
        const auto lookupRace = [&dataHandler](
                                    RE::FormID a_local,
                                    std::string_view a_plugin) {
            const auto form = dataHandler ?
                dataHandler->LookupForm<RE::TESRace>(a_local, a_plugin) :
                nullptr;
            if (!form) {
                SKSE::log::error(
                    "Silver Blessing target race {}|{:06X} could not be resolved.",
                    a_plugin,
                    a_local);
            }
            return form;
        };

        silverExpertMarker_ = lookupMgef(0x0F00);
        elementalMarker_ = lookupMgef(0x0F01);
        berserkerMarker_ = lookupMgef(0x0F02);
        honedMarker_ = lookupMgef(0x0F03);
        unburdenMarker_ = lookupMgef(0x0F04);
        reinforcementMarker_ = lookupMgef(0x0F05);
        silverMasterMarker_ = lookupMgef(0x0F18);

        unburdenMasterPerk_ = lookupPerk(0x0F12);
        reinforcementMasterPerk_ = lookupPerk(0x0F13);
        elementalExpertPerk_ = lookupPerk(0x0F19);
        elementalMasterPerk_ = lookupPerk(0x0F1A);
        unburdenExpertPerk_ = lookupPerk(0x0F2B);
        reinforcementExpertPerk_ = lookupPerk(0x0F2C);
        elementalCombinedPerk_ = lookupPerk(0x0F2D);
        elementalMaximumPerk_ = lookupPerk(0x0F2E);

        werewolfRace_ = lookupRace(0x0CDD84, "Skyrim.esm");
        werebearRace_ = lookupRace(0x01E17B, "Dragonborn.esm");

        const auto settings = RE::GameSettingCollection::GetSingleton();
        const auto readSetting = [&](const char* a_name) {
            const auto setting = settings ? settings->GetSetting(a_name) : nullptr;
            if (!setting || setting->GetType() != RE::Setting::Type::kFloat) {
                SKSE::log::error(
                    "Required sprint Game Setting '{}' could not be resolved.",
                    a_name);
                return 0.0F;
            }
            return setting->GetFloat();
        };
        sprintDrainMultiplier_ = readSetting("fSprintStaminaDrainMult");
        sprintWeightBase_ = readSetting("fSprintStaminaWeightBase");
        if (const auto setting = settings ?
                settings->GetSetting("fArmorScalingFactor") : nullptr;
            setting && setting->GetType() == RE::Setting::Type::kFloat &&
            setting->GetFloat() > 0.0F) {
            armorScalingFactor_ = setting->GetFloat();
        }
        if (const auto setting = settings ?
                settings->GetSetting("fMaxArmorRating") : nullptr;
            setting && setting->GetType() == RE::Setting::Type::kFloat &&
            setting->GetFloat() > 0.0F) {
            maximumArmorReduction_ = setting->GetFloat();
        }
    }

    void ExclusiveEffects::Initialize()
    {
        std::scoped_lock lock{ mutex_ };
        if (!initialized_) {
            ResolveForms();
            initialized_ = true;
        }
        SKSE::log::info(
            "Exclusive runtime support: active; resolved forms: {}/15, "
            "Silver target races: {}/2, sprint settings: multiplier {:.3f}, "
            "weight base {:.3f}, armor settings: scale {:.3f}, cap {:.3f}.",
            ResolvedFormCount(),
            static_cast<int>(werewolfRace_ != nullptr) +
                static_cast<int>(werebearRace_ != nullptr),
            sprintDrainMultiplier_,
            sprintWeightBase_,
            armorScalingFactor_,
            maximumArmorReduction_);
    }

    void ExclusiveEffects::ActivateRuntime()
    {
        std::scoped_lock lock{ mutex_ };
        if (!initialized_) {
            return;
        }
        if (!eventsRegistered_) {
            if (const auto events = RE::ScriptEventSourceHolder::GetSingleton()) {
                events->AddEventSink<RE::TESEquipEvent>(this);
                eventsRegistered_ = true;
            }
        }
    }

    std::size_t ExclusiveEffects::ResolvedFormCount() const noexcept
    {
        const std::array forms{
            static_cast<const RE::TESForm*>(silverExpertMarker_),
            static_cast<const RE::TESForm*>(silverMasterMarker_),
            static_cast<const RE::TESForm*>(elementalMarker_),
            static_cast<const RE::TESForm*>(berserkerMarker_),
            static_cast<const RE::TESForm*>(honedMarker_),
            static_cast<const RE::TESForm*>(unburdenMarker_),
            static_cast<const RE::TESForm*>(reinforcementMarker_),
            static_cast<const RE::TESForm*>(unburdenExpertPerk_),
            static_cast<const RE::TESForm*>(unburdenMasterPerk_),
            static_cast<const RE::TESForm*>(reinforcementExpertPerk_),
            static_cast<const RE::TESForm*>(reinforcementMasterPerk_),
            static_cast<const RE::TESForm*>(elementalExpertPerk_),
            static_cast<const RE::TESForm*>(elementalMasterPerk_),
            static_cast<const RE::TESForm*>(elementalCombinedPerk_),
            static_cast<const RE::TESForm*>(elementalMaximumPerk_),
        };
        return static_cast<std::size_t>(std::ranges::count_if(
            forms,
            [](const RE::TESForm* a_form) { return a_form != nullptr; }));
    }

    float ExclusiveEffects::MarkerMagnitude(
        const RE::EnchantmentItem* a_enchantment,
        const RE::EffectSetting* a_marker) const
    {
        if (!a_enchantment || !a_marker) {
            return 0.0F;
        }
        float result = 0.0F;
        for (const auto effect : a_enchantment->effects) {
            if (effect && effect->baseEffect == a_marker) {
                result = std::max(result, effect->effectItem.magnitude);
            }
        }
        return result;
    }

    bool ExclusiveEffects::IsExclusive(
        const RE::EnchantmentItem* a_enchantment) const
    {
        if (!a_enchantment) {
            return false;
        }
        const std::array markers{
            silverExpertMarker_,
            silverMasterMarker_,
            elementalMarker_,
            berserkerMarker_,
            honedMarker_,
            unburdenMarker_,
            reinforcementMarker_,
        };
        return std::ranges::any_of(markers, [&](const RE::EffectSetting* a_marker) {
            return MarkerMagnitude(a_enchantment, a_marker) > 0.0F;
        });
    }

    void ExclusiveEffects::SetTemporaryPerk(
        RE::PlayerCharacter* a_player,
        RE::BGSPerk* a_perk,
        bool a_enabled)
    {
        if (!a_player || !a_perk) {
            return;
        }
        const bool engineActive = a_player->HasPerk(a_perk);
        if (a_enabled) {
            if (!engineActive) {
                a_player->ApplyTemporaryPerk(a_perk);
            }
            activeTemporaryPerks_.insert(a_perk);
        } else if (engineActive || activeTemporaryPerks_.contains(a_perk)) {
            RemoveTemporaryPerkCompletely(a_player, a_perk);
        }
    }

    void ExclusiveEffects::RemoveTemporaryPerkCompletely(
        RE::PlayerCharacter* a_player,
        RE::BGSPerk* a_perk)
    {
        if (!a_player || !a_perk) {
            return;
        }

        // ApplyTemporaryPerk entries can survive a load independently of the
        // DLL's in-memory bookkeeping. Remove every retained copy so a perk
        // saved while equipped cannot be multiplied again on the next load.
        constexpr std::size_t kMaximumRetainedCopies = 64;
        for (std::size_t index = 0; index < kMaximumRetainedCopies; ++index) {
            const bool present = a_player->HasPerk(a_perk);
            if (!present && index > 0) {
                break;
            }
            a_player->RemoveTemporaryPerk(a_perk);
            if (!present) {
                break;
            }
        }
        activeTemporaryPerks_.erase(a_perk);
    }

    void ExclusiveEffects::ClearRuntimePerks(RE::PlayerCharacter* a_player)
    {
        const std::array apparelPerks{
            unburdenExpertPerk_,
            unburdenMasterPerk_,
            reinforcementExpertPerk_,
            reinforcementMasterPerk_,
            elementalExpertPerk_,
            elementalMasterPerk_,
            elementalCombinedPerk_,
            elementalMaximumPerk_,
        };
        for (const auto perk : apparelPerks) {
            RemoveTemporaryPerkCompletely(a_player, perk);
        }
        activeTemporaryPerks_.clear();
        unburdenStaminaReduction_ = 0.0F;
    }

    void ExclusiveEffects::SetTieredPerks(
        RE::PlayerCharacter* a_player,
        RE::BGSPerk* a_expert,
        RE::BGSPerk* a_master,
        EffectTier a_tier)
    {
        SetTemporaryPerk(
            a_player,
            a_expert,
            a_tier == EffectTier::kExpert);
        SetTemporaryPerk(
            a_player,
            a_master,
            a_tier == EffectTier::kMaster);
    }

    bool ExclusiveEffects::IsSilverBlessingTarget(
        RE::Actor* a_target) const
    {
        if (!a_target) {
            return false;
        }
        const auto race = a_target->GetRace();
        return race == werewolfRace_ || race == werebearRace_ ||
            a_target->HasKeywordString("ActorTypeUndead") ||
            a_target->HasKeywordString("ActorTypeGhost");
    }

    void ExclusiveEffects::RunWeaponHit(
        RE::HitData* a_hitData,
        RE::Actor* a_aggressor,
        RE::Actor* a_target,
        RE::InventoryEntryData* a_weapon,
        bool a_isLeftHand,
        HitDataPopulate a_original)
    {
        if (!a_original) {
            return;
        }
        a_original(a_hitData, a_aggressor, a_target, a_weapon, a_isLeftHand);
        if (!g_loggedWeaponPipeline.exchange(true)) {
            SKSE::log::info("Exact-weapon hit detour received its first hit.");
        }
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!initialized_ || !player || !a_hitData ||
            a_aggressor != player || !a_target || !a_weapon) {
            return;
        }

        const auto enchantment = EntryEnchantment(a_weapon);
        const auto charge = a_weapon->GetEnchantmentCharge();
        if (!enchantment || !charge || *charge <= 0.0) {
            return;
        }

        const bool silverMaster =
            MarkerMagnitude(enchantment, silverMasterMarker_) > 0.0F;
        const bool silverExpert =
            MarkerMagnitude(enchantment, silverExpertMarker_) > 0.0F;
        const auto silverTier = silverMaster ? EffectTier::kMaster :
            (silverExpert ? EffectTier::kExpert : EffectTier::kNone);
        const auto berserkerTier = SelectTier(
            MarkerMagnitude(enchantment, berserkerMarker_),
            kBerserkerSoulMagnitudes);
        const auto honedTier = SelectTier(
            MarkerMagnitude(enchantment, honedMarker_),
            kHonedMetalMagnitudes);

        float damageMultiplier = 1.0F;
        if (honedTier != EffectTier::kNone) {
            damageMultiplier *= PercentageMultiplier(
                honedTier == EffectTier::kMaster ?
                    kHonedMetalMagnitudes.master :
                    kHonedMetalMagnitudes.expert);
        }
        if (silverTier != EffectTier::kNone &&
            IsSilverBlessingTarget(a_target)) {
            damageMultiplier *= PercentageMultiplier(
                silverTier == EffectTier::kMaster ?
                    kSilverBlessingMagnitudes.master :
                    kSilverBlessingMagnitudes.expert);
        }
        if (berserkerTier != EffectTier::kNone) {
            const auto values = a_target->AsActorValueOwner();
            const auto armorRating = values ?
                values->GetActorValue(RE::ActorValue::kDamageResist) : 0.0F;
            damageMultiplier *= ArmorBypassDamageMultiplier(
                armorRating,
                berserkerTier == EffectTier::kMaster ?
                    kBerserkerSoulMagnitudes.master :
                    kBerserkerSoulMagnitudes.expert,
                armorScalingFactor_,
                maximumArmorReduction_);
        }

        if (damageMultiplier > 1.0F &&
            std::isfinite(damageMultiplier) &&
            a_hitData->totalDamage > 0.0F &&
            std::isfinite(a_hitData->totalDamage)) {
            const auto originalDamage = a_hitData->totalDamage;
            a_hitData->totalDamage = originalDamage * damageMultiplier;
            if (!g_loggedExclusiveWeaponDamage.exchange(true)) {
                SKSE::log::info(
                    "Applied direct exclusive weapon damage on the exact hit "
                    "instance: {:.3f} -> {:.3f} (x{:.3f}).",
                    originalDamage,
                    a_hitData->totalDamage,
                    damageMultiplier);
            }
        }
    }

    void ExclusiveEffects::RefreshArmorEffects()
    {
        std::scoped_lock lock{ mutex_ };
        refreshQueued_ = false;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!initialized_ || !player) {
            return;
        }

        float elementalMagnitude = 0.0F;
        float unburdenMagnitude = 0.0F;
        float reinforcementMagnitude = 0.0F;

        const auto inventory = player->GetInventory([](RE::TESBoundObject& a_object) {
            return a_object.Is(RE::FormType::Armor);
        });
        for (const auto& [object, inventoryData] : inventory) {
            const auto& data = inventoryData.second;
            if (!object || !data || !data->extraLists) {
                continue;
            }
            for (const auto extra : *data->extraLists) {
                if (!IsWorn(extra)) {
                    continue;
                }
                const auto extraEnchantment =
                    extra->GetByType<RE::ExtraEnchantment>();
                const auto enchantment = extraEnchantment ?
                    extraEnchantment->enchantment :
                    nullptr;
                elementalMagnitude +=
                    MarkerMagnitude(enchantment, elementalMarker_);
                unburdenMagnitude = std::max(
                    unburdenMagnitude,
                    MarkerMagnitude(enchantment, unburdenMarker_));
                reinforcementMagnitude = std::max(
                    reinforcementMagnitude,
                    MarkerMagnitude(enchantment, reinforcementMarker_));
            }
        }

        const auto elementalBonus = ElementalBurstBonus(elementalMagnitude);
        SetTemporaryPerk(player, elementalExpertPerk_, elementalBonus == 25.0F);
        SetTemporaryPerk(player, elementalMasterPerk_, elementalBonus == 50.0F);
        SetTemporaryPerk(player, elementalCombinedPerk_, elementalBonus == 75.0F);
        SetTemporaryPerk(player, elementalMaximumPerk_, elementalBonus == 100.0F);

        SetTieredPerks(
            player,
            reinforcementExpertPerk_,
            reinforcementMasterPerk_,
            SelectTier(
                reinforcementMagnitude,
                kReinforcementMagnitudes));
        SetTieredPerks(
            player,
            unburdenExpertPerk_,
            unburdenMasterPerk_,
            SelectTier(unburdenMagnitude, kUnburdenMagnitudes));
        unburdenStaminaReduction_ = std::clamp(
            unburdenMagnitude / 100.0F,
            0.0F,
            0.30F);
    }

    void ExclusiveEffects::QueueArmorRefresh()
    {
        if (!initialized_) {
            return;
        }
        if (refreshQueued_.exchange(true)) {
            return;
        }
        const auto tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            refreshQueued_ = false;
            return;
        }
        tasks->AddTask([] {
            ExclusiveEffects::GetSingleton().RefreshArmorEffects();
        });
    }

    void ExclusiveEffects::ClearForLoad()
    {
        std::scoped_lock lock{ mutex_ };
        if (!initialized_) {
            return;
        }
        ClearRuntimePerks(RE::PlayerCharacter::GetSingleton());
    }

    void ExclusiveEffects::ResetForLoadedGame()
    {
        std::scoped_lock lock{ mutex_ };
        if (!initialized_) {
            return;
        }
        ClearRuntimePerks(RE::PlayerCharacter::GetSingleton());
        QueueArmorRefresh();
    }

    RE::BSEventNotifyControl ExclusiveEffects::ProcessEvent(
        const RE::TESEquipEvent* a_event,
        RE::BSTEventSource<RE::TESEquipEvent>*)
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (a_event && player && a_event->actor.get() == player) {
            const auto object =
                RE::TESForm::LookupByID<RE::TESBoundObject>(
                    a_event->baseObject);
            if (!object || object->Is(RE::FormType::Armor)) {
                QueueArmorRefresh();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    void InstallExclusiveDisenchantHooks()
    {
        REL::Relocation<std::uintptr_t> craftingMenuVtable{
            RE::VTABLE_CraftingMenu[0]
        };
        g_originalAdvanceMovie = craftingMenuVtable.write_vfunc(
            5,
            AdvanceMovieHook);

        REL::Relocation<std::uintptr_t> callbackVtable{
            RE::VTABLE_CraftingSubMenus__EnchantConstructMenu__EnchantMenuDisenchantCallback[0]
        };
        g_originalDisenchantRun = callbackVtable.write_vfunc(
            1,
            DisenchantRunHook);

        SKSE::log::info(
            "Installed exclusive enchantment disenchant guards.");
    }

    void InstallExclusiveRuntimeHooks()
    {
        static bool hitHookInstalled = false;
        static bool playerHookInstalled = false;

        if (!hitHookInstalled) {
            REL::Relocation<HitDataPopulate> hitDataPopulate{
                RELOCATION_ID(42832, 44001)
            };
            const auto initializeStatus = MH_Initialize();
            if (initializeStatus != MH_OK &&
                initializeStatus != MH_ERROR_ALREADY_INITIALIZED) {
                SKSE::log::critical(
                    "Could not initialize the exact-weapon detour: {}.",
                    MH_StatusToString(initializeStatus));
            } else {
                auto original = static_cast<void*>(nullptr);
                const auto target = reinterpret_cast<void*>(
                    hitDataPopulate.address());
                const auto createStatus = MH_CreateHook(
                    target,
                    reinterpret_cast<void*>(HitDataPopulateHook),
                    std::addressof(original));
                if (createStatus != MH_OK) {
                    SKSE::log::critical(
                        "Could not create the exact-weapon hit detour: {}.",
                        MH_StatusToString(createStatus));
                } else {
                    g_originalHitDataPopulate =
                        reinterpret_cast<HitDataPopulate>(original);
                    const auto enableStatus = MH_EnableHook(target);
                    if (enableStatus != MH_OK &&
                        enableStatus != MH_ERROR_ENABLED) {
                        g_originalHitDataPopulate = nullptr;
                        MH_RemoveHook(target);
                        SKSE::log::critical(
                            "Could not enable the exact-weapon hit detour: {}.",
                            MH_StatusToString(enableStatus));
                    } else {
                        hitHookInstalled = true;
                        SKSE::log::info(
                            "Installed the exact-weapon HitData::Populate detour.");
                    }
                }
            }
        }

        if (!playerHookInstalled) {
            REL::Relocation<std::uintptr_t> playerVtable{
                RE::VTABLE_PlayerCharacter[0]
            };
            g_originalPlayerUpdate = playerVtable.write_vfunc(
                0xAD,
                PlayerUpdateHook);
            playerHookInstalled = true;
            SKSE::log::info(
                "Installed Unburden's bounded vanilla sprint-cost hook. Weapon "
                "effects use direct damage on the exact InventoryEntryData for "
                "each hit.");
        }
    }
}
