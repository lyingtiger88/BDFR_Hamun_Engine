# BDFR Hamun Engine

> **A modern, scalable game engine for large open worlds, high-performance rendering, and visual gameplay authoring.**

**BDFR Hamun Engine** is an experimental, next-generation game engine project focused on large-scale open worlds, modern low-level graphics APIs, modular engine architecture, and a complete node-based visual scripting environment.

The project takes inspiration from the open-world and streaming philosophy of engines such as **Grit Engine**, while rebuilding the core architecture for modern hardware and APIs.

> **Project status:** Pre-Alpha / Foundation stage  
> **Repository:** `BDFR_Hamun_Engine`

---

## Why “Hamun”?

**Hamun (هامون)** is a word strongly associated with the landscape and identity of the Balochistan/Sistan region and with large natural expanses and lakes. The name reflects the engine's focus on **large worlds, open environments, scale, and freedom**.

---

## Project Vision

Hamun Engine is intended to become a modular engine capable of powering games ranging from smaller 3D projects to very large seamless worlds.

The main goals are:

- Large-scale **open-world streaming**
- Modern explicit graphics backends
- High-performance, multi-threaded runtime
- GPU-driven rendering
- Scalable renderer for desktop and mobile-class hardware
- Fully integrated visual scripting
- Modular physics, AI, audio, networking, and animation systems
- Data-driven asset pipeline
- Extensible plugin architecture
- Editor-first development workflow
- Source-friendly and version-control-friendly assets
- Long-term support for very large worlds and high-speed traversal

---

## Graphics API Targets

Hamun Engine is being designed around a dedicated **Render Hardware Interface (RHI)**.

| Platform / API | Target |
|---|---|
| DirectX 12 | Primary Windows backend |
| Vulkan | Primary cross-platform backend |
| OpenGL ES | Compatibility / mobile / embedded backend |

The renderer will not be restricted to the lowest common denominator. Hardware capabilities will be exposed through feature tiers so advanced GPUs can use features unavailable on lower-end devices.

Planned examples include:

- Compute workloads
- Async compute where supported
- Indirect drawing
- Bindless/resource indexing where available
- GPU scene management
- GPU culling
- Mesh/cluster-based rendering paths where supported
- Ray tracing as an optional high-end feature
- Fallback rendering paths for OpenGL ES-class devices

---

## High-Level Architecture

```text
BDFR Hamun Engine
|
+-- HamunCore
|   +-- Memory
|   +-- Containers
|   +-- Math
|   +-- Threading
|   +-- Job System
|   +-- Logging
|   +-- Profiling
|
+-- HamunPlatform
|   +-- Windows
|   +-- Linux
|   +-- Android
|
+-- HamunRHI
|   +-- DirectX 12
|   +-- Vulkan
|   +-- OpenGL ES
|
+-- HamunRenderer
|   +-- Render Graph
|   +-- Materials
|   +-- Lighting
|   +-- Shadows
|   +-- GPU Scene
|   +-- Culling
|   +-- Post Processing
|
+-- HamunWorld
|   +-- World Partition
|   +-- Streaming
|   +-- Terrain
|   +-- Foliage
|   +-- LOD / HLOD
|   +-- Large World Coordinates
|
+-- HamunGraph
|   +-- Visual Scripting
|   +-- Graph Compiler
|   +-- Bytecode VM
|   +-- Debugger
|   +-- Reflection
|
+-- HamunPhysics
+-- HamunAI
+-- HamunAnimation
+-- HamunAudio
+-- HamunNetwork
+-- HamunEditor
+-- HamunTools
```

---

## HamunGraph — Visual Scripting

**HamunGraph** is the planned visual scripting system for the engine.

The goal is not merely to reproduce a simple node editor, but to provide a complete gameplay programming environment comparable in scope to modern visual scripting systems while remaining tightly integrated with native C++.

### Planned HamunGraph features

- Execution and data pins
- Variables and typed data
- Functions
- Events
- Custom events
- Branches and loops
- Arrays, sets, and maps
- Structures and enums
- Subgraphs
- Macros
- Interfaces
- State machines
- Async and latent nodes
- Timelines
- C++ reflection integration
- Plugin-defined nodes
- Hot reload
- Breakpoints
- Step Into / Step Over / Step Out
- Variable watches
- Call stack
- Execution tracing
- Graph profiler
- Text-friendly serialization
- Visual graph diff
- Bytecode execution
- Native compilation path
- Multi-thread-aware graphs
- Network/replication nodes

Future specialized graph types are expected to include:

- Gameplay Graph
- AI Graph
- Behavior Graph
- Animation Graph
- Material Graph
- UI Graph
- Audio Graph
- Physics Graph
- Vehicle Graph
- Mission Graph
- Procedural Generation Graph

