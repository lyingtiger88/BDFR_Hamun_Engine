param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [string]$Version = "v0.4",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$BuildRoot = Join-Path $Root $BuildDir

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = "dist/Hamun_Test_$Version"
}

$Candidates = @(
    (Join-Path $BuildRoot "Samples/Sandbox/$Configuration"),
    (Join-Path $BuildRoot "Samples/Sandbox")
)

$BinaryDir = $null
foreach ($Candidate in $Candidates) {
    $Exe = Join-Path $Candidate "HamunSandbox.exe"
    if (Test-Path $Exe) {
        $BinaryDir = $Candidate
        break
    }
}

if (-not $BinaryDir) {
    throw "HamunSandbox.exe was not found. Build the Release configuration first."
}

$PackageDir = Join-Path $Root $OutputDir
if (Test-Path $PackageDir) {
    Remove-Item $PackageDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

Copy-Item (Join-Path $BinaryDir "HamunSandbox.exe") $PackageDir

$AssetsSource = Join-Path $BinaryDir "Assets"
if (-not (Test-Path $AssetsSource)) {
    throw "Assets folder was not found next to HamunSandbox.exe."
}

Copy-Item $AssetsSource (Join-Path $PackageDir "Assets") -Recurse

@"
@echo off
cd /d "%~dp0"
HamunSandbox.exe
"@ | Set-Content -Encoding ASCII (Join-Path $PackageDir "Run_DX12.bat")

@"
@echo off
cd /d "%~dp0"
HamunSandbox.exe --d3d11
"@ | Set-Content -Encoding ASCII (Join-Path $PackageDir "Run_DX11.bat")

@"
BDFR Hamun Engine - Test $Version
================================

Recommended:
  Double-click Run_DX12.bat

Compatibility:
  Double-click Run_DX11.bat

Controls:
  W / A / S / D : Move
  Q / E         : Down / Up
  Hold RMB      : Mouse look
  Shift         : Sprint
  Esc           : Exit

Test $Version includes:
  - renderer-owned RenderGraph (separate from HamunGraph visual scripting)
  - renderer-owned indexed scene draw submission
  - MainRenderPass + ComputePass ordering
  - double-buffered per-frame constant resources
  - DX12 and DX11 compute shader/pipeline/dispatch foundation
  - runtime Material + MaterialInstance system
  - glTF metallic/roughness factor import
  - GGX/Schlick metallic-roughness PBR direct-light foundation
  - GPU adapter, VRAM and capability reporting
  - temporal frame history and Halton jitter foundation for FSR
  - HamunHair runtime capability planner
  - TressFX-ready DX12/Vulkan strand path selection
  - card-hair fallback selection when TressFX is unavailable

The external AMD FSR and TressFX SDK binaries are not bundled yet. This build
validates the engine-side prerequisites and renderer architecture before those
third-party SDK bridges are linked.

DX12 is the primary Windows backend.
DX11 is the compatibility backend.

Shader note:
Hamun uses DXC Shader Model 6 when dxcompiler.dll is available.
Otherwise this bootstrap build falls back to the Windows D3DCompile path.
"@ | Set-Content -Encoding UTF8 (Join-Path $PackageDir "README_TEST.txt")

$Commit = "unknown"
try {
    $Commit = (git -C $Root rev-parse --short=12 HEAD).Trim()
} catch {
}

@"
Version: Test $Version
Commit: $Commit
Configuration: $Configuration
Architecture: x64
Generated: $(Get-Date -Format "yyyy-MM-ddTHH:mm:ssK")
"@ | Set-Content -Encoding UTF8 (Join-Path $PackageDir "BUILD_INFO.txt")

Write-Host "[Hamun] Windows test package ready:"
Write-Host "  $PackageDir"
