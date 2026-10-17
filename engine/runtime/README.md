# Hamun Runtime — GLB Geometry Pipeline (first integration milestone)

This is the first path from a real GLB asset to independent Vulkan rendering.
It currently uses a conversion step rather than native runtime GLB parsing.

## Windows x64 commands

First build the runtime:

```bat
cd /d D:\HamunDev\Hamun
git pull --ff-only origin feature/blender-source-integration
cmake --build build/hamun-runtime --config Release
```

Export any static mesh from Hamun Editor as GLB and place it at
`D:\HamunDev\Hamun\sample.glb`. Convert it:

```bat
py -3 engine\runtime\tools\glb_to_hmesh.py sample.glb build\hamun-runtime\Release\scene.hmesh
build\hamun-runtime\Release\HamunRuntime.exe
```

If `scene.hmesh` is present beside `HamunRuntime.exe`, the runtime displays
the converted mesh with a basic soil-style material and ground plane.
Without that file it displays the previous four-sphere material laboratory.

**Limits:** This first reader accepts embedded GLB 2.0 triangle meshes with
FLOAT32 POSITION/NORMAL and standard unsigned index formats. It combines
primitive meshes, but does not currently apply glTF node transforms, import
PBR textures/materials, process skins, animation, extensions, or read external
buffer files. Ground shadows on imported geometry are not yet implemented.
The geometry pipeline must be validated on the user's Vega 56 before being
considered complete.

## Next milestone

Import full glTF scene graphs and PBR materials, then connect the Hamun Editor
Play workflow to a selected scene and a standalone runtime process.

---

## v0.8 ground and moving-light shadow test

- Added a flat ground mesh to the existing indexed Vulkan vertex/index buffers.
- Added hard shadows on that ground cast by the four preview spheres, evaluated through analytical ray/sphere tests in the fragment shader.
- Light positions 1/2 can be moved with J/L (X), U/O (Y), I/K (Z).
- F6 cycles Unlimited, 30, 60, 120, 144 FPS caps.
- The existing material spheres and transparent HUD are retained.

**Scope:** This first shadow pass only handles the four known spheres on the floor. It is not a general Shadow Map implementation and will not shadow arbitrary imported GLB geometry. For arbitrary meshes, implement shadow depth passes and per-light shadow maps next.

### Test on Vega 56

```bat
cd /d D:\HamunDev\Hamun
git pull --ff-only origin feature/blender-source-integration
cmake --build build/hamun-runtime --config Release
build\hamun-runtime\Release\HamunRuntime.exe
```

Check floor visibility and moving shadow silhouettes when moving each light. No engine/Editor rebuild is required.

# HamunRuntime v0.3 — GPU Mesh Rendering

Standalone Win32 + Vulkan renderer, independent of the Blender-based Hamun Editor.

## Build (VS2022 x64 Native Tools Command Prompt)

Requires LunarG Vulkan SDK (with `glslc`), CMake and Visual Studio C++.

```bat
cd /d D:\HamunDev\Hamun
git pull --ff-only origin feature/blender-source-integration
cmake --build build/hamun-runtime --config Release
build\hamun-runtime\Release\HamunRuntime.exe
```

## Features under test

- Vertex data is supplied through a Vulkan vertex buffer instead of hardcoded shader geometry.
- Indices reside in a Vulkan index buffer and are used with `vkCmdDrawIndexed`.
- Three instances of a cube mesh have independent transforms using push constants.
- Depth buffer, surface resize support, and basic orbit camera are retained.
- Arrow keys rotate the view, W/S change view distance.

## Important

Source changes are committed, but v0.3 has **not yet been compiled or run** on the target system. Confirm build, depth ordering, orbit controls, resize, and clean exit before considering it validated. v0.2 remains the last known-good version.

Future: split Vulkan backend out of `main.cpp`, introduce proper per-scene camera/object transforms and GLB mesh loading.
