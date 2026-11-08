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

#include "NPCSmithServices/OrderService.h"

#include "NPCSmithServices/CourierService.h"
#include "NPCSmithServices/DurationFormatter.h"
#include "ArtisansOfSkyrim/Localization.h"
#include "NPCSmithServices/FormResolver.h"
#include "NPCSmithServices/SmithClassifier.h"

#include "ArtisansOfSkyrim/TransactionFeedback.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace
{
    constexpr std::uint32_t kSerializationID = 'AOSO';
    constexpr std::uint32_t kOrderRecordType = 'ORDR';
    constexpr std::uint32_t kOrderRecordVersion = 1;
    constexpr std::uint32_t kMaximumSerializedString = 4096;

    [[nodiscard]] bool TryRemoveGold(
        RE::PlayerCharacter* a_player,
        RE::TESBoundObject* a_gold,
        std::int32_t a_price)
    {
        if (!a_player || !a_gold || a_price <= 0 ||
            a_player->GetItemCount(a_gold) < a_price) {
            return false;
        }
        const auto before = a_player->GetItemCount(a_gold);
        a_player->RemoveItem(
            a_gold,
            a_price,
            RE::ITEM_REMOVE_REASON::kRemove,
            nullptr,
            nullptr);
        return before - a_player->GetItemCount(a_gold) == a_price;
    }

    [[nodiscard]] bool WriteString(
        SKSE::SerializationInterface* a_interface,
        const std::string& a_value)
    {
        const auto length = static_cast<std::uint32_t>(a_value.size());
        return a_interface->WriteRecordData(length) &&
            (length == 0 ||
             a_interface->WriteRecordData(a_value.data(), length));
    }

    template <class T>
    [[nodiscard]] bool ReadValue(
        SKSE::SerializationInterface* a_interface,
        T& a_value)
    {
        return a_interface->ReadRecordData(a_value) == sizeof(T);
    }

    [[nodiscard]] bool ReadString(
        SKSE::SerializationInterface* a_interface,
        std::string& a_value)
    {
        std::uint32_t length = 0;
        if (!ReadValue(a_interface, length) ||
            length > kMaximumSerializedString) {
            return false;
        }
        a_value.resize(length);
        return length == 0 ||
            a_interface->ReadRecordData(a_value.data(), length) == length;
    }

    [[nodiscard]] std::string FormPluginName(const RE::TESForm* a_form)
    {
        const auto file = a_form ? a_form->GetFile(0) : nullptr;
        return file ? std::string{ file->GetFilename() } : std::string{};
    }

    [[nodiscard]] std::string EscapeBookMarkup(std::string_view a_value)
    {
        std::string result;
        result.reserve(a_value.size());
        for (const auto character : a_value) {
            switch (character) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            default: result.push_back(character); break;
            }
        }
        return result;
    }

    [[nodiscard]] bool IsActiveStatus(
        NPCSS::CraftingOrderStatus a_status) noexcept
    {
        return a_status == NPCSS::CraftingOrderStatus::kPending ||
            a_status == NPCSS::CraftingOrderStatus::kReady;
    }

    [[nodiscard]] std::string ItemSummaryLine(
        const NPCSS::CraftingOrderRecord& a_order)
    {
        std::string result = a_order.displayName.empty() ?
            AOS::Text::Localize("Unknown item") :
            a_order.displayName;
        if (a_order.quantity > 1) {
            result += " x" + std::to_string(a_order.quantity);
        }
        return result;
    }

    [[nodiscard]] std::string LostOrdersMessage(
        const std::vector<NPCSS::CraftingOrderRecord>& a_orders)
    {
        std::string message =
            "A blacksmith handling your orders has died.\n\n"
            "The following orders were permanently lost:";
        for (const auto& order : a_orders) {
            message += "\n- " + ItemSummaryLine(order);
        }
        message += "\n\nNo refund is available.";
        return message;
    }
}

