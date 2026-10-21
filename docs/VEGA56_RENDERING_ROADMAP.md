# Hamun Engine — Material and Rendering Roadmap (Vega 56 baseline)

Status: staged plan; the Vulkan viewport currently uses untextured vertex positions/normals, SceneLights, and shadow mapping. This document does **not** claim texture/PBR/RT features have shipped.

## Baseline and compatibility
- Mandatory baseline: AMD Radeon RX Vega 56, Vulkan rasterization. No hardware RT requirement.
- Preserve current Sun/Point/Spot/Area shadow rendering, Poisson PCF, shadow matrices, viewport overlays, and Blender 5.4 texture-upload compatibility.
- Optional hardware-RT backend must require runtime feature probing (acceleration structure and ray-tracing pipeline or ray query), never silently assume it is available.
- Always supply fallback for unsupported hardware. Keep render path selection explicit in diagnostics.

## 1 — UV and Base Color (next implementation target)
- Blender extraction must use **loop/corner UVs**, not mesh-vertex UVs: duplicate vertices at UV seams, preserve triangle winding and world-space normal transforms.
- New versioned render-bridge ABI rather than altering the existing 12-float light ABI or mesh entry point. Old API stays as fallback during rollout.
- Pass positions/normals/UVs and material indices (or submeshes) with documented byte strides; validate array bounds and index counts.
- Import image textures from connected material Base Color nodes, resolve color space (sRGB for albedo), cache decoded pixels on change, and avoid per-frame uploads.
- Shader: sampled Base Color combined with current lighting and shadow visibility; keep no-texture material fallback.
- Acceptance test: textured cube with distinct UV islands, UV seam test, two textured objects with different materials, moving lights still cast identical shadows.

## 2 — PBR materials
- Metallic-roughness BRDF: GGX normal distribution, Smith visibility, Schlick Fresnel; energy-consistent direct lighting.
- Normal maps (tangent-space and handedness), base-color, metallic, roughness, emissive and alpha modes.
- Linear-light shading, correct sRGB conversion; fallback textures and manageable descriptor layout.
- Validate with reference metal/dielectric spheres and normal-map test under all light types.

## 3 — Vega 56 visual improvements
- Screen-space reflections (SSR) with temporal history and stable fallbacks.
- Screen-space ambient occlusion (SSAO), environment lighting and reflection probes where practical.
- Performance budgets and shader profiling at 25/50/75/100% viewport scales.

## 4 — Ray tracing tiers
- **Tier 0 (Vega 56)**: Vulkan raster + PCF + SSR; optionally bounded compute-based software ray traversal/BVH for targeted effects; not promised real-time for arbitrary scenes.
- **Tier 1 (modern AMD/NVIDIA)**: optional Vulkan ray queries for shadows, gated by device feature support and actual performance measurements.
- **Tier 2**: hardware ray-tracing pipeline and hybrid reflections/lighting when extension support is confirmed.
- No change to Tier 0 when enabling experimental RT. Clear fallback and diagnostic logs.

## Validation and release gates
1. Compile GLSL/SPIR-V and native DLL; Blender addon syntax check.
2. Screenshot parity on Cube/Plane with Point, Sun, Spot, Area; no shadow-position or edge-quality regressions.
3. Shader/base color UV seam tests, missing texture fallback.
4. Multi-frame image persistence and PERF_PROFILE measurements, including conversion/upload cost and overlay anchoring.
5. Only mark each milestone complete after a Windows HamunEditor test.

## Current blocking technical detail
The current Blender bridge builds indexed positions by mesh vertex and face normals by triangle, without UVs. UV seams require changing the vertex layout/indexing across both Blender extraction and C++ Vulkan input pipeline. Simply adding `mesh.uv_layers.active.data[vertex.index]` would be incorrect.
