# HamunRuntime v0.1 — Vulkan Triangle
Native Windows x64 game-runtime prototype; no Blender dependency.
Requires Visual Studio 2022 C++ tools, CMake and LunarG Vulkan SDK with glslc.

From Developer Command Prompt:
```bat
cd /d D:\HamunDev\Hamun
cmake -S engine/runtime -B build/hamun-runtime -G "Visual Studio 17 2022" -A x64
cmake --build build/hamun-runtime --config Release
build\hamun-runtime\Release\HamunRuntime.exe
```
Keep its shaders subdirectory alongside the executable.
This is an isolated first-triangle milestone, not a complete game renderer.