---

## Large World System

Hamun Engine is being architected for worlds far larger than a conventional single loaded level.

The planned hierarchy is:

```text
World
  -> Region
      -> Cell
          -> SubCell
              -> Entities
```

Only the required part of the world should remain resident around active players, cameras, simulation zones, or network-relevant areas.

Planned large-world technologies include:

- Cell-based world partition
- Asynchronous world streaming
- Predictive streaming
- Velocity-aware streaming
- Altitude-aware streaming
- Streaming for high-speed aircraft and vehicles
- CPU asset cache
- GPU residency management
- Background decompression
- LOD and HLOD
- Far-distance impostors
- Camera-relative rendering
- High-precision world positions
- Origin management/rebasing where necessary

A major design objective is to allow high-speed traversal without forcing the entire world to remain in memory.

---

## Renderer Direction

The renderer is planned around a modern Render Graph and GPU-driven architecture.

```text
World / ECS
    |
    v
GPU Scene
    |
    v
Frustum + Occlusion Culling
    |
    v
LOD / HLOD Selection
    |
    v
Indirect Command Generation
    |
    v
Render Graph
    |
    v
DX12 / Vulkan / GLES
```

Planned rendering systems include:

- Physically Based Rendering
- Forward and deferred-capable paths
- Render Graph
- GPU Scene
- GPU frustum culling
- GPU occlusion culling
- Indirect rendering
- Shadow system
- Atmospheric rendering
- Sky and day/night cycle
- Terrain rendering
- Foliage rendering
- Post-processing
- Temporal systems
- Virtualized/streamed resources where appropriate

---

## Shader Pipeline

The preferred long-term shader workflow is centered around **HLSL** with backend compilation/translation for supported APIs.

```text
HLSL
 |
 +--> DXC --> DXIL ------> DirectX 12
 |
 +--> DXC --> SPIR-V ----> Vulkan
 |
 +--> SPIR-V / translation path --> GLSL ES --> OpenGL ES
```

The exact shader toolchain may evolve as the renderer is implemented.

---

## Physics

Physics will be accessed through an engine-level abstraction instead of being tightly coupled to a single backend.

Possible backends include:

- Bullet
- Jolt Physics
- Future/custom physics backend

This architecture is intended to allow specialized physics systems to integrate without rewriting gameplay code.

---

## Scripting Strategy

Hamun Engine is planned to support multiple levels of gameplay authoring:

- **C++** — engine systems and performance-critical gameplay
- **HamunGraph** — visual gameplay programming
- **Lua** — lightweight scripting, missions, events, tooling, and modding

Additional managed scripting options may be evaluated later.

---

# Roadmap

The roadmap is intentionally milestone-based. Features may move between phases as engine architecture is validated.

## Phase 0 — Foundation **(Current Focus)**

- [ ] Establish repository structure
- [ ] C++20/23 baseline
- [ ] CMake build system
- [ ] Coding conventions
- [ ] Core types and utilities
- [ ] Logging system
- [ ] Assertion/error system
- [ ] Math library foundation
- [ ] Platform abstraction
- [ ] Unit-test foundation
- [ ] CI build validation

**Milestone:** clean engine skeleton that builds reliably.

---

## Phase 1 — Core Runtime

- [ ] Memory allocators
- [ ] Threading primitives
- [ ] Job system
- [ ] File system abstraction
- [ ] Timing system
- [ ] Profiling foundation
- [ ] Module/plugin loader
- [ ] Reflection foundation
- [ ] Application lifecycle
- [ ] Window/input abstraction

**Milestone:** launch a native Hamun application window with stable runtime services.

---

## Phase 2 — HamunRHI + DirectX 12

- [ ] RHI resource model
- [ ] Command queues
- [ ] Command lists
- [ ] Buffers
- [ ] Textures
- [ ] Samplers
- [ ] Descriptor management
- [ ] Pipeline states
- [ ] Shader loading
- [ ] Swap chain
- [ ] Synchronization
- [ ] GPU memory management
- [ ] First triangle
- [ ] First textured mesh

**Milestone:** functional DirectX 12 backend through HamunRHI.

---

## Phase 3 — Vulkan + OpenGL ES

### Vulkan

- [ ] Vulkan device/backend
- [ ] Resource creation
- [ ] Descriptor system
- [ ] Pipeline system
- [ ] Synchronization
- [ ] Presentation

### OpenGL ES

- [ ] GLES compatibility backend
- [ ] Feature/capability detection
- [ ] Reduced rendering feature tier
- [ ] Mobile/embedded validation

**Milestone:** one renderer architecture operating across all three target APIs.

