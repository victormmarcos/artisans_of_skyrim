# Artisans of Skyrim

SKSE plugin for Skyrim Special Edition / Anniversary Edition. Hire Skyrim's
blacksmiths, enchanters and alchemists for Smithing, Enchanting, Alchemy and
timed Orders through a dedicated Service Menu.

Nexus Mods: <https://www.nexusmods.com/skyrimspecialedition/mods/185212>

## Features

- **Service Menu** with the artisan's rank, category tabs, a searchable and
  sortable item list, a large 3D item preview and details for each service.
  Mouse, keyboard and gamepad are supported; the preview rotates with a mouse
  drag or the right stick. SkyUI is not required and no vanilla or SkyUI menu is
  replaced.
- **Dialogue** appears only after the artisan's existing requirements are met.
  Requests use the vanilla voice response followed by the Service Menu; artisans
  with voice types from other mods use silent fallback lines.
- **Enchanting**: Master enchanters apply one or two effects, each with its own
  price; Craft opens an itemized confirmation with the total and optional Rename.
- **Alchemy** sells five-item batches. **Orders** show delivery estimates and
  ready items are collected directly.
- **Settings** through the installer presets, optional MCM (MCM Helper), a
  personal `ArtisansOfSkyrim_Custom.toml`, and patch files from other mods.
- **Translations**: every player-facing text is in
  `Interface/Translations/ArtisansOfSkyrim_<LANGUAGE>.txt` (all game languages).

Controls and menu behavior: [docs/SERVICE_MENU.md](docs/SERVICE_MENU.md).
Current release notes: [docs/RELEASE_NOTES.md](docs/RELEASE_NOTES.md).

## Requirements

- SKSE64 and Address Library for SKSE Plugins, matching the exact Skyrim runtime
  and store.
- Runtimes: Steam 1.5.97, 1.6.317, 1.6.318, 1.6.323, 1.6.342, 1.6.353, 1.6.629,
  1.6.640, 1.6.1130, 1.6.1170, 1.7.99, 1.7.104; GOG 1.6.659 and 1.6.1179. No VR.
  A listed runtime is accepted by the DLL; it is not a claim that every runtime
  was tested in game.
- Optional: MCM Helper for the settings menu.

## Installation

Install `Artisans of Skyrim.zip` (Nexus download) with a mod manager and choose
the FOMOD options. The FOMOD combines equipment origin (official or including
mods) with timed or immediate Smithing, and offers three optional additions:
Custom Skyforge Equipment, Custom Enchantments and Custom Potions. The archive
includes `README - Patches.md` (compatibility patches) and
`README - Translations.md`.

## Building

See [docs/BUILD.md](docs/BUILD.md). The version is set only in `CMakeLists.txt`.

## Repository layout

| Location | Contents |
|---|---|
| `src/`, `include/ArtisansOfSkyrim/` | Lifecycle, Service Menu, settings, translation, Papyrus bridge, shared rules |
| `smithing_src/`, `smithing_include/` | Smithing, Orders, workshops, courier |
| `enchanting_src/`, `enchanting_include/` | Enchanting catalog and transactions |
| `alchemy_src/`, `include/NPCAlchemyServices/` | Alchemy catalog and transactions |
| `tests/` | Engine-independent unit tests (run by `ctest`) |
| `interface/` | AS2 sources of the Service Menu, MCM logo source, build manifest |
| `papyrus/Source/` | Papyrus sources: native bridge, MCM script, dialogue fragments |
| `config/` | Main runtime configuration and an example of personal settings |
| `source-data/` | Reviewed tables and record snapshots used by the generators |
| `installer/` | Generated FOMOD presets, option files, MCM variants, archive READMEs |
| `fomod/` | FOMOD installer definition |
| `tools/` | Generation, validation and packaging scripts |
| `dist/Data/` | Staged game files (ESP, scripts, interface, translations, DLL) |
| `docs/` | Documentation |

## Documentation

- [Build and release](docs/BUILD.md)
- [Architecture](docs/ARCHITECTURE.md): module boundaries and data ownership
- [Service Menu](docs/SERVICE_MENU.md): layout, controls and limits
- [ESP generation](docs/ESP_GENERATION_NOTES.md), [dialogue](docs/DIALOGUE_PLUGIN_SPEC.md),
  [alchemy](docs/ALCHEMY_IMPLEMENTATION_NOTES.md),
  [voice responses](docs/VANILLA_VOICE_RESPONSE_AUDIT.md)
- [Runtime test plan](docs/RUNTIME_TEST_PLAN.md) and [QA notes](docs/QA_NOTES.md)
- [Changelog](CHANGELOG.md)

## License

Copyright (C) 2026 victormmarcos

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

The full license text is in [LICENSE.txt](LICENSE.txt), also available at
<https://www.gnu.org/licenses/gpl-3.0.txt>. The GPL applies because the SKSE
plugin is built with CommonLibSSE-NG (GPL-3.0). Third-party components are
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
