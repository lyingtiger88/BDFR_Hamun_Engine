# Hamun Native Material System — Blender/Eevee/Cycles interoperability

## Evidence and direction
Blender 5.4 Principled BSDF is layered OpenPBR; EEVEE is realtime raster/PBR approximation, Cycles is path tracing. They share authored shader nodes, not equivalent light-transport execution. Hamun will read supported Blender nodes and translate into its own **MaterialDesc**; it will NOT execute Blender's Eevee or Cycles renderer.

Authoritative documentation:
- https://docs.blender.org/manual/en/dev/render/shader_nodes/shader/principled.html
- https://developer.blender.org/docs/features/eevee/
- https://developer.blender.org/docs/features/eevee/pipelines/deferred/
- https://docs.blender.org/manual/en/latest/render/eevee/limitations/limitations.html
- https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc
- https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures

## Implemented foundation (this commit)
`engine/runtime/src/MaterialCore.hpp` defines backend-independent material semantics:
`baseColor`, `metallic`, `roughness`, `emissive`, `alphaMode`, alpha cutoff,
double-sided rendering and five texture slots with explicit color encoding.
`sanitize` validates incoming values. It is a **data contract**, not yet wired
into both backend rendering pipelines. No claim of DirectX material rendering.

## Pipeline design
```
Blender 5.4 shader nodes / material slots
    -> graph translator (supported subset, warnings for unsupported nodes)
    -> HamunMaterialDesc + TextureAssets + triangle material IDs
    -> MaterialAssetCache (content hash, version, residency, dirty flags)
    -> Renderer frontend (material index per draw/triangle/submesh)
          |                       |
          v                       v
       Vulkan                  Direct3D 12
    descriptor sets          SRV/sampler/CBV tables
       SPIR-V GLSL              DXIL HLSL
          |                       |
          +--- shared GGX BRDF & rendering validation
```

## Graph translation scope
Phase A: Principled BSDF directly feeding active Material Output. Read defaults
for Base Color, Metallic, Roughness, Alpha, Emission and relevant normal
strength. Follow Image Texture -> Base Color / Metallic / Roughness / Normal Map
/ Emission links, detect UV Map and channel extraction, and classify sRGB
(albedo/emissive) vs linear (data textures). Preserve material slots and
per-triangle material indices. Unsupported procedural networks must report an
explicit warning and preview fallback; do not silently claim fidelity.

Phase B: support Mix, Mapping, RGB Curves and basic Math nodes via baking or
small intermediate expression graph. Tangent basis must preserve UV seams and
handedness. Implement alpha MASK/BLEND and double-sided behavior independently.

Phase C: OpenPBR layers, coat, subsurface skin, transmission, sheen and hair.
Implement targeted realtime approximations first, then optional richer offline
or hardware RT effects. Exact Eevee/Cycles visual parity is not guaranteed.

## GPU and memory
- `VkDescriptorSetLayout` and Vulkan immutable sampler/descriptors are NOT
  interchangeable with D3D12 root signature/descriptor heap bindings.
- Use shared material definition, asset IDs and physical shading equations;
  implement backend resource lifetime/transitions separately.
- Do not allocate image/staging resources on every camera frame. Cache by
  image identity + revision; upload only changed images.
- Per-submesh material binding or material ID buffer is required before claiming
  multi-material support. Current prototype uses one shared image for a scene.
- Keep Vega 56 baseline: Vulkan raster + shadow maps; D3D12 and hardware RT
  remain feature-gated, not assumed. The Material Core must not require RT.

## Release gates
1. C++ compile/static analysis, Python compile, SPIR-V compile.
2. Cube with separate UV islands and checker image; no inversion/misalignment.
3. Two objects, different textures and material factors; exact per-draw isolation.
4. GGX dielectric and metal sphere set vs Eevee/Cycles visual references.
5. Mask/blend/normal/tangent and missing-image fallback.
6. Same scene Vulkan vs Direct3D 12; compare within specified tolerance.
7. PERF_PROFILE at 25/50/75/100% scale on Vega 56, stable frame-to-frame
   output, no shader/shadow regressions.

## Compatibility warning
The currently committed `GpuViewport.cpp` textured entry point and Blender
addon handle a *single shared Base Color image* and allocate Vulkan albedo
resources in the render call. They are prototypes, NOT proof of full material
or efficient multi-material support. Existing shadow/API fallbacks stay in
place until a separate validated renderer frontend can replace them.
