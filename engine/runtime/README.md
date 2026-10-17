# HamunRuntime v0.2 — Interactive Vulkan Cube

Native Windows x64 standalone rendering prototype. No Blender dependency.

## Build (VS2022 x64 Native Tools Command Prompt)

Requires the LunarG Vulkan SDK (including `glslc`), CMake and Visual Studio C++ tools.

```bat
cd /d D:\HamunDev\Hamun
cmake -S engine/runtime -B build/hamun-runtime -G "Visual Studio 17 2022" -A x64
cmake --build build/hamun-runtime --config Release
build\hamun-runtime\Release\HamunRuntime.exe
```

Shader binaries are compiled into the `shaders` directory beside the executable.

## Controls

- Left/right arrows: rotate cube horizontally
- Up/down arrows: rotate cube vertically
- W/S: zoom in/out
- Window resize: rebuild swapchain and depth resources

## Status

Implements Vulkan instance, graphics/present queue, swapchain, depth buffer, graphics pipeline, a 36-vertex indexed-by-shader cube, per-frame camera push constants, and basic error handling.

This is an isolated rendering milestone, **not** a complete game runtime or Editor integration. First test the build, camera controls, occlusion, and resizing on your Vega 56.
