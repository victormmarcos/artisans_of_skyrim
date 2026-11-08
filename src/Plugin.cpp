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

#include "ArtisansOfSkyrim/Localization.h"
#include "NPCAlchemyServices/AlchemistClassifier.h"
#include "NPCAlchemyServices/AlchemyCatalog.h"
#include "NPCAlchemyServices/Config.h"
#include "NPCAlchemyServices/FormResolver.h"
#include "NPCAlchemyServices/InputHandler.h"
#include "NPCAlchemyServices/InventoryService.h"
#include "NPCAlchemyServices/MenuController.h"
#include "ArtisansOfSkyrim/PapyrusBridge.h"
#include "ArtisansOfSkyrim/RuntimeCompatibility.h"
#include "ArtisansOfSkyrim/Version.h"
#include "ArtisansOfSkyrim/ServiceMenu.h"
#include "ArtisansOfSkyrim/ExclusiveEffects.h"
#include "ArtisansOfSkyrim/UserSettings.h"

#include "NPCEnchantingServices/Config.h"
#include "NPCEnchantingServices/EnchanterClassifier.h"
#include "NPCEnchantingServices/EnchantmentCatalog.h"
#include "NPCEnchantingServices/FormResolver.h"
#include "NPCEnchantingServices/InputHandler.h"
#include "NPCEnchantingServices/InventoryService.h"
#include "NPCEnchantingServices/MenuController.h"

#include "NPCSmithServices/Config.h"
#include "NPCSmithServices/CourierService.h"
#include "NPCSmithServices/FormResolver.h"
#include "NPCSmithServices/InputHandler.h"
#include "NPCSmithServices/InventoryService.h"
#include "NPCSmithServices/MenuController.h"
#include "NPCSmithServices/OrderService.h"
#include "NPCSmithServices/OrderDialogueService.h"
#include "NPCSmithServices/RecipeIndex.h"
#include "NPCSmithServices/SmithClassifier.h"

namespace
{
    [[nodiscard]] bool SupportsRuntime(const REL::Version& a_version) noexcept
    {
        return AOS::RuntimeCompatibility::Supports(a_version.pack());
    }

    std::unique_ptr<NPCSS::Config> g_smithConfig;
    std::unique_ptr<NPCES::Config> g_enchantingConfig;
    std::unique_ptr<NPCAS::Config> g_alchemyConfig;

    constexpr RE::FormID kDialogueQuestLocalFormID = 0x800;
    constexpr std::string_view kDialoguePlugin = "ArtisansOfSkyrim.esp";
    constexpr std::string_view kDialogueSeq = "ArtisansOfSkyrim.seq";
    constexpr std::string_view kDialogueQuestEditorID = "AOS_DialogueQuest";

