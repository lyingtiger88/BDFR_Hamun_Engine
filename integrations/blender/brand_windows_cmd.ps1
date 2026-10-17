# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
Rebrands the 11 legacy Blender command helpers in a STAGING package only.
.DESCRIPTION
Edits both filenames and embedded commands. Does not touch the build directory.
#>
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PackageDir)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $PackageDir -ErrorAction Stop).Path
$names = @(
    'blender_debug_cycles.cmd',
    'blender_debug_gpu.cmd',
    'blender_debug_gpu_glitchworkaround.cmd',
    'blender_debug_log.cmd',
    'blender_factory_startup.cmd',
    'blender_factory_startup_opengl.cmd',
    'blender_factory_startup_vulkan.cmd',
    'blender_oculus.cmd',
    'blender_startup_opengl.cmd',
    'blender_startup_vulkan.cmd',
    'blender_system_info.cmd'
)
$jobs = @()
foreach ($name in $names) {
    $source = Join-Path $root $name
    $destName = $name -replace '^blender_', 'hamun_'
    $target = Join-Path $root $destName
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Expected CMD file missing: $source"
    }
    if (Test-Path -LiteralPath $target) {
        throw "Refusing to overwrite existing file: $target"
    }
    $raw = [System.IO.File]::ReadAllText($source)
    $fixed = $raw.Replace('%~dp0\blender"', '%~dp0HamunEditorCore.exe"')
    # Use the console-enabled core for debugging so redirected output is preserved.
    $fixed = $fixed.Replace('blender_debug_output.txt', 'hamun_debug_output.txt')
    $fixed = $fixed.Replace('blender_system_info.txt', 'hamun_system_info.txt')
    $fixed = $fixed.Replace('%temp%\blender\debug_logs', '%temp%\hamun\debug_logs')
    $fixed = $fixed.Replace('Starting blender', 'Starting Hamun Editor')
    $fixed = $fixed.Replace('Starting Blender', 'Starting Hamun Editor')
    $fixed = $fixed.Replace('close blender', 'close Hamun Editor')
    $fixed = $fixed.Replace('Your Blender installation', 'Your Hamun Editor installation')
    # Environment variable names are local to this helper, not Blender API names.
    $fixed = $fixed.Replace('BLENDER_INSTALL_DIRECTORY', 'HAMUN_INSTALL_DIRECTORY')
    $fixed = $fixed.Replace('BLENDER_VERSION_FOLDER', 'HAMUN_VERSION_FOLDER')
    if ($fixed -match '(?i)%~dp0\\blender\b') {
        throw "Unconverted executable invocation: $name"
    }
    $jobs += [pscustomobject]@{ Source=$source; Target=$target; Content=$fixed; Name=$destName }
}
foreach ($job in $jobs) {
    [System.IO.File]::WriteAllText($job.Target, $job.Content, [System.Text.Encoding]::Default)
    Remove-Item -LiteralPath $job.Source -Force
    Write-Host "Branded: $($job.Name)"
}
if (@(Get-ChildItem -LiteralPath $root -Filter 'blender_*.cmd' -File).Count -gt 0) {
    throw 'Legacy CMD filename remains in staging'
}
Write-Host "Branded $($jobs.Count) Windows CMD helpers."