namespace NPCSS
{
    std::optional<CraftingOrderCategory>
    ClassifyCraftingOrderCategory(RE::TESBoundObject* a_object)
    {
        if (!a_object) {
            return std::nullopt;
        }
        if (const auto ammo = a_object->As<RE::TESAmmo>()) {
            return ammo->IsBolt() ?
                CraftingOrderCategory::kBolt :
                CraftingOrderCategory::kArrow;
        }
        if (const auto weapon = a_object->As<RE::TESObjectWEAP>()) {
            switch (weapon->GetWeaponType()) {
            case RE::WEAPON_TYPE::kOneHandDagger:
                return CraftingOrderCategory::kDagger;
            case RE::WEAPON_TYPE::kOneHandSword:
                return CraftingOrderCategory::kSword;
            case RE::WEAPON_TYPE::kOneHandAxe:
                return CraftingOrderCategory::kWarAxe;
            case RE::WEAPON_TYPE::kOneHandMace:
                return CraftingOrderCategory::kMace;
            case RE::WEAPON_TYPE::kTwoHandSword:
                return CraftingOrderCategory::kGreatsword;
            case RE::WEAPON_TYPE::kTwoHandAxe:
                if (FormResolver::HasExactKeyword(
                        weapon,
                        std::array<std::string, 1>{ "WeapTypeWarhammer" })) {
                    return CraftingOrderCategory::kWarhammer;
                }
                // Battleaxes, and mod two-handers that reuse the axe animation
                // (spears, halberds) without a battleaxe keyword: same size and time.
                return CraftingOrderCategory::kBattleaxe;
            case RE::WEAPON_TYPE::kBow:
                return CraftingOrderCategory::kBow;
            case RE::WEAPON_TYPE::kCrossbow:
                return CraftingOrderCategory::kCrossbow;
            default:
                return std::nullopt;
            }
        }
        if (const auto armor = a_object->As<RE::TESObjectARMO>()) {
            using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
            if (armor->IsShield() || armor->HasPartOf(Slot::kShield)) {
                return CraftingOrderCategory::kShield;
            }
            static const std::array<std::string, 1> helmetKeywords{
                "ArmorHelmet"
            };
            static const std::array<std::string, 1> jewelryTokens{
                "jewelry"
            };
            static const std::array<std::string, 1> circletTokens{
                "circlet"
            };
            const bool circletIdentity =
                FormResolver::HasKeywordToken(armor, jewelryTokens) ||
                FormResolver::HasTextToken(armor, circletTokens);
            if (IsSupportedHelmet(
                    FormResolver::HasExactKeyword(armor, helmetKeywords),
                    armor->IsLightArmor() || armor->IsHeavyArmor(),
                    armor->HasPartOf(Slot::kHead),
                    armor->HasPartOf(Slot::kHair),
                    armor->HasPartOf(Slot::kCirclet),
                    circletIdentity)) {
                return CraftingOrderCategory::kHelmet;
            }
            if (armor->HasPartOf(Slot::kFeet)) {
                return CraftingOrderCategory::kBoots;
            }
            if (armor->HasPartOf(Slot::kHands) ||
                armor->HasPartOf(Slot::kForearms)) {
                return CraftingOrderCategory::kGauntlets;
            }
            if (armor->HasPartOf(Slot::kBody)) {
                if (armor->IsLightArmor()) {
                    return CraftingOrderCategory::kLightBodyArmor;
                }
                if (armor->IsHeavyArmor()) {
                    return CraftingOrderCategory::kHeavyBodyArmor;
                }
            }
        }
        return std::nullopt;
    }

    ServiceComplexity ClassifyServiceComplexity(
        RE::TESBoundObject* a_object)
    {
        const auto category = ClassifyCraftingOrderCategory(a_object);
        return category ?
            ComplexityForCategory(*category) :
            ServiceComplexity::kMedium;
    }

