# BDFR Hamun Engine

> **A modern, scalable game engine for large open worlds, high-performance rendering, and visual gameplay authoring.**

**BDFR Hamun Engine** is an experimental, next-generation game engine project focused on large-scale open worlds, modern low-level graphics APIs, modular engine architecture, and a complete node-based visual scripting environment.

The project takes inspiration from the open-world and streaming philosophy of engines such as **Grit Engine**, while rebuilding the core architecture for modern hardware and APIs.

> **Project status:** Pre-Alpha / RHI Foundation  
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

## Easy Android Builds

Android packaging is designed to be a one-command workflow rather than a manual
Android Studio setup process.

### Windows

```bat
BuildAndroid.bat
```

Build, install, and launch on a connected device:

```bat
BuildAndroid.bat --install
```

The build helper automatically detects the Android SDK, verifies the required
NDK/CMake packages, provisions Gradle when necessary, invokes the same Hamun
CMake project used by the engine, packages the native application, and copies
the APK to:

```text
dist/android/HamunGame-debug.apk
```

The initial Android target uses a native activity and defaults to `arm64-v8a`.
The current bootstrap validates the native Hamun runtime; Vulkan/OpenGL ES
surface rendering will be connected in the Android graphics milestone.

See `docs/ANDROID_BUILD.md` for the complete toolchain details.

---

## DirectX 11 Compatibility

Hamun includes a separate **Direct3D 11 compatibility backend** behind the same
HamunRHI interfaces used by DirectX 12.

The compatibility backend currently supports:

- textured indexed meshes
- vertex/index/constant buffers
- 2D textures and shader-resource views
- samplers
- depth buffers
- HLSL shaders
- input layouts
- rasterizer/depth state
- swap-chain presentation

Feature-level negotiation currently attempts:

`11_1 -> 11_0 -> 10_1 -> 10_0`

The renderer does not reduce the DirectX 12/Vulkan design to DX11 limitations.
Advanced systems such as bindless rendering, mesh shaders, ray tracing and
explicit asynchronous compute remain higher capability-tier features.

Windows CI renders the same textured RHI test on both DX12 and DX11.

---

## Graphics API Targets

Hamun Engine is being designed around a dedicated **Render Hardware Interface (RHI)**.

| Platform / API | Target |
|---|---|
| DirectX 12 | Primary Windows backend |
| DirectX 11 | Windows compatibility backend |
| Vulkan | Primary cross-platform backend |
| OpenGL ES | Compatibility / mobile / embedded backend |
| Metal | Planned Apple backend for macOS / iOS / iPadOS / visionOS |
| Console SDK backends | Planned RHI backends using official platform SDKs/devkits |

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
+-- HamunProject
|   +-- Template Catalog
|   +-- Project Creation
|   +-- Data-driven Template Manifests
|
+-- HamunRHI
|   +-- DirectX 12
|   +-- DirectX 11
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
+-- HamunUpscale
|   +-- AMD FSR runtime loader
|   +-- Upscale quality/resolution policy
|   +-- DX12 SDK bridge
|
+-- HamunHair
|   +-- TressFX bridge
|   +-- Strand simulation
|   +-- Strand rendering
|   +-- Hair LOD / card fallback
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

## Asset Pipeline & First Interactive Scene

Hamun now includes an initial file-based asset pipeline:

- glTF 2.0 mesh loading through `cgltf`
- PNG/JPG decoding through `stb_image`
- indexed mesh data
- UV coordinates
- optional normals
- base-color texture discovery from glTF materials
- real file-backed texture loading
- a repository test glTF scene and PNG texture
- WASD free-camera movement
- Q/E vertical movement
- Shift sprint
- RMB + mouse look
- Escape to exit
- FPS/camera-position debug output
- glTF scene-node/world-transform import
- multi-instance scene rendering
- normal-based directional lighting
- ambient lighting
- base-color material factors
- multiple indexed draw calls per frame

The same asset-driven scene is smoke-tested through both **DirectX 12** and
**DirectX 11** on Windows. Linux validates the cross-platform modules and the
Android APK pipeline remains green.

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

## Phase 0 — Foundation

- [x] Establish repository structure
- [x] C++20 baseline
- [x] CMake build system
- [ ] Coding conventions
- [ ] Core types and utilities
- [x] Logging system
- [ ] Assertion/error system
- [ ] Math library foundation
- [x] Platform abstraction
- [ ] Unit-test foundation
- [x] CI build validation

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

## Phase 2 — HamunRHI + DirectX 12 / DirectX 11 **(Current Graphics Focus)**

