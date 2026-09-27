# Grit -> Hamun migration

Hamun does not patch the historical Grit renderer in place. It preserves the
useful open-world concepts and rebuilds the runtime around modern interfaces.

## Retained ideas
- demand-driven background resource loading
- distance-based streaming priority
- resource residency/reclamation
- open-world-first runtime organization
- script-friendly gameplay architecture

## Replaced/extended
- legacy graphics coupling -> HamunRHI
- DX9-era assumptions -> DX12 / Vulkan / OpenGL ES feature tiers
- nearest-only load priority -> predictive velocity-aware streaming
- old build files -> CMake + C++20
- single-precision world positioning -> cell + local coordinates
- ad-hoc visual logic -> HamunGraph bytecode/IR architecture

## First mapping
| Grit | Hamun |
|---|---|
| BackgroundLoader / Demand | HamunWorld::StreamingScheduler |
| DiskResource | future HamunAsset residency system |
| legacy gfx backend | HamunRHI |
| Lua gameplay bindings | future HamunScript + HamunGraph bindings |