    double CraftingDurationHours(
        CraftingOrderCategory a_category,
        const SmithingOrderDurations& a_durations) noexcept
    {
        switch (a_category) {
        case CraftingOrderCategory::kArrow:
        case CraftingOrderCategory::kBolt: return 0.0;
        case CraftingOrderCategory::kDagger: return a_durations.daggerHours;
        case CraftingOrderCategory::kSword: return a_durations.swordHours;
        case CraftingOrderCategory::kWarAxe: return a_durations.warAxeHours;
        case CraftingOrderCategory::kMace: return a_durations.maceHours;
        case CraftingOrderCategory::kBoots: return a_durations.bootsHours;
        case CraftingOrderCategory::kGauntlets: return a_durations.gauntletsHours;
        case CraftingOrderCategory::kBow: return a_durations.bowHours;
        case CraftingOrderCategory::kShield: return a_durations.shieldHours;
        case CraftingOrderCategory::kHelmet: return a_durations.helmetHours;
        case CraftingOrderCategory::kGreatsword: return a_durations.greatswordHours;
        case CraftingOrderCategory::kBattleaxe: return a_durations.battleaxeHours;
        case CraftingOrderCategory::kWarhammer: return a_durations.warhammerHours;
        case CraftingOrderCategory::kLightBodyArmor: return a_durations.lightBodyArmorHours;
        case CraftingOrderCategory::kHeavyBodyArmor: return a_durations.heavyBodyArmorHours;
        case CraftingOrderCategory::kCrossbow: return a_durations.crossbowHours;
        }
        return 0.0;
    }

    OrderService& OrderService::GetSingleton()
    {
        static OrderService singleton;
        return singleton;
    }

    void OrderService::Configure(const Config& a_config)
    {
        config_ = std::addressof(a_config);
        if (!config_->orders.enabled) {
            orders_.clear();
            CourierService::GetSingleton().Revert();
            return;
        }
        Update();
    }

    void OrderService::RegisterSerialization()
    {
        if (serializationRegistered_) {
            return;
        }
        const auto serialization = SKSE::GetSerializationInterface();
        if (!serialization) {
            SKSE::log::critical("SKSE serialization is unavailable for smithing orders.");
            return;
        }
        serialization->SetUniqueID(kSerializationID);
        serialization->SetSaveCallback(Save);
        serialization->SetLoadCallback(Load);
        serialization->SetRevertCallback(Revert);
        serializationRegistered_ = true;
    }

    void OrderService::RegisterEvents()
    {
        if (eventsRegistered_) {
            return;
        }
        const auto events = RE::ScriptEventSourceHolder::GetSingleton();
        if (!events) {
            SKSE::log::error("Game event source is unavailable for smithing orders.");
            return;
        }
        events->AddEventSink<RE::TESWaitStopEvent>(this);
        events->AddEventSink<RE::TESSleepStopEvent>(this);
        events->AddEventSink<RE::TESFastTravelEndEvent>(this);
        events->AddEventSink<RE::TESDeathEvent>(this);
        eventsRegistered_ = true;
    }

    RuntimeOrderPreview OrderService::Preview(
        const SmithContext& a_smith,
        const CraftableItem& a_item) const
    {
        RuntimeOrderPreview result;
        const auto actor = a_smith.actor.get();
        if (!config_ || !actor || !a_item.object) {
            result.error = "The blacksmith is no longer available.";
            return result;
        }
        const auto category = ClassifyCraftingOrderCategory(a_item.object);
        if (!category) {
            result.error = "This equipment has no supported crafting-time category.";
            return result;
        }
        result.category = *category;
        result.durationHours = CraftingDurationHours(
            *category,
            config_->orders.smithing);
        if (IsInstantOrderCategory(*category)) {
            result.valid = true;
            result.schedule.accepted = true;
            result.schedule.startGameHour = CurrentGameHour();
            result.schedule.readyGameHour = result.schedule.startGameHour;
            return result;
        }

        result.schedule = PreviewOrderSchedule(
            a_smith.workshopKey,
            CurrentGameHour(),
            result.durationHours,
            config_->orders.maximumActivePerSmith,
            Timeline());
        result.valid = result.schedule.accepted;
        if (!result.valid) {
            result.error = "This blacksmith already has five active orders for you.";
        }
        return result;
    }

    TransactionResult OrderService::PlaceOrder(
        const SmithContext& a_smith,
        const CraftableItem& a_item)
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto actor = a_smith.actor.get();
        if (!config_ || !config_->orders.enabled || !player || !actor ||
            !a_smith.servicesUnlocked || !a_item.object) {
            return { false, "The smithing order could not be placed." };
        }

        const auto available = RecipeIndex::GetSingleton().ListCraftable(a_smith);
        const auto current = std::ranges::find_if(
            available,
            [&](const CraftableItem& a_candidate) {
                return a_candidate.object == a_item.object &&
                    a_candidate.recipe == a_item.recipe;
            });
        if (current == available.end() || current->price <= 0 ||
            current->outputCount <= 0) {
            return { false, "This item is no longer available from this blacksmith." };
        }

