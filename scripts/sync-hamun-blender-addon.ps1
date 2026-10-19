param(
    [string]$PackagesRoot = 'D:\HamunDev\HamunPackages'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $repo 'integrations\blender\hamun_authoring_tools\__init__.py'
if (-not (Test-Path $source)) { throw "Source add-on missing: $source" }
$destinations = @(Get-ChildItem -Path $PackagesRoot -Directory -ErrorAction Stop |
    ForEach-Object { Join-Path $_.FullName '5.4\scripts\addons\hamun_authoring_tools' } |
    Where-Object { Test-Path (Join-Path $_ '__init__.py') })
if (-not $destinations.Count) { throw "No installed Hamun Blender 5.4 add-on found in $PackagesRoot" }
foreach ($target in $destinations) {
    $dest = Join-Path $target '__init__.py'
    $backup = "$dest.bak"
    Copy-Item $dest $backup -Force
    Copy-Item $source $dest -Force
    $cache = Join-Path $target '__pycache__'
    if (Test-Path $cache) { Remove-Item $cache -Recurse -Force }
    Write-Host "Updated installed addon: $dest"
}
$dll = Join-Path $repo 'build\viewport\Release\HamunRenderer.dll'
if (-not (Test-Path $dll)) {
    Write-Warning "Viewport DLL not built: $dll. Build with cmake --build build/viewport --config Release --target HamunRenderer"
} else {
    Write-Host "Updated addon will prefer: $dll"
}
Write-Host 'Completely restart HamunEditor/Blender before testing.'
