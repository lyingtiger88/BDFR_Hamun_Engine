param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [string]$Version = "v0.5",
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

$LauncherCandidates = @(
    (Join-Path $BuildRoot "Tools/HamunLauncher/$Configuration/HamunLauncher.exe"),
    (Join-Path $BuildRoot "Tools/HamunLauncher/HamunLauncher.exe")
)

$LauncherExe = $null
foreach ($Candidate in $LauncherCandidates) {
    if (Test-Path $Candidate) {
        $LauncherExe = $Candidate
        break
    }
}

if ($LauncherExe) {
    Copy-Item $LauncherExe $PackageDir

    $EditorCandidates = @(
        (Join-Path $BuildRoot "Tools/HamunEditor/$Configuration/HamunEditor.exe"),
        (Join-Path $BuildRoot "Tools/HamunEditor/HamunEditor.exe")
    )

    foreach ($EditorCandidate in $EditorCandidates) {
        if (Test-Path $EditorCandidate) {
            Copy-Item $EditorCandidate $PackageDir
            break
        }
    }

    $LauncherTemplates = Join-Path (Split-Path $LauncherExe -Parent) "Templates"
    if (Test-Path $LauncherTemplates) {
        Copy-Item $LauncherTemplates (Join-Path $PackageDir "Templates") -Recurse
    }
    else {
        $RootTemplates = Join-Path $Root "Templates"
        if (Test-Path $RootTemplates) {
            Copy-Item $RootTemplates (Join-Path $PackageDir "Templates") -Recurse
        }
    }

    @"
@echo off
cd /d "%~dp0"
HamunLauncher.exe
"@ | Set-Content -Encoding ASCII (Join-Path $PackageDir "Run_Launcher.bat")
}

$FidelityFxDlls = Get-ChildItem -Path $BinaryDir -Filter "amd_fidelityfx_*.dll" -File -ErrorAction SilentlyContinue
foreach ($Dll in $FidelityFxDlls) {
    Copy-Item $Dll.FullName $PackageDir
}

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

Project Launcher:
  Double-click Run_Launcher.bat to create a project from an installed template.
  When HamunEditor.exe is present, newly created projects open directly in the editor shell.

Editor:
  HamunEditor.exe can open a Project.hamunproject file directly.
  The editor includes a live HamunRenderer glTF scene viewport.
  Use Renderer > Auto / DirectX 12 / DirectX 11 to compare the same scene with live FPS and frame time.

Recommended renderer test:
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
  - HamunLauncher project browser
  - HamunEditor native editor shell and project loading
  - live HamunRenderer glTF scene viewport with DX12/DX11 switching and FPS/frame-time display
  - data-driven Template catalog and project creation workflow
  - Blank Project technical template
  - renderer-owned RenderGraph and indexed scene submission
  - DX12 / DX11 compute pipelines, storage buffers and storage textures
  - double-buffered frame resources
  - glTF metallic/roughness PBR using GGX/Schlick
  - HDR scene target, motion vectors and temporal GPU resources
  - AMD FSR SDK/runtime/provider/context bridge
  - FSR provider enumeration
  - HamunHair native .tfx loader
  - guide-strand GPU storage upload
  - guide-strand simulation ComputePass
  - official TressFX 4.1.0 header bridge validation
  - card-hair compatibility fallback

The standard package does not bundle external AMD FSR runtime DLLs. The
separate Hamun-FSR-DX12-Dev package bundles the official signed AMD runtime
for live testing on a physical DX12 GPU.

DX12 is the primary Windows backend.
DX11 is the compatibility backend.

Shader note:
Hamun uses DXC Shader Model 6 when dxcompiler.dll is available.
Otherwise this bootstrap build falls back to the Windows D3DCompile path.
"@ | Set-Content -Encoding UTF8 (Join-Path $PackageDir "README_TEST.txt")

$FsrRuntimeBundled = Test-Path (Join-Path $PackageDir "amd_fidelityfx_loader_dx12.dll")

@"
FSR Runtime Bundled: $FsrRuntimeBundled
Template Launcher Bundled: $(Test-Path (Join-Path $PackageDir "HamunLauncher.exe"))
HamunEditor Bundled: $(Test-Path (Join-Path $PackageDir "HamunEditor.exe"))
"@ | Add-Content -Encoding UTF8 (Join-Path $PackageDir "README_TEST.txt")

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
