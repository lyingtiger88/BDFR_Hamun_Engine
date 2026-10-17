# SPDX-License-Identifier: GPL-2.0-or-later
<#
Rename the two shipped Blender theme presets in a staging package.
Keep XML bytes unchanged; do not touch the source or active user preferences.
#>
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PackageDir)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $PackageDir -ErrorAction Stop).Path
$themeDir = Join-Path $root '5.4\scripts\presets\interface_theme'
foreach ($variant in @('Dark', 'Light')) {
    $source = Join-Path $themeDir "Blender_$variant.xml"
    $target = Join-Path $themeDir "Hamun_$variant.xml"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing theme preset: $source"
    }
    if (Test-Path -LiteralPath $target) {
        throw "Refusing to overwrite existing theme: $target"
    }
}
foreach ($variant in @('Dark', 'Light')) {
    Rename-Item -LiteralPath (Join-Path $themeDir "Blender_$variant.xml") -NewName "Hamun_$variant.xml"
    Write-Host "Branded theme: Hamun_$variant.xml"
}
