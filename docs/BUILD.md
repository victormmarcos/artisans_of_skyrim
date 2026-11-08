# Build and release

The only build workflow for Artisans of Skyrim. Run commands from the project
root in a Visual Studio x64 developer shell. The version is set only on the
`project()` line of `CMakeLists.txt`; C++ and the Python tools read it from there.

## Requirements

- Windows x64; Visual Studio 2022 or newer with Desktop C++ tools.
- CMake 3.24 or newer and Ninja on PATH.
- Python 3.11 or newer and PowerShell.
- vcpkg with `VCPKG_ROOT` set to its checkout; dependencies follow `vcpkg.json`.
- MTASC 1.15; set `MTASC_PATH` to its executable or pass `--mtasc` explicitly.

CommonLibSSE-NG 7.2.0 and OpenVR sources are pinned by URL and hash in
`CMakeLists.txt`. Existing offline checkouts may be supplied through CMake's
`FETCHCONTENT_SOURCE_DIR_COMMONLIBSSE` and `FETCHCONTENT_SOURCE_DIR_OPENVR`.
Do not hardcode local checkout paths into source or presets.

## Compile

Build releases from a folder outside the user profile (for example
`C:\AOSBuild`), so no account name can reach the binaries even if a dependency
ignores path mapping.

```powershell
cmake --preset release
python tools/CompileInterface.py
cmake --build --preset release --clean-first
ctest --preset release
```

The clean build avoids stale layouts after shared-header changes. Six native
test executables cover Smithing, Enchanting, Alchemy, runtime compatibility,
menu sorting, and exclusive-effect rules. CMake stages the DLL in
`dist/Data/SKSE/Plugins/`. The interface compiler stages the Flash 8/AS2 SWF
in `dist/Data/Interface/ArtisansOfSkyrim/` and records source/output hashes.

Release compilation remaps source and dependency paths and ships no PDB.
`tools/CheckReleaseBinary.py` rejects a DLL with an embedded profile path; if it
fails, rebuild the responsible dependency with path mapping.

## Generated assets

Do not regenerate records or scripts for a UI-only change. When their
authoritative sources deliberately change, use the corresponding command:

```powershell
python tools/GenerateInstallerPresets.py
python tools/GenerateArtisansOfSkyrimEsp.py
python tools/GeneratePapyrusPex.py
python tools/GenerateMcmConfig.py
python tools/GenerateTranslations.py
python tools/MakeMcmLogo.py
```

Run `GenerateTranslations.py` after changing any player-facing text in C++,
`T("...")`/`U("...")` texts in the movie, or the MCM; it also rewrites
`include/ArtisansOfSkyrim/InterfaceTexts.h`, so rebuild the DLL afterwards.
`GenerateMcmConfig.py --check` and `GenerateTranslations.py --check` report stale
outputs without writing.

The ESP generator also updates tables and configuration, so review every
generated diff. For a dialogue-only change, `build_esp(read_alchemy_records(),
read_dialogue_responses(read_npc_dialogue_conditions()))` rebuilds the ESP and
manifest without rewriting catalog configuration. `papyrus/Source/` is
authoritative; CMake copies and compares all seven distributed PSC files.
The dialogue quest FormID and SEQ must remain stable when no quest is added.

## Validate and package

```powershell
python tools/CheckReleaseBinary.py dist/Data/SKSE/Plugins/ArtisansOfSkyrim.dll
python tools/ValidateRuntimeAssets.py
python tools/ValidateArtisansOfSkyrimAlchemyEsp.py
python tools/ValidateArtisansOfSkyrimDialogue.py
python tools/ValidateGameplayRecords.py
powershell -ExecutionPolicy Bypass -File tools/MakeInstallableZip.ps1
python tools/ValidateReleasePackage.py "Artisans of Skyrim.zip"
```

After successful validation, remove the local `build/` directory. Keep the
staged DLL/SWF and the installer.

## Installer contents

`Artisans of Skyrim.zip` is the only public output. Its FOMOD root contains
`fomod/`, `Core/`, `Presets/`, `Options/`, `Mcm/`, `THIRD_PARTY_NOTICES.md`,
`LICENSE.txt`, `README - Translations.md` and `README - Patches.md`.

Core contains one ESP, DLL, SEQ, the Service Menu SWF, the MCM logo, the native
bridge PEX, the MCM PEX, five dialogue-fragment PEX files, the MCM files, the
translation file for every game language, and a low-priority official delayed
fallback TOML. The selected preset overwrites that fallback at priority 100,
leaving exactly one main runtime configuration.

Four presets combine equipment origin with immediate/timed Smithing. Three option
files (Custom Skyforge Equipment, Custom Enchantments, Custom Potions) and the
matching MCM page variants are installed by checkbox. The recommended preset is
`official_delayed` with all three options. Existing exclusive items keep runtime
support and disenchant protection in all presets. The FOMOD's author and Nexus
website metadata must not change.

The installer never contains PDBs, build trees, caches, logs, source tables or
redistributed voice assets.

## Release checklist

Offline:

- DLL and SWF compile from the current sources; all six test suites pass.
- Runtime metadata, GOG subtype, version, ESP header/range and SWF hashes pass.
- Dialogue, Alchemy, gameplay-record and release validators pass.
- FormIDs, gameplay records, prices, quantities and configuration stay the same
  unless the change is intended.
- `GenerateMcmConfig.py --check` and `GenerateTranslations.py --check` pass.
- `fomod/info.xml` and the `moduleName` in `fomod/ModuleConfig.xml` carry the
  version from `CMakeLists.txt` (the release validator checks this).
- `CHANGELOG.md` and `docs/RELEASE_NOTES.md` describe the release.

In game: run the [runtime test plan](RUNTIME_TEST_PLAN.md). Prioritize
locked/unlocked dialogue, all services, single/dual Enchanting and Rename,
five-item Alchemy, Orders, long descriptions, notices, mouse wheel,
cancel/reopen, and switching between keyboard, mouse and gamepad.

Offline checks validate records, contracts, metadata and archives. They do not
prove the engine ABI, cursor behavior, renderer or controller integration, so do
not mark a runtime or input path as tested until it was checked in game. The
optional voice audit needs explicit paths to the local game masters, Strings and
voice archive; see `ValidateVanillaVoiceAssets.py --help`.
