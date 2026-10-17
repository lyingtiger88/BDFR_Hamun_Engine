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