---

## Phase 4 — Modern Renderer

- [ ] Render Graph
- [ ] Mesh system
- [ ] Material system
- [ ] PBR
- [ ] Camera system
- [ ] Lighting
- [ ] Shadowing
- [ ] GPU Scene
- [ ] GPU culling
- [ ] Indirect rendering
- [ ] Post-processing
- [ ] Sky/atmosphere
- [ ] Debug renderer
- [ ] Renderer profiler

**Milestone:** production-oriented real-time 3D renderer foundation.

---

## Phase 5 — HamunWorld

- [ ] Entity/world model
- [ ] Region/Cell/SubCell partition
- [ ] Async cell streaming
- [ ] Streaming scheduler
- [ ] Predictive streaming
- [ ] Large-world coordinate system
- [ ] Camera-relative rendering
- [ ] LOD
- [ ] HLOD
- [ ] Impostors
- [ ] Terrain
- [ ] Foliage
- [ ] World serialization
- [ ] Streaming profiler

**Milestone:** seamless large-world traversal.

---

## Phase 6 — HamunGraph

- [ ] Graph asset format
- [ ] Node editor
- [ ] Pin/type system
- [ ] Variables
- [ ] Functions
- [ ] Events
- [ ] Branches and loops
- [ ] Reflection binding
- [ ] Graph compiler
- [ ] Intermediate representation
- [ ] Bytecode VM
- [ ] Runtime execution
- [ ] Breakpoints
- [ ] Execution trace
- [ ] Watch variables
- [ ] Hot reload
- [ ] Text-based graph serialization
- [ ] Graph profiler

**Milestone:** create gameplay without writing native C++.

---

## Phase 7 — Gameplay Systems

- [ ] Physics abstraction
- [ ] Character framework
- [ ] Animation system
- [ ] Navigation
- [ ] AI framework
- [ ] Behavior/state graphs
- [ ] Audio engine
- [ ] Input mapping
- [ ] Save/load system
- [ ] Vehicle framework

---

## Phase 8 — HamunEditor

- [ ] Main editor shell
- [ ] Scene/world viewport
- [ ] Hierarchy/outliner
- [ ] Inspector/details panel
- [ ] Asset browser
- [ ] Material editor
- [ ] HamunGraph editor
- [ ] Terrain tools
- [ ] World-partition visualization
- [ ] Profiler UI
- [ ] Debugging tools
- [ ] Plugin management UI

**Milestone:** build complete projects without relying on external scene-authoring tools for routine workflows.

---

## Phase 9 — Networking

- [ ] Client/server architecture
- [ ] Entity replication
- [ ] RPC system
- [ ] Relevancy
- [ ] Network prediction
- [ ] Lag compensation foundations
- [ ] Replication Graph
- [ ] Large-world multiplayer streaming

---

## Phase 10 — Optimization & Production Tooling

- [ ] Asset cooking
- [ ] Asset dependency graph
- [ ] Incremental builds
- [ ] Shader cache
- [ ] Pipeline cache
- [ ] Background asset processing
- [ ] Crash reporting
- [ ] Performance capture
- [ ] Memory diagnostics
- [ ] Packaging
- [ ] Automated testing
- [ ] Documentation pipeline

---

## First Playable Technical Target

The first meaningful engine milestone is intentionally small:

- Native window
- C++ runtime
- HamunCore
- HamunRHI
- DirectX 12
- Shader compilation
- Mesh rendering
- Texture rendering
- Camera
- Basic PBR material
- Basic ECS/world representation
- Basic asset loading
- Basic streaming prototype

Once this foundation is stable, Vulkan, OpenGL ES, HamunWorld, and HamunGraph can grow on top of a validated architecture.

## Development Philosophy

Hamun Engine follows a few core principles:

1. **Modernize instead of patching legacy assumptions.**
2. **Keep rendering APIs behind a clean RHI.**
3. **Design large-world support from the beginning.**
4. **Treat visual scripting as a first-class programming environment.**
5. **Prefer modular systems over hard dependencies.**
6. **Keep source assets and graphs version-control friendly.**
7. **Expose hardware capability tiers instead of limiting the engine to the weakest backend.**
8. **Profile first-class systems from their earliest implementation.**

---

## Current Status

Hamun Engine is at the **foundation stage**.

The architecture and long-term technical direction are being defined before production systems are implemented. APIs, formats, module names, and roadmap ordering may change significantly during early development.

---

## Contributing

Contribution guidelines will be added once the initial architecture, coding conventions, and core module boundaries are stabilized.

---

## License

A project license will be selected and documented before external distribution of production code.

---

**BDFR Hamun Engine**  
*Build large worlds. Keep the core modular.*
