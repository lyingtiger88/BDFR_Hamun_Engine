# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
Package a locally built Hamun Editor for Windows x64.
.DESCRIPTION
Copies the complete Blender Release runtime, overlays Hamun's bundled add-on and
startup integration, and creates a portable ZIP plus SHA256 checksum.
Does not compile Blender, publish to GitHub, or claim clean-machine testing.
.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File .\integrations\blender\package_windows.ps1
#>
[CmdletBinding()]
param(
    [string] $ReleaseDir = 'D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release',
    [string] $OutputDir = 'D:\HamunDev\HamunPackages',
    [string] $Version = '0.1.0-alpha'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$addonSource = Join-Path $PSScriptRoot 'hamun_authoring_tools'
$startupSource = Join-Path $PSScriptRoot 'startup\hamun_startup.py'
$release = (Resolve-Path $ReleaseDir -ErrorAction Stop).Path
$releaseRoot = Join-Path $release '5.4'
foreach ($required in @(
    (Join-Path $release 'HamunEditor.exe'),
    (Join-Path $release 'HamunEditorCore.exe'),
    (Join-Path $releaseRoot 'scripts'),
    (Join-Path $release 'hamun.shared\hamun.shared.manifest'),
    (Join-Path $release 'hamun.crt\hamun.crt.manifest'),
    (Join-Path $release 'hamun_cpu_check.dll'),
    (Join-Path $release 'HamunThumb.dll'),
    (Join-Path $release 'license\license.md'),
    (Join-Path $release 'license\licenses.json'),
    (Join-Path $addonSource '__init__.py'),
    $startupSource
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required input is missing: $required"
    }
}
$dest = [System.IO.Path]::GetFullPath($OutputDir)
if ($dest.TrimEnd('\') -eq $release.TrimEnd('\') -or
    $dest.StartsWith($release.TrimEnd('\') + '\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputDir cannot be the Release directory or inside it.'
}
New-Item -ItemType Directory -Path $dest -Force | Out-Null
$packageName = 'HamunEditor-v' + $Version + '-Windows-x64'
$staging = Join-Path $dest $packageName
$zip = Join-Path $dest ($packageName + '.zip')
if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Path $staging -Force | Out-Null
Write-Host 'Copying Hamun Release runtime...'
& robocopy.exe $release $staging /E /R:2 /W:2 /NFL /NDL /NP /XD blender.shared blender.crt /XF *.pdb *.ilk *.lib *.exp *.pyc blender.exe blender-launcher.exe blender_cpu_check.dll BlendThumb.dll
if ($LASTEXITCODE -ge 8) { throw "Robocopy failed (exit code $LASTEXITCODE)" }
# Convert all 11 top-level diagnostic command helpers in the staging tree.
& (Join-Path $PSScriptRoot 'brand_windows_cmd.ps1') -PackageDir $staging
$addonTarget = Join-Path $staging '5.4\scripts\addons\hamun_authoring_tools'
New-Item -ItemType Directory -Path (Split-Path $addonTarget) -Force | Out-Null
Copy-Item -LiteralPath $addonSource -Destination $addonTarget -Recurse -Force
$startupTarget = Join-Path $staging '5.4\scripts\startup\hamun_startup.py'
Copy-Item -LiteralPath $startupSource -Destination $startupTarget -Force
# Remove obsolete experimental bootstrap if it existed in the source Release.
$legacyBootstrap = Join-Path $staging '5.4\scripts\startup\hamun_bootstrap.py'
if (Test-Path -LiteralPath $legacyBootstrap) {
    Remove-Item -LiteralPath $legacyBootstrap -Force
}
foreach ($required in @(
    (Join-Path $addonTarget '__init__.py'),
    $startupTarget,
    (Join-Path $staging 'HamunEditor.exe'),
    (Join-Path $staging 'HamunEditorCore.exe'),
    (Join-Path $staging 'hamun.shared\hamun.shared.manifest'),
    (Join-Path $staging 'hamun.crt\hamun.crt.manifest'),
    (Join-Path $staging 'hamun_cpu_check.dll'),
    (Join-Path $staging 'HamunThumb.dll'),
    (Join-Path $staging 'hamun_debug_log.cmd'),
    (Join-Path $staging 'hamun_factory_startup.cmd'),
    (Join-Path $staging 'license\license.md'),
    (Join-Path $staging 'license\licenses.json')
)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Package validation failed: $required" }
}
$oldNames = @('blender.exe','blender-launcher.exe','blender_cpu_check.dll','BlendThumb.dll','blender.shared','blender.crt')
foreach ($oldName in $oldNames) {
    if (Test-Path -LiteralPath (Join-Path $staging $oldName)) {
        throw "Deprecated top-level artifact still exists: $oldName"
    }
}
$readme = @"
Hamun Editor $Version - Windows x64 portable test build

Extract the whole folder and launch HamunEditor.exe.
Includes a modified Blender 5.4 Alpha core and Hamun authoring tools.
License information is in the license directory.
Blender upstream: https://projects.blender.org/blender/blender
Hamun source and patches: https://github.com/lyingtiger88/BDFR_Hamun_Engine/tree/feature/blender-source-integration
Blender base commit: 2622ed297d669e71dbfdc3478d844ed6004d9285

This package is for testing; test on a separate clean Windows machine before public release.
Verify corresponding source availability and license compliance before distribution.
"@
[System.IO.File]::WriteAllText((Join-Path $staging 'HAMUN_README.txt'), $readme, [System.Text.UTF8Encoding]::new($false))
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Write-Host 'Creating portable ZIP...'
Compress-Archive -LiteralPath $staging -DestinationPath $zip -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
$hashPath = Join-Path $dest ($packageName + '.sha256.txt')
[System.IO.File]::WriteAllText($hashPath, "$hash  $packageName.zip`r`n")
Write-Host "ZIP: $zip"
Write-Host "SHA256: $hash"
Write-Host "Checksum file: $hashPath"
Write-Host 'Next: test the extracted ZIP on a clean Windows x64 system.'
