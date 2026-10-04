# Hamun Test v0.5

v0.5 expands Hamun from a renderer/runtime test into the beginning of a full
engine project workflow.

## Main Windows package

```text
Hamun-Test-v0.5-Windows-x64.zip
```

Contains:

- HamunLauncher.exe
- Templates/
- Run_Launcher.bat
- HamunSandbox.exe
- Run_DX12.bat
- Run_DX11.bat
- renderer test assets

## FSR development package

```text
Hamun-FSR-DX12-Dev-Windows-x64.zip
```

Contains the FSR-enabled DX12 sandbox plus the official signed AMD FidelityFX
runtime DLLs from FidelityFX SDK v2.3.0.

CI verifies runtime loading, provider enumeration, provider selection and
context creation. GitHub hosted Windows runners expose Microsoft Hyper-V Video,
so live GPU dispatch is diagnostic/non-blocking there and must be validated on
a physical DX12 GPU.

## Hair/TressFX progress

- native .tfx parser
- guide position/strand UV ingestion
- GPU structured-storage upload
- simulated-position storage
- first guide-strand ComputePass
- official TressFX 4.1.0 header bridge CI

The current simulation pass establishes the real GPU execution path. Physical
constraints, wind, collisions, LOD and strand rendering build on top of it.

## Templates

The launcher is functional, but the final template lineup is intentionally not
decided yet. v0.5 includes only the technical Blank Project template.