    void InitializeLogging()
    {
        std::vector<std::filesystem::path> candidates;
        if (const auto directory = SKSE::log::log_directory()) {
            candidates.push_back(*directory / "ArtisansOfSkyrim.log");
        }
        try {
            const auto fallback =
                std::filesystem::temp_directory_path() /
                "ArtisansOfSkyrim.log";
            if (candidates.empty() || candidates.front() != fallback) {
                candidates.push_back(fallback);
            }
        } catch (const std::exception&) {
        }

        for (const auto& path : candidates) {
            try {
                auto sink =
                    std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                        path.string(),
                        true);
                auto logger = std::make_shared<spdlog::logger>(
                    "ArtisansOfSkyrim",
                    std::move(sink));
                spdlog::set_default_logger(std::move(logger));
                spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
#ifdef _DEBUG
                spdlog::set_level(spdlog::level::debug);
#else
                spdlog::set_level(spdlog::level::info);
#endif
                spdlog::flush_on(spdlog::level::info);
                SKSE::log::info(
                    "Logging initialized at '{}'.",
                    path.string());
                return;
            } catch (const std::exception&) {
                // Try the next deterministic location. Logging must never
                // prevent the SKSE plugin from loading.
            }
        }
    }

    [[nodiscard]] std::filesystem::path DataPath()
    {
        const std::filesystem::path executable{
            REL::Module::get().filePath().data()
        };
        return executable.parent_path() / "Data";
    }

    void ValidateDialogueAsset(
        std::string_view a_pluginName,
        std::string_view a_seqName)
    {
        const auto dataPath = DataPath();
        const auto espPath = dataPath / a_pluginName;
        const auto seqPath = dataPath / "SEQ" / a_seqName;

        if (!std::filesystem::exists(espPath)) {
            SKSE::log::warn(
                "Dialogue ESP was not found at '{}'.",
                espPath.string());
        }
        if (!std::filesystem::exists(seqPath)) {
            SKSE::log::warn(
                "Dialogue SEQ was not found at '{}'. Generate it with xEdit.",
                seqPath.string());
        }
    }

    [[nodiscard]] RE::TESQuest* ResolveDialogueQuest()
    {
        if (const auto quest =
                RE::TESForm::LookupByEditorID<RE::TESQuest>(
                    kDialogueQuestEditorID)) {
            return quest;
        }

        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }
        return dataHandler->LookupForm<RE::TESQuest>(
            kDialogueQuestLocalFormID,
            kDialoguePlugin);
    }

    void EnsureDialogueQuestStarted()
    {
        const auto quest = ResolveDialogueQuest();
        if (!quest) {
            SKSE::log::warn(
                "Dialogue quest '{}' / {:06X} was not found in {}.",
                kDialogueQuestEditorID,
                kDialogueQuestLocalFormID,
                kDialoguePlugin);
            return;
        }
        if (quest->IsRunning()) {
            return;
        }
        quest->SetEnabled(true);
        if (!quest->Start() && !quest->IsRunning()) {
            SKSE::log::error(
                "Dialogue quest in {} could not be started.",
                kDialoguePlugin);
        }
    }

    void InitializeSmithing(const toml::table* a_settings)
    {
        SKSE::log::info("DataLoaded/Smithing: begin configuration.");
        try {
            if (a_settings) {
                g_smithConfig =
                    std::make_unique<NPCSS::Config>(
                        NPCSS::Config::FromTable(*a_settings));
            } else {
                g_smithConfig = std::make_unique<NPCSS::Config>();
                SKSE::log::warn(
                    "Smithing configuration '{}' was not found; using defaults.",
                    AOS::UserSettings::PresetPath().string());
            }
        } catch (const std::exception& error) {
            SKSE::log::critical("Smithing configuration error: {}", error.what());
            return;
        }

        SKSE::log::info("DataLoaded/Smithing: building form resolver.");
        auto& resolver = NPCSS::FormResolver::GetSingleton();
        resolver.Build(*g_smithConfig);
        SKSE::log::info("DataLoaded/Smithing: building recipe index.");
        NPCSS::RecipeIndex::GetSingleton().Build(*g_smithConfig);
        SKSE::log::info("DataLoaded/Smithing: configuring classifier.");
        NPCSS::SmithClassifier::GetSingleton().Configure(*g_smithConfig);
        SKSE::log::info("DataLoaded/Smithing: configuring inventory.");
        NPCSS::InventoryService::GetSingleton().Configure(*g_smithConfig);
        SKSE::log::info("DataLoaded/Smithing: configuring orders.");
        NPCSS::OrderService::GetSingleton().Configure(*g_smithConfig);
        if (g_smithConfig->orders.enabled) {
            NPCSS::CourierService::GetSingleton().InstallBookCallHooks();
            NPCSS::CourierService::GetSingleton().Initialize();
            NPCSS::OrderService::GetSingleton().RegisterEvents();
        }
        NPCSS::OrderDialogueService::GetSingleton().Configure(
            g_smithConfig->orders.enabled);
        SKSE::log::info("DataLoaded/Smithing: registering order dialogue.");
        NPCSS::OrderDialogueService::GetSingleton().Register();
        SKSE::log::info("DataLoaded/Smithing: configuring menu/input.");
        NPCSS::MenuController::GetSingleton().Configure(*g_smithConfig);
        NPCSS::InputHandler::GetSingleton().Configure(*g_smithConfig);
        NPCSS::InputHandler::GetSingleton().Register();
        SKSE::log::info("Smithing service initialized.");
    }

    void InitializeEnchanting(const toml::table* a_settings)
    {
        SKSE::log::info("DataLoaded/Enchanting: begin configuration.");
        try {
            if (!a_settings) {
                SKSE::log::critical(
                    "Enchanting configuration '{}' was not found.",
                    AOS::UserSettings::PresetPath().string());
                return;
            }
            g_enchantingConfig =
                std::make_unique<NPCES::Config>(
                    NPCES::Config::FromTable(*a_settings));
        } catch (const std::exception& error) {
            SKSE::log::critical(
                "Enchanting configuration error: {}",
                error.what());
            return;
        }

        SKSE::log::info("DataLoaded/Enchanting: building form resolver.");
        auto& resolver = NPCES::FormResolver::GetSingleton();
        resolver.Build(*g_enchantingConfig);
        SKSE::log::info("DataLoaded/Enchanting: building catalog.");
        NPCES::EnchantmentCatalog::GetSingleton().Build(*g_enchantingConfig);
        SKSE::log::info("DataLoaded/Enchanting: configuring services.");
        NPCES::EnchanterClassifier::GetSingleton().Configure(*g_enchantingConfig);
        NPCES::InventoryService::GetSingleton().Configure(*g_enchantingConfig);
        NPCES::MenuController::GetSingleton().Configure(*g_enchantingConfig);
        NPCES::InputHandler::GetSingleton().Configure(*g_enchantingConfig);
        NPCES::InputHandler::GetSingleton().Register();
        SKSE::log::info("Enchanting service initialized.");
    }

    void InitializeAlchemy(const toml::table* a_settings)
    {
        SKSE::log::info("DataLoaded/Alchemy: begin configuration.");
        try {
            if (!a_settings) {
                SKSE::log::critical(
                    "Alchemy configuration '{}' was not found.",
                    AOS::UserSettings::PresetPath().string());
                return;
            }
            g_alchemyConfig =
                std::make_unique<NPCAS::Config>(
                    NPCAS::Config::FromTable(*a_settings));
        } catch (const std::exception& error) {
            SKSE::log::critical("Alchemy configuration error: {}", error.what());
            return;
        }

        SKSE::log::info("DataLoaded/Alchemy: building form resolver.");
        auto& resolver = NPCAS::FormResolver::GetSingleton();
        resolver.Build(*g_alchemyConfig);
        SKSE::log::info("DataLoaded/Alchemy: building catalog.");
        NPCAS::AlchemyCatalog::GetSingleton().Build(*g_alchemyConfig);
        SKSE::log::info("DataLoaded/Alchemy: configuring services.");
        NPCAS::AlchemistClassifier::GetSingleton().Configure(*g_alchemyConfig);
        NPCAS::InventoryService::GetSingleton().Configure(*g_alchemyConfig);
        NPCAS::MenuController::GetSingleton().Configure(*g_alchemyConfig);
        NPCAS::InputHandler::GetSingleton().Configure(*g_alchemyConfig);
        NPCAS::InputHandler::GetSingleton().Register();
        SKSE::log::info("Alchemy service initialized.");
    }

    void InitializeServices()
    {
        SKSE::log::info("DataLoaded/Services: begin.");
        AOS::UserSettings::LoadMcmSettingsFile();
        std::optional<toml::table> settings;
        bool settingsReadable = true;
        try {
            settings = AOS::UserSettings::LoadMergedTable();
        } catch (const std::exception& error) {
            settingsReadable = false;
            SKSE::log::critical("Configuration error: {}", error.what());
        }
        if (settingsReadable) {
            const auto* table = settings ? std::addressof(*settings) : nullptr;
            InitializeSmithing(table);
            InitializeEnchanting(table);
            InitializeAlchemy(table);
        }
        AOS::ExclusiveEffects::GetSingleton().Initialize();
        const auto state = [](bool a_enabled) { return a_enabled ? "enabled" : "disabled"; };
        SKSE::log::info(
            "Features: Skyforge equipment={}, custom enchantments={}, custom potions={}, "
            "exclusive runtime support=always-on, timed Smithing orders={}.",
            state(g_smithConfig && g_smithConfig->enableSkyforgeEquipment),
            state(g_enchantingConfig && g_enchantingConfig->enableCustomEnchantments),
            state(g_alchemyConfig && g_alchemyConfig->enableCustomPotions),
            state(g_smithConfig && g_smithConfig->orders.enabled));
        SKSE::log::info("Artisans of Skyrim services initialized.");
    }

    // Applies changed prices while the game runs. Only price fields are copied,
    // in place, because catalogs and services hold pointers into these configs.
    void ReloadPricingSettings()
    {
        std::optional<toml::table> settings;
        try {
            settings = AOS::UserSettings::LoadMergedTable();
        } catch (const std::exception& error) {
            SKSE::log::error("Pricing reload skipped: {}", error.what());
            return;
        }
        if (!settings) {
            SKSE::log::error("Pricing reload skipped: configuration file was not found.");
            return;
        }

        AOS::ServiceMenu::Close();

        if (g_smithConfig) {
            try {
                const auto fresh = NPCSS::Config::FromTable(*settings);
                g_smithConfig->improvementPricing = fresh.improvementPricing;
                g_smithConfig->craftingPricing = fresh.craftingPricing;
                g_smithConfig->temperTargets = fresh.temperTargets;
                g_smithConfig->barterPricing = fresh.barterPricing;
            } catch (const std::exception& error) {
                SKSE::log::error("Smithing pricing reload failed: {}", error.what());
            }
        }

        if (g_enchantingConfig) {
            try {
                const auto fresh = NPCES::Config::FromTable(*settings);
                g_enchantingConfig->pricing = fresh.pricing;
                g_enchantingConfig->barterPricing = fresh.barterPricing;
                for (auto& definition : g_enchantingConfig->enchantments) {
                    const auto match = std::ranges::find(
                        fresh.enchantments,
                        definition.id,
                        &NPCES::EnchantmentDefinition::id);
                    if (match == fresh.enchantments.end()) {
                        continue;
                    }
                    for (std::size_t tier = 0; tier < definition.values.size(); ++tier) {
                        definition.values[tier].price = match->values[tier].price;
                    }
                }
            } catch (const std::exception& error) {
                SKSE::log::error("Enchanting pricing reload failed: {}", error.what());
            }
        }

        if (g_alchemyConfig) {
            try {
                const auto fresh = NPCAS::Config::FromTable(*settings);
                g_alchemyConfig->barterPricing = fresh.barterPricing;
                for (auto& item : g_alchemyConfig->items) {
                    const auto match = std::ranges::find(
                        fresh.items,
                        item.id,
                        &NPCAS::AlchemyDefinition::id);
                    if (match != fresh.items.end()) {
                        item.batchPrice = match->batchPrice;
                    }
                }
            } catch (const std::exception& error) {
                SKSE::log::error("Alchemy pricing reload failed: {}", error.what());
            }
        }

        SKSE::log::info("Pricing settings reloaded.");
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        if (a_message->type == SKSE::MessagingInterface::kInputLoaded) {
            AOS::ServiceMenu::Register();
        } else if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
            SKSE::log::info("DataLoaded: handler entered.");
            AOS::Text::Initialize();
            ValidateDialogueAsset(
                kDialoguePlugin,
                kDialogueSeq);
            SKSE::log::info("DataLoaded: dialogue assets checked.");
            InitializeServices();
            SKSE::log::info("DataLoaded: ensuring dialogue quest.");
            EnsureDialogueQuestStarted();
            SKSE::log::info("DataLoaded: handler completed.");
        } else if (a_message->type == SKSE::MessagingInterface::kPreLoadGame) {
            AOS::ServiceMenu::Close();
            AOS::ExclusiveEffects::GetSingleton().ClearForLoad();
        } else if (
            a_message->type == SKSE::MessagingInterface::kPostLoadGame ||
            a_message->type == SKSE::MessagingInterface::kNewGame) {
            EnsureDialogueQuestStarted();
            AOS::InstallExclusiveRuntimeHooks();
            AOS::ExclusiveEffects::GetSingleton().ActivateRuntime();
            AOS::ExclusiveEffects::GetSingleton().ResetForLoadedGame();
            if (g_smithConfig && g_smithConfig->orders.enabled) {
                NPCSS::OrderService::GetSingleton().Update();
            }
        }
    }
}