- [x] RHI resource model
- [x] Command queues
- [x] Command lists
- [x] Buffers (initial upload/vertex path)
- [x] Textures (initial 2D RGBA8 path)
- [x] Samplers
- [x] Descriptor management (bootstrap shader-visible heaps)
- [x] Pipeline states
- [x] Shader loading/compilation (DXC Shader Model 6 primary, temporary fallback retained)
- [x] Swap chain
- [x] Synchronization
- [ ] GPU memory management
- [x] First triangle through backend-neutral RHI
- [x] First textured mesh
- [x] Indexed 3D cube with depth and constant buffers
- [x] DirectX 11 compatibility backend
- [x] DX11 Feature Level 11_1 / 11_0 / 10_1 / 10_0 negotiation
- [x] Shared textured-mesh smoke test on DX12 and DX11

**Milestone:** functional DirectX 12 backend through HamunRHI.

Current verified Windows RHI test path:

```text
Win32 Window
  -> HamunRHI
  -> Vertex / Index / Constant Buffers
  -> DX12: DXC HLSL Shader Model 6 + explicit texture upload/descriptors
  -> DX11: HLSL SM5/SM4 compatibility path + SRV/sampler binding
  -> Textured Indexed Mesh
  -> Depth Test
  -> Present
```

---

## Phase 3 — Vulkan + OpenGL ES + Metal / Apple

### Vulkan

- [ ] Vulkan device/backend
- [ ] Resource creation
- [ ] Descriptor system
- [ ] Pipeline system
- [ ] Synchronization
- [ ] Presentation

### OpenGL ES

- [x] Android native build/package bootstrap
- [ ] Android Vulkan/GLES surface rendering
- [ ] GLES compatibility backend
- [ ] Feature/capability detection
- [ ] Reduced rendering feature tier
- [ ] Mobile/embedded validation

### Metal / Apple

- [ ] Metal backend foundation
- [ ] macOS application target
- [ ] iOS device target
- [ ] iOS simulator target
- [ ] Xcode/CMake presets
- [ ] one-command Apple build scripts
- [ ] Apple asset/resource packaging
- [ ] macOS CI validation

**Milestone:** one renderer architecture operating across desktop, mobile and Apple graphics backends.

---

## Phase 4 — Modern Renderer

- [x] Render Graph foundation (pass registration / compile / execute)
- [x] Mesh system (initial file-backed glTF path)
- [x] Material system foundation (runtime Material + MaterialInstance + glTF parameters)
- [x] PBR foundation (glTF metallic/roughness factors + GGX/Schlick direct lighting)
- [x] Camera system (free camera)
- [x] Lighting foundation (directional + ambient)
- [ ] Shadowing
- [ ] GPU Scene
- [ ] GPU culling
- [ ] Indirect rendering
- [ ] Post-processing
- [x] Temporal frame-state foundation (history + Halton jitter + render/display sizing)
- [x] Temporal GPU allocation foundation (history color, motion, depth, reactive, upscaled color)
- [x] Offscreen HDR/depth target allocation foundation (DX12/DX11)
- [x] HDR offscreen scene + tonemap/present pass
- [x] Motion-vector render target / pass foundation
- [ ] Full temporal GPU resource/barrier integration
- [x] Compute pipeline foundation (DX12/DX11 + RenderGraph ComputePass)
- [x] Structured storage/UAV buffer foundation (DX12/DX11)
- [x] Compute storage texture/UAV foundation (DX12/DX11, HDR/motion formats)
- [x] Controlled native RHI interop bridge for external GPU SDKs
- [x] AMD FSR runtime loader / ABI bridge foundation
- [x] FSR post-scene dispatch path (SDK-enabled DX12 builds)
- [x] Package/validate official FSR runtime in dedicated Windows FSR development build
- [x] Hair runtime interface / capability planner
- [x] AMD TressFX 4.1.0 header bridge / CI integration
- [x] Hair simulation ComputePass foundation
- [ ] Hair physical constraints / collision / LOD
- [x] Card-hair fallback selection foundation
- [ ] Sky/atmosphere
- [ ] Debug renderer
- [ ] Renderer profiler

**Milestone:** production-oriented real-time 3D renderer foundation.

---

## Phase 5 — HamunWorld

- [ ] Entity/world model
- [ ] Region/Cell/SubCell partition
- [ ] Async cell streaming
- [x] Streaming scheduler
- [x] Predictive streaming foundation
- [x] Large-world coordinate foundation
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
- [x] Bytecode VM
- [x] Runtime execution foundation
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

- [x] Project launcher / template-selection foundation
- [x] Main editor shell
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
- [x] Packaging foundation (Windows test artifact + Android APK)
- [ ] Automated testing
- [ ] Documentation pipeline

---

## Download Latest Windows Test

The current portable Windows technical test is **Hamun Test v0.5**.

Release asset:

```text
Hamun-Test-v0.5-Windows-x64.zip
```

Open the repository's **Releases** section and download the v0.5 Windows x64 ZIP.
The package contains separate DX12 and DX11 launchers.

---

## Initial Test v0.1 Status

The first interactive desktop test path is now functional:

- [x] Native Win32 window
- [x] DirectX 12
- [x] DirectX 11 compatibility backend
- [x] HLSL shader compilation
- [x] Vertex / index / constant buffers
- [x] Texture + sampler
- [x] Depth buffer
- [x] glTF 2.0 file import
- [x] PNG/JPG image decode
- [x] File-backed textured mesh
- [x] Free camera
- [x] WASD + mouse input
- [x] HamunGraph VM bootstrap test
- [x] World streaming bootstrap test
- [x] Windows DX12/DX11 CI smoke tests
- [x] Android APK CI build

The portable Windows package is now automated through GitHub Actions.

### Test v0.2

- [x] glTF scene-node transforms
- [x] Multiple scene instances
- [x] Translation and scale from glTF
- [x] Vertex-normal lighting
- [x] Directional + ambient light foundation
- [x] Material base-color factor
- [x] Multi-draw scene rendering on DX12
- [x] Multi-draw scene rendering on DX11
- [x] Portable Windows v0.2 artifact

### Test v0.3

- [x] RenderGraph runtime foundation
- [x] MainRenderPass bootstrap
- [x] HamunRenderer -> RenderGraph execution path
- [x] Runtime Material + MaterialInstance foundation
- [x] Base-color / metallic / roughness material parameters
- [x] GPU adapter and memory reporting on DX11 / DX12
- [x] DX11 capability report
- [x] DX12 resource-binding capability query
- [x] DX12 mesh-shader capability query
- [x] DX12 ray-tracing capability query
- [x] Windows DX12/DX11 CI regression validation
- [x] Linux foundation CI validation
- [x] Android APK CI validation
- [x] Local Windows v0.3 validation by project owner

Next renderer work: move actual scene draw submission into renderer-owned
passes, then add frame resources / compute support required by PBR, FSR and
TressFX.

### Test v0.4

- [x] RenderGraph moved into HamunRenderer; HamunGraph remains visual scripting
- [x] Renderer-owned indexed scene draw submission
- [x] Double-buffered frame resources
- [x] DX12 compute shader / compute pipeline / dispatch
- [x] DX11 compute shader / compute pipeline / dispatch on supported feature levels
- [x] RenderGraph ComputePass before MainRenderPass
- [x] glTF metallic and roughness factor import
- [x] GGX/Schlick metallic-roughness PBR direct-light foundation
- [x] Temporal frame history foundation
- [x] Halton jitter generation for future temporal upscaling
- [x] HamunHair module
- [x] TressFX runtime capability planning
- [x] Card-hair fallback selection
- [x] DX12/DX11 Windows smoke tests
- [x] Linux and Android CI validation
- [x] Project-owner local Windows v0.4 validation

The external AMD FSR and TressFX SDKs remain intentionally unbundled in v0.4.
The engine-side prerequisites are now in place for the next integration phase.

### Test v0.5

- [x] HamunProject module
- [x] Data-driven template manifests
- [x] Windows HamunLauncher project browser
- [x] Template selection/details UI
- [x] Project name/location workflow
- [x] Project creation with token substitution
- [x] CI template creation smoke test
- [x] Launcher + Templates included in Windows package
- [x] Native TressFX .tfx parser
- [x] Guide-strand data uploaded to GPU storage buffers
- [x] Hair simulation ComputePass foundation
- [x] Official TressFX 4.1.0 headers compiled in dedicated CI
- [x] AMD FidelityFX SDK 2.3.0 runtime/provider/context validation
- [x] FSR provider enumeration and portable 3.1.5 preference
- [x] Separate FSR DX12 development package with official signed AMD DLLs
- [ ] Live FSR dispatch validation on a physical DX12 GPU
- [ ] Final production template lineup

The only installed project template in v0.5 is a technical **Blank Project**.
It exists to validate the full launcher/catalog/create-project workflow while
the production template categories are still being decided.

### Development after Test v0.5

- [x] Native Windows HamunEditor shell
- [x] `.hamunproject` project-file parser in HamunProject
- [x] Launcher -> HamunEditor handoff after project creation
- [x] Responsive Outliner / Viewport / Inspector / Asset Browser shell layout
- [x] Project metadata and top-level project asset listing
- [x] Headless HamunEditor project-loading smoke test in Windows CI
- [ ] Live renderer-backed editor viewport
- [ ] Editable hierarchy / inspector data model
- [ ] Full asset browser indexing and import workflow

The production template lineup remains intentionally undecided; editor work can
continue independently using the Blank Project validation template.

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

Hamun Engine is at the **Pre-Alpha RHI foundation stage**. DirectX 12 and DirectX 11 now render the same indexed textured 3D mesh through the public HamunRHI interfaces. DX12 uses the modern explicit path with DXC Shader Model 6 support, while DX11 provides a compatibility path down through Feature Level 10_0. Windows, Linux and Android CI validation are in place. The Windows toolchain now also includes a tested HamunEditor shell that opens generated `.hamunproject` projects and establishes the first editor workspace layout.

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
