# BDFR Hamun Engine

> **A modern, scalable game engine for large open worlds, high-performance rendering, and visual gameplay authoring.**

**BDFR Hamun Engine** is an experimental, next-generation game engine project focused on large-scale open worlds, modern low-level graphics APIs, modular engine architecture, and a complete node-based visual scripting environment.

The project takes inspiration from the open-world and streaming philosophy of engines such as **Grit Engine**, while rebuilding the core architecture for modern hardware and APIs.

> **Project status:** Pre-Alpha / RHI Foundation  
> **Repository:** `BDFR_Hamun_Engine`

---

## Language | زبان

**Choose your language / زبان صفحه را انتخاب کنید:**

- 🇬🇧 [English](README.md#english)
- 🇮🇷 [فارسی](README.fa.md)

---

<a id="english"></a>

## English

### Why “Hamun”?

**Hamun (هامون)** is a word strongly associated with the landscape and identity of the Balochistan/Sistan region and with large natural expanses and lakes. The name reflects the engine's focus on **large worlds, open environments, scale, and freedom**.

### Project Vision

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

### Editor Roadmap

The native Hamun 3D editor roadmap, including the Blender-inspired modeling workflow, is maintained here:

- [Editor Roadmap (English + فارسی)](docs/EDITOR_ROADMAP.md)

### Easy Android Builds

Android packaging is designed to be a one-command workflow rather than a manual Android Studio setup process.

**Windows**
```bat
BuildAndroid.bat
```

Build, install, and launch on a connected device:
```bat
BuildAndroid.bat --install
```

The build helper automatically detects the Android SDK, verifies required NDK/CMake packages, provisions Gradle when necessary, invokes the same Hamun CMake project used by the engine, packages the native application, and copies the APK to:
```text
dist/android/HamunGame-debug.apk
```

The initial Android target uses a native activity and defaults to `arm64-v8a`. The current bootstrap validates the native Hamun runtime; Vulkan/OpenGL ES surface rendering will be connected in the Android graphics milestone.

See [Android build documentation](docs/ANDROID_BUILD.md) for toolchain details.

### DirectX 11 Compatibility

Hamun includes a separate **Direct3D 11 compatibility backend** behind the same HamunRHI interfaces used by DirectX 12.

The compatibility backend currently supports textured indexed meshes, vertex/index/constant buffers, 2D textures and shader-resource views, samplers, depth buffers, HLSL shaders, input layouts, rasterizer/depth state, and swap-chain presentation.

Feature-level negotiation currently attempts `11_1 -> 11_0 -> 10_1 -> 10_0`.

The renderer does not reduce the DirectX 12/Vulkan design to DX11 limitations. Advanced systems such as bindless rendering, mesh shaders, ray tracing, and explicit asynchronous compute remain higher capability-tier features.

### Graphics API Targets

| Platform / API | Target |
|---|---|
| DirectX 12 | Primary Windows backend |
| DirectX 11 | Windows compatibility backend |
| Vulkan | Primary cross-platform backend |
| OpenGL ES | Compatibility / mobile / embedded backend |
| Metal | Planned Apple backend |
| Console SDK backends | Planned, using official platform SDKs/devkits |

### High-Level Architecture

```text
BDFR Hamun Engine
├── HamunCore
├── HamunPlatform
├── HamunProject
├── HamunAssets
├── HamunRHI
├── HamunRenderer
├── HamunUpscale
├── HamunHair
├── HamunGraph
├── HamunWorld
└── Tools
    ├── HamunLauncher
    └── HamunEditor
```

See the repository source and documentation for implementation details.

---

## Contributing

Contributions and focused bug reports are welcome. Please describe reproduction steps and your environment when reporting issues.