        const auto preview = Preview(a_smith, *current);
        if (!preview.valid || IsInstantOrderCategory(preview.category)) {
            return {
                false,
                preview.error.empty() ?
                    "The smithing order could not be placed." :
                    preview.error
            };
        }

        const auto gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        if (!gold || player->GetItemCount(gold) < current->price) {
            return { false, "You do not have enough septims." };
        }

        CraftingOrderRecord record;
        record.uniqueOrderID = nextOrderID_++;
        record.smithFormID = actor->GetFormID();
        record.smithBaseFormID = a_smith.baseFormID;
        record.workshopKey = a_smith.workshopKey;
        record.itemFormID = current->object->GetFormID();
        record.itemPlugin = FormPluginName(current->object);
        record.itemLocalFormID = current->object->GetFile(0) ?
            current->object->GetLocalFormID() :
            current->object->GetFormID();
        record.quantity = current->outputCount;
        record.category = preview.category;
        record.displayName =
            FormResolver::GetSingleton().MenuLabelFor(current->object);
        record.smithDisplayName = a_smith.artisanDisplayName;
        record.pickupLocation = a_smith.courierLocation;
        record.price = current->price;
        record.placedGameHour = CurrentGameHour();
        record.startGameHour = preview.schedule.startGameHour;
        record.readyGameHour = preview.schedule.readyGameHour;
        if (const auto cell = actor->GetParentCell()) {
            record.pickupCellFormID = cell->GetFormID();
        }
        if (const auto worldspace = actor->GetWorldspace()) {
            record.pickupWorldspaceFormID = worldspace->GetFormID();
        }
        const auto position = actor->GetPosition();
        record.pickupX = position.x;
        record.pickupY = position.y;
        record.pickupZ = position.z;

        orders_.push_back(std::move(record));
        if (!TryRemoveGold(player, gold, current->price)) {
            orders_.pop_back();
            --nextOrderID_;
            return { false, "The payment could not be completed." };
        }

