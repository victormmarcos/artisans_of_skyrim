param(
    [string]$OutputZip = "Artisans of Skyrim.zip"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$distData = Join-Path $root "dist\Data"
$staging = Join-Path $root "build\fomod-package"

$requiredCore = @(
    "SKSE/Plugins/ArtisansOfSkyrim.dll",
    "Interface/ArtisansOfSkyrim/ServiceMenu.swf",
    "Interface/ArtisansOfSkyrim/mcm_logo.dds",
    "ArtisansOfSkyrim.esp",
    "SEQ/ArtisansOfSkyrim.seq",
    "Scripts/ArtisansOfSkyrim.pex",
    "Scripts/AOS_TIF_SmithCraft.pex",
    "Scripts/AOS_TIF_SmithImprove.pex",
    "Scripts/AOS_TIF_Enchant.pex",
    "Scripts/AOS_TIF_Alchemy.pex",
    "Scripts/AOS_TIF_CollectOrders.pex",
    "Scripts/AOS_MCM.pex",
    "MCM/Config/ArtisansOfSkyrim/settings.ini",
    "MCM/Config/ArtisansOfSkyrim/config.json",
    "Interface/Translations/ArtisansOfSkyrim_ENGLISH.txt",
    "Interface/Translations/ArtisansOfSkyrim_FRENCH.txt",
    "Interface/Translations/ArtisansOfSkyrim_GERMAN.txt",
    "Interface/Translations/ArtisansOfSkyrim_ITALIAN.txt",
    "Interface/Translations/ArtisansOfSkyrim_SPANISH.txt",
    "Interface/Translations/ArtisansOfSkyrim_POLISH.txt",
    "Interface/Translations/ArtisansOfSkyrim_RUSSIAN.txt",
    "Interface/Translations/ArtisansOfSkyrim_JAPANESE.txt",
    "Interface/Translations/ArtisansOfSkyrim_CHINESE.txt"
)

$mcmVariants = @(
    "enchantments",
    "potions",
    "enchantments_potions"
)

$optionNames = @(
    "skyforge_equipment",
    "custom_enchantments",
    "custom_potions"
)

$presetNames = @(
    "official_instant",
    "official_delayed",
    "mod_added_instant",
    "mod_added_delayed"
)

$missing = @()
foreach ($relative in $requiredCore) {
    if (-not (Test-Path -LiteralPath (Join-Path $distData $relative))) {
        $missing += "dist/Data/$relative"
    }
}
foreach ($relative in @("fomod/info.xml", "fomod/ModuleConfig.xml", "LICENSE.txt")) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        $missing += $relative
    }
}
foreach ($name in $presetNames) {
    $relative = "installer/presets/$name/SKSE/Plugins/ArtisansOfSkyrim.toml"
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        $missing += $relative
    }
}
foreach ($name in $optionNames) {
    $relative = "installer/options/$name/SKSE/Plugins"
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        $missing += $relative
    }
}
foreach ($name in $mcmVariants) {
    $relative = "installer/mcm/$name/MCM/Config/ArtisansOfSkyrim/config.json"
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        $missing += $relative
    }
}
if ($missing.Count -gt 0) {
    throw "Installable package is incomplete. Missing:`n - $($missing -join "`n - ")"
}

$resolvedStaging = [IO.Path]::GetFullPath($staging)
$expectedBuild = [IO.Path]::GetFullPath((Join-Path $root "build")) + [IO.Path]::DirectorySeparatorChar
if (-not $resolvedStaging.StartsWith($expectedBuild, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Package staging directory must stay inside the project build directory"
}
if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $staging | Out-Null

foreach ($relative in $requiredCore) {
    $source = Join-Path $distData $relative
    $target = Join-Path (Join-Path $staging "Core") $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
}

# Install the recommended delayed/official configuration as a low-priority
# fallback. The FOMOD-selected preset is copied at priority 100 and therefore
# overrides this file during a normal install. This prevents an upgrade or
# interrupted conditional install from silently falling back to the DLL's
# immediate-order defaults.
$fallbackConfig = Join-Path $root "installer\presets\official_delayed\SKSE\Plugins\ArtisansOfSkyrim.toml"
$fallbackTarget = Join-Path $staging "Core\SKSE\Plugins\ArtisansOfSkyrim.toml"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fallbackTarget) | Out-Null
Copy-Item -LiteralPath $fallbackConfig -Destination $fallbackTarget -Force

Copy-Item -LiteralPath (Join-Path $root "fomod") -Destination $staging -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root "THIRD_PARTY_NOTICES.md") -Destination $staging -Force
Copy-Item -LiteralPath (Join-Path $root "LICENSE.txt") -Destination $staging -Force
foreach ($readme in @("README - Translations.md", "README - Patches.md")) {
    Copy-Item -LiteralPath (Join-Path (Join-Path $root "installer") $readme) -Destination $staging -Force
}
foreach ($name in $presetNames) {
    $source = Join-Path (Join-Path $root "installer\presets") $name
    $target = Join-Path (Join-Path $staging "Presets") $name
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Recurse -Force
}
foreach ($name in $optionNames) {
    $source = Join-Path (Join-Path $root "installer\options") $name
    $target = Join-Path (Join-Path $staging "Options") $name
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Recurse -Force
}
foreach ($name in $mcmVariants) {
    $source = Join-Path (Join-Path $root "installer\mcm") $name
    $target = Join-Path (Join-Path $staging "Mcm") $name
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Recurse -Force
}

$outPath = Join-Path $root $OutputZip
if (Test-Path -LiteralPath $outPath) {
    Remove-Item -LiteralPath $outPath -Force
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open(
    $outPath,
    [System.IO.Compression.ZipArchiveMode]::Create)
try {
    $files = Get-ChildItem -LiteralPath $staging -File -Recurse |
        Sort-Object FullName
    foreach ($file in $files) {
        $entryName = $file.FullName.Substring($staging.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip,
            $file.FullName,
            $entryName,
            [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
}
finally {
    $zip.Dispose()
}

Write-Host "Created FOMOD installable package: $outPath" -ForegroundColor Green
Write-Host "Core runtime files: $($requiredCore.Count); presets: $($presetNames.Count); options: $($optionNames.Count); MCM variants: $($mcmVariants.Count)" -ForegroundColor Green