SKSE_PLUGIN_VERSION = []() constexpr {
    SKSE::PluginVersionData data;
    data.PluginVersion({ AOS::Version::kMajor, AOS::Version::kMinor, AOS::Version::kPatch, 0 });
    data.PluginName("Artisans of Skyrim");
    data.AuthorName("Artisans of Skyrim");
    data.UsesNoStructs();
    for (std::size_t index = 0; index < AOS::RuntimeCompatibility::Versions.size(); ++index) {
        data.compatibleVersions[index] = AOS::RuntimeCompatibility::Versions[index];
    }
    return data;
}();

SKSE_EXPORT bool SKSEPlugin_Query(
    const SKSE::QueryInterface* a_skse,
    SKSE::PluginInfo* a_info)
{
    if (!a_skse || !a_info) {
        return false;
    }
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = "Artisans of Skyrim";
    a_info->version = REL::Version{ AOS::Version::kMajor, AOS::Version::kMinor, AOS::Version::kPatch, 0 }.pack();
    return !a_skse->IsEditor() && SupportsRuntime(a_skse->RuntimeVersion());
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    if (!a_skse || a_skse->IsEditor() ||
        !SupportsRuntime(a_skse->RuntimeVersion())) {
        return false;
    }
    SKSE::Init(a_skse);
    InitializeLogging();
    SKSE::log::info(
        "Artisans of Skyrim " AOS_VERSION_STRING " is loading. Skyrim={}, SKSE={}, CommonLibSSE-NG=7.2.0.",
        a_skse->RuntimeVersion().string(),
        REL::Version::unpack(a_skse->SKSEVersion()).string());
    NPCSS::OrderService::GetSingleton().RegisterSerialization();
    AOS::UserSettings::SetReloadHandler(ReloadPricingSettings);
    AOS::InstallExclusiveDisenchantHooks();
    AOS::InstallExclusiveRuntimeHooks();

    const auto papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus ||
        !papyrus->Register(AOS::PapyrusBridge::Register)) {
        SKSE::log::critical("Could not register the Artisans of Skyrim Papyrus bridge.");
        return false;
    }
    const auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        SKSE::log::critical("Could not register the SKSE messaging listener.");
        return false;
    }
    return true;
}