        const auto& saved = orders_.back();
        AOS::PlayPurchaseSound();
        SKSE::log::info(
            "Placed smithing order {} for {}x {:08X}; ready at game hour {:.3f}.",
            saved.uniqueOrderID,
            saved.quantity,
            saved.itemFormID,
            saved.readyGameHour);
        return {
            true,
            AOS::Text::Format("{} ordered. Estimated completion in {}.", saved.displayName,
                AOS::Text::Localize(FormatDurationHours(saved.readyGameHour - CurrentGameHour())))
        };
    }

    bool OrderService::HasActiveOrdersForSmith(const RE::Actor* a_smith) const
    {
        if (!config_ || !config_->orders.enabled || !a_smith) {
            return false;
        }
        const auto context = SmithClassifier::GetSingleton().Classify(
            const_cast<RE::Actor*>(a_smith));
        if (!context) {
            return false;
        }
        const auto workshopKey = context->workshopKey;
        return std::ranges::any_of(
            orders_,
            [workshopKey](const CraftingOrderRecord& a_order) {
                return a_order.workshopKey == workshopKey &&
                    (a_order.status == CraftingOrderStatus::kPending ||
                     a_order.status == CraftingOrderStatus::kReady);
            });
    }

    std::string OrderService::CollectOrders(RE::Actor* a_smith)
    {
        // Collection remains independent from the courier. Update readiness
        // first, deliver everything available, then decide whether any notice
        // still has something useful to report.
        Update(false);
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!config_ || !config_->orders.enabled || !a_smith || !player) {
            return "The orders could not be checked.";
        }
        const auto context = SmithClassifier::GetSingleton().Classify(a_smith);
        if (!context) {
            return "The orders could not be checked.";
        }
        const auto workshopKey = context->workshopKey;
        std::vector<std::string> delivered;
        std::vector<std::string> pending;

        for (auto& order : orders_) {
            if (order.workshopKey != workshopKey) {
                continue;
            }
            if (order.status == CraftingOrderStatus::kReady) {
                const auto object =
                    RE::TESForm::LookupByID<RE::TESBoundObject>(order.itemFormID);
                if (!object || order.quantity <= 0) {
                    order.status = CraftingOrderStatus::kInvalid;
                    continue;
                }
                const auto before = player->GetItemCount(object);
                player->AddObjectToContainer(
                    object,
                    nullptr,
                    order.quantity,
                    nullptr);
                const auto after = player->GetItemCount(object);
                if (after - before >= order.quantity) {
                    order.status = CraftingOrderStatus::kCollected;
                    delivered.push_back(ItemSummaryLine(order));
                } else {
                    pending.push_back(AOS::Text::Format(
                        "{} - collection failed; try again", ItemSummaryLine(order)));
                }
            } else if (order.status == CraftingOrderStatus::kPending) {
                pending.push_back(AOS::Text::Format(
                    "{} - {} remaining", order.displayName,
                    AOS::Text::Localize(FormatDurationHours(
                        order.readyGameHour - CurrentGameHour()))));
            }
        }

        std::erase_if(orders_, [](const CraftingOrderRecord& a_order) {
            return !IsActiveStatus(a_order.status);
        });

        std::string summary = "Smithing Orders";
        const auto append = [&](std::string_view a_heading,
                                const std::vector<std::string>& a_lines) {
            if (a_lines.empty()) {
                return;
            }
            summary += "\n\n" + std::string{ a_heading };
            for (const auto& line : a_lines) {
                summary += "\n- " + line;
            }
        };
        append("Collected", delivered);
        append("Still Pending", pending);
        if (delivered.empty() && pending.empty()) {
            summary += "\n\nYou have no orders with this blacksmith.";
        }
        DispatchCourierNotices();
        return summary;
    }

    void OrderService::Update(bool a_dispatchCouriers)
    {
        if (!config_ || !config_->orders.enabled) {
            orders_.clear();
            return;
        }

        const auto now = CurrentGameHour();
        std::vector<CraftingOrderRecord> newlyLost;
        const auto& classifier = SmithClassifier::GetSingleton();
        for (auto& order : orders_) {
            if (order.status != CraftingOrderStatus::kPending &&
                order.status != CraftingOrderStatus::kReady) {
                continue;
            }
            const auto item = RE::TESForm::LookupByID(order.itemFormID);
            if (!item) {
                order.status = CraftingOrderStatus::kInvalid;
                continue;
            }
            const auto dead = classifier.OrderOwnerIsDead(
                order.workshopKey,
                order.smithFormID);
            order.status = UpdatedOrderStatus(
                order.status,
                order.readyGameHour,
                now,
                dead);
            if (order.status == CraftingOrderStatus::kLost) {
                newlyLost.push_back(order);
            }
        }

        if (!newlyLost.empty()) {
            const auto message = AOS::Text::LocalizeMessage(LostOrdersMessage(newlyLost));
            RE::DebugMessageBox(message.c_str());
        }

        std::erase_if(orders_, [](const CraftingOrderRecord& a_order) {
            return !IsActiveStatus(a_order.status);
        });
        if (a_dispatchCouriers) {
            DispatchCourierNotices();
        }
    }

    void OrderService::DispatchCourierNotices()
    {
        std::unordered_set<RE::FormID> candidates;
        for (const auto& order : orders_) {
            if (order.status == CraftingOrderStatus::kReady &&
                !order.courierNotified) {
                candidates.insert(order.workshopKey);
            }
        }

        for (const auto workshopKey : candidates) {
            const bool hasPending = std::ranges::any_of(
                orders_,
                [workshopKey](const CraftingOrderRecord& a_order) {
                    return a_order.workshopKey == workshopKey &&
                        a_order.status == CraftingOrderStatus::kPending;
                });
            if (hasPending) {
                continue;
            }

            std::vector<CraftingOrderRecord*> ready;
            for (auto& order : orders_) {
                if (order.workshopKey == workshopKey &&
                    order.status == CraftingOrderStatus::kReady) {
                    ready.push_back(std::addressof(order));
                }
            }
            if (ready.empty()) {
                continue;
            }

            auto& representative = *ready.front();
            if (const auto artisan =
                    SmithClassifier::GetSingleton().ArtisanForWorkshop(
                        workshopKey,
                        representative.smithFormID)) {
                representative.smithDisplayName = artisan->displayName;
                representative.pickupLocation = artisan->courierLocation;
            }
            if (representative.smithDisplayName.empty()) {
                representative.smithDisplayName = AOS::Text::Localize("Blacksmith");
            }
            if (representative.pickupLocation.empty()) {
                representative.pickupLocation =
                    AOS::Text::Localize("forge where the order was placed");
            }
            for (auto* order : ready) {
                order->smithDisplayName = representative.smithDisplayName;
                order->pickupLocation = representative.pickupLocation;
                order->schemaVersion = 3;
            }

            std::string body =
                "<font face=\"$HandwrittenFont\">\n\n"
                + AOS::Text::Localize("Your smithing orders are ready for collection.") + "\n\n";
            if (representative.pickupLocation == "Fort Dawnguard") {
                body += AOS::Text::Localize("You may retrieve them at Fort Dawnguard.") + "\n\n";
            } else {
                body += AOS::Text::Format("You may retrieve them at the {}.",
                    EscapeBookMarkup(AOS::Text::Localize(representative.pickupLocation))) + "\n\n";
            }
            body += EscapeBookMarkup(representative.smithDisplayName) +
                "\n</font>";
            const auto title =
                AOS::Text::Format("Letter from {}", representative.smithDisplayName);
            const auto noticeID =
                CourierService::GetSingleton().QueueNotice(title, body);
            if (!noticeID) {
                continue;
            }
            for (auto* order : ready) {
                if (!order->courierNotified) {
                    order->courierNotified = true;
                    order->courierGeneration = *noticeID;
                }
            }
        }
    }

    RE::BSEventNotifyControl OrderService::ProcessEvent(
        const RE::TESWaitStopEvent*,
        RE::BSTEventSource<RE::TESWaitStopEvent>*)
    {
        Update();
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl OrderService::ProcessEvent(
        const RE::TESSleepStopEvent*,
        RE::BSTEventSource<RE::TESSleepStopEvent>*)
    {
        Update();
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl OrderService::ProcessEvent(
        const RE::TESFastTravelEndEvent*,
        RE::BSTEventSource<RE::TESFastTravelEndEvent>*)
    {
        Update();
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl OrderService::ProcessEvent(
        const RE::TESDeathEvent* a_event,
        RE::BSTEventSource<RE::TESDeathEvent>*)
    {
        if (a_event && a_event->dead && a_event->actorDying) {
            Update();
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    double OrderService::CurrentGameHour()
    {
        const auto calendar = RE::Calendar::GetSingleton();
        return calendar ?
            static_cast<double>(calendar->GetCurrentGameTime()) * 24.0 :
            0.0;
    }

    std::vector<OrderTimelineEntry> OrderService::Timeline() const
    {
        std::vector<OrderTimelineEntry> result;
        result.reserve(orders_.size());
        for (const auto& order : orders_) {
            result.push_back({
                order.workshopKey,
                order.status,
                order.readyGameHour
            });
        }
        return result;
    }

    void OrderService::Save(SKSE::SerializationInterface* a_interface)
    {
        GetSingleton().SaveImpl(a_interface);
    }

    void OrderService::Load(SKSE::SerializationInterface* a_interface)
    {
        GetSingleton().LoadImpl(a_interface);
    }

    void OrderService::Revert(SKSE::SerializationInterface*)
    {
        GetSingleton().RevertImpl();
    }

    void OrderService::SaveImpl(
        SKSE::SerializationInterface* a_interface) const
    {
        if (!config_ || !config_->orders.enabled) {
            return;
        }
        if (!a_interface ||
            !a_interface->OpenRecord(kOrderRecordType, kOrderRecordVersion)) {
            SKSE::log::error("Could not open the smithing-order save record.");
            return;
        }
        const auto count = static_cast<std::uint32_t>(
            std::ranges::count_if(orders_, [](const CraftingOrderRecord& a_order) {
                return IsActiveStatus(a_order.status);
            }));
        if (!a_interface->WriteRecordData(nextOrderID_) ||
            !a_interface->WriteRecordData(count)) {
            return;
        }
        for (const auto& order : orders_) {
            if (!IsActiveStatus(order.status)) {
                continue;
            }
            const auto category = static_cast<std::uint8_t>(order.category);
            const auto status = static_cast<std::uint8_t>(order.status);
            if (!a_interface->WriteRecordData(order.schemaVersion) ||
                !a_interface->WriteRecordData(order.uniqueOrderID) ||
                !a_interface->WriteRecordData(order.smithFormID) ||
                !a_interface->WriteRecordData(order.smithBaseFormID) ||
                (order.schemaVersion >= 3 &&
                 !a_interface->WriteRecordData(order.workshopKey)) ||
                !a_interface->WriteRecordData(order.itemFormID) ||
                !WriteString(a_interface, order.itemPlugin) ||
                !a_interface->WriteRecordData(order.itemLocalFormID) ||
                !a_interface->WriteRecordData(order.quantity) ||
                !a_interface->WriteRecordData(category) ||
                !WriteString(a_interface, order.displayName) ||
                !a_interface->WriteRecordData(order.price) ||
                !a_interface->WriteRecordData(order.placedGameHour) ||
                !a_interface->WriteRecordData(order.startGameHour) ||
                !a_interface->WriteRecordData(order.readyGameHour) ||
                !a_interface->WriteRecordData(order.pickupCellFormID) ||
                !a_interface->WriteRecordData(order.pickupWorldspaceFormID) ||
                !a_interface->WriteRecordData(order.pickupX) ||
                !a_interface->WriteRecordData(order.pickupY) ||
                !a_interface->WriteRecordData(order.pickupZ) ||
                !a_interface->WriteRecordData(status) ||
                !a_interface->WriteRecordData(order.courierGeneration) ||
                !a_interface->WriteRecordData(order.courierNotified) ||
                !a_interface->WriteRecordData(order.lossNotified) ||
                (order.schemaVersion >= 2 &&
                 (!WriteString(a_interface, order.smithDisplayName) ||
                  !WriteString(a_interface, order.pickupLocation)))) {
                SKSE::log::error(
                    "Could not serialize smithing order {}.",
                    order.uniqueOrderID);
                return;
            }
        }
        if (!CourierService::GetSingleton().SaveRecord(a_interface)) {
            SKSE::log::error("Could not serialize smithing courier notices.");
        }
    }

    void OrderService::LoadImpl(SKSE::SerializationInterface* a_interface)
    {
        RevertImpl();
        if (!a_interface) {
            return;
        }
        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;
        if (!config_ || !config_->orders.enabled) {
            while (a_interface->GetNextRecordInfo(type, version, length)) {
                SKSE::log::info(
                    "Discarding disabled smithing-order record {:08X} version {}.",
                    type,
                    version);
            }
            return;
        }
        while (a_interface->GetNextRecordInfo(type, version, length)) {
            if (type == CourierService::RecordType) {
                if (!CourierService::GetSingleton().LoadRecord(
                        a_interface,
                        version)) {
                    SKSE::log::error(
                        "Smithing courier-notice save data is invalid.");
                    RevertImpl();
                    return;
                }
                continue;
            }
            if (type != kOrderRecordType || version != kOrderRecordVersion) {
                SKSE::log::warn(
                    "Ignoring unknown smithing-order record {:08X} version {}.",
                    type,
                    version);
                continue;
            }
            std::uint32_t count = 0;
            if (!ReadValue(a_interface, nextOrderID_) ||
                !ReadValue(a_interface, count) || count > 100000) {
                SKSE::log::error("Smithing-order save header is invalid.");
                RevertImpl();
                return;
            }
            orders_.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index) {
                CraftingOrderRecord order;
                std::uint8_t category = 0;
                std::uint8_t status = 0;
                if (!ReadValue(a_interface, order.schemaVersion) ||
                    !ReadValue(a_interface, order.uniqueOrderID) ||
                    !ReadValue(a_interface, order.smithFormID) ||
                    !ReadValue(a_interface, order.smithBaseFormID)) {
                    SKSE::log::error("Smithing-order save data is truncated.");
                    RevertImpl();
                    return;
                }
                if (order.schemaVersion >= 3 &&
                    !ReadValue(a_interface, order.workshopKey)) {
                    SKSE::log::error(
                        "Smithing-order workshop data is truncated.");
                    RevertImpl();
                    return;
                }
                if (
                    !ReadValue(a_interface, order.itemFormID) ||
                    !ReadString(a_interface, order.itemPlugin) ||
                    !ReadValue(a_interface, order.itemLocalFormID) ||
                    !ReadValue(a_interface, order.quantity) ||
                    !ReadValue(a_interface, category) ||
                    !ReadString(a_interface, order.displayName) ||
                    !ReadValue(a_interface, order.price) ||
                    !ReadValue(a_interface, order.placedGameHour) ||
                    !ReadValue(a_interface, order.startGameHour) ||
                    !ReadValue(a_interface, order.readyGameHour) ||
                    !ReadValue(a_interface, order.pickupCellFormID) ||
                    !ReadValue(a_interface, order.pickupWorldspaceFormID) ||
                    !ReadValue(a_interface, order.pickupX) ||
                    !ReadValue(a_interface, order.pickupY) ||
                    !ReadValue(a_interface, order.pickupZ) ||
                    !ReadValue(a_interface, status) ||
                    !ReadValue(a_interface, order.courierGeneration) ||
                    !ReadValue(a_interface, order.courierNotified) ||
                    !ReadValue(a_interface, order.lossNotified)) {
                    SKSE::log::error("Smithing-order save data is truncated.");
                    RevertImpl();
                    return;
                }
                if (order.schemaVersion >= 2 &&
                    (!ReadString(a_interface, order.smithDisplayName) ||
                     !ReadString(a_interface, order.pickupLocation))) {
                    SKSE::log::error(
                        "Smithing-order courier metadata is truncated.");
                    RevertImpl();
                    return;
                }
                if (category >
                        static_cast<std::uint8_t>(
                            CraftingOrderCategory::kCrossbow) ||
                    status >
                        static_cast<std::uint8_t>(
                            CraftingOrderStatus::kInvalid)) {
                    order.status = CraftingOrderStatus::kInvalid;
                } else {
                    order.category =
                        static_cast<CraftingOrderCategory>(category);
                    order.status =
                        static_cast<CraftingOrderStatus>(status);
                }

                if (!IsActiveStatus(order.status) || order.quantity <= 0) {
                    continue;
                }

                RE::FormID resolved = 0;
                if (!a_interface->ResolveFormID(order.smithFormID, resolved)) {
                    continue;
                }
                order.smithFormID = resolved;
                if (!a_interface->ResolveFormID(
                        order.smithBaseFormID,
                        resolved)) {
                    continue;
                }
                order.smithBaseFormID = resolved;
                if (order.schemaVersion >= 3) {
                    if (!a_interface->ResolveFormID(
                            order.workshopKey,
                            resolved)) {
                        continue;
                    }
                    order.workshopKey = resolved;
                } else {
                    order.workshopKey =
                        SmithClassifier::GetSingleton().WorkshopKeyForBase(
                            order.smithBaseFormID);
                    order.schemaVersion = 3;
                }
                if (!a_interface->ResolveFormID(order.itemFormID, resolved)) {
                    continue;
                }
                order.itemFormID = resolved;
                if (!RE::TESForm::LookupByID<RE::TESBoundObject>(
                        order.itemFormID)) {
                    continue;
                }
                if (order.pickupCellFormID &&
                    a_interface->ResolveFormID(order.pickupCellFormID, resolved)) {
                    order.pickupCellFormID = resolved;
                } else {
                    order.pickupCellFormID = 0;
                }
                if (order.pickupWorldspaceFormID &&
                    a_interface->ResolveFormID(
                        order.pickupWorldspaceFormID,
                        resolved)) {
                    order.pickupWorldspaceFormID = resolved;
                } else {
                    order.pickupWorldspaceFormID = 0;
                }
                nextOrderID_ = std::max(
                    nextOrderID_,
                    order.uniqueOrderID + 1);
                orders_.push_back(std::move(order));
            }
        }
        Update();
        SKSE::log::info("Loaded {} smithing orders.", orders_.size());
    }

    void OrderService::RevertImpl()
    {
        orders_.clear();
        nextOrderID_ = 1;
        CourierService::GetSingleton().Revert();
    }
}
