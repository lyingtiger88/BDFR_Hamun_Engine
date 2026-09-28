# Hamun Test v0.2 - Windows Package

Test v0.2 moves the first Hamun demo from a single asset to a small,
asset-driven multi-object scene.

## What's new in v0.2

- glTF scene-node import
- node world transforms
- multiple instances of one mesh
- translation and non-uniform scale from glTF
- vertex normals
- base-color material factor
- simple directional lighting
- ambient lighting
- multiple indexed draw calls per frame
- larger scene for free-camera traversal
- DX12 and DX11 validation of the same scene

## Build locally

```bat
BuildWindowsTest.bat
```

Output:

```text
dist/Hamun_Test_v0.2/
```

## Run

Primary:

```text
Run_DX12.bat
```

Compatibility:

```text
Run_DX11.bat
```

Controls:

- WASD — move
- Q / E — down / up
- right mouse button + mouse — look
- Shift — sprint
- Esc — exit

## CI validation

Before the package is uploaded, GitHub Actions runs the v0.2 scene for three
frames through both DirectX 12 and DirectX 11. Linux module validation and the
Android APK build remain part of the same workflow.

The Windows artifact is published as:

`Hamun-Test-v0.2-Windows-x64`
