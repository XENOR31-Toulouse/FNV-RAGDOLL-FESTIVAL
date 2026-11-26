# Construit le paquet a publier : dist\NVReactiveRagdoll-<version>.zip, pret pour Mod Organizer / Vortex / Nexus.
# Contenu (racine = dossier Data du jeu) : NVSE\Plugins\ (DLL + licence GPL-3), Config\ (.ini), MCM\ (menu en jeu).
# Lance d'abord build.cmd (package.cmd le fait).
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$version = [regex]::Match((Get-Content -Raw "$root\src\main.cpp"), 'kVersionName = "([^"]+)"').Groups[1].Value
if (-not $version) { throw 'kVersionName introuvable dans src\main.cpp' }

$stage = Join-Path $root 'build\package'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
$plugins = New-Item -ItemType Directory -Force (Join-Path $stage 'NVSE\Plugins')
$config = New-Item -ItemType Directory -Force (Join-Path $stage 'Config')
$mcm = New-Item -ItemType Directory -Force (Join-Path $stage 'MCM')
Copy-Item "$root\dist\Data\NVSE\Plugins\NVReactiveRagdoll.dll" $plugins
Copy-Item "$root\LICENSE" (Join-Path $plugins 'NVReactiveRagdoll-LICENSE.txt')
Copy-Item "$root\data\NVReactiveRagdoll.ini" $config
Copy-Item "$root\data\MCM\NVReactiveRagdoll.json" $mcm

$zip = Join-Path $root "dist\NVReactiveRagdoll-$version.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
# Pas Compress-Archive : sous PowerShell 5.1 il ecrit des chemins avec '\', hors norme zip
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, 'Create')
try {
    Get-ChildItem -Recurse -File $stage | ForEach-Object {
        $entry = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $_.FullName, $entry, 'Optimal')
    }
} finally { $archive.Dispose() }
Write-Output "Paquet : $zip"
