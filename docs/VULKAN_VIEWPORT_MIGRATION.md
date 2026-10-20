# Hamun Vulkan-first viewport migration

## Baseline (HumanGen, 2026-10-20)
- 37,033 vertices, 73,490 triangles, 1584 × 887 render target.
- CPU Sun + shadow fallback: approx. 308–552 ms in the DLL (variable).
- At 25% scale, CPU shadow rasterization approx. 25–52 ms.
- The old `dll_ms=0.07` for unshadowed Vulkan excluded the GPU call. The bridge profiler was corrected in commit 91c4fe6.
- Blender geometry caching is already implemented. A new geometry cache measurement with a static camera often reads 0 ms.

## Implementation milestones (in order)

### 1. Correct GPU direct Sun path and measurements — IMPLEMENTED (needs Windows validation)
- If no active Sun casts shadows, use `hamun_gpu_viewport_render` for direct Sun shading, including an unshadowed Sun.
- If any active Sun casts shadows, retain the existing CPU renderer until the GPU shadow sampler is functional.
- Profile the GPU function call (including Vulkan setup, submission, readback) in native_ms.
- Check `RENDER_ROUTE=VULKAN_DIRECT_SUN_NO_SHADOW` with Blender Sun Cast Shadow disabled.
- Verify visually that direct lighting still responds to Sun rotation, energy and color.

### 2. Full Vulkan shadow rendering — NOT YET IMPLEMENTED
- Design GPU input for Sun shadow settings (shadow map resolution, softness, bias) while retaining the old exported ABI.
- Generate robust scene light-space bounds and a Sun view-projection matrix; test with negative scales and multiple objects.
- Build a depth-only shadow render pass (one shadow map per supported shadow-casting Sun), create `VK_FORMAT_D32_SFLOAT` sampleable depth image and its image view.
- Use the dedicated `viewport_sun_depth.vert` already staged in CMake.
- Add a descriptor set containing a shadow image/sampler and update `viewport_direct.frag` for slope-aware bias and bounded PCF.
- Synchronize shadow depth writes with fragment sampling using valid Vulkan image layout transitions and access barriers.
- Add the new Vulkan shadow entry point, preserving legacy `hamun_gpu_viewport_render` callers. Update the Blender addon to prefer GPU shadows and fall back to CPU when that call fails.
- Tests: shadow on/off, Sun angular size 0–180 degrees, resolution 64/256/1024/4096, softness and bias ranges, mirrored scale, camera motion, resource cleanup on exceptions.

### 3. Persistent GPU resources — NOT YET IMPLEMENTED
- Keep Vulkan instance, device, graphics queue, render pass, shader modules, pipeline, descriptors, memory allocators and command pools alive between frames.
- Rebuild only size-dependent attachments on viewport resize and shadow resources on shadow resolution changes.
- Upload vertex/index buffers only for geometry changes, and update light/camera uniforms independently.
- Remove per-frame full-scene hashing once robust dirty flags/generation numbers are available.
- GPU timing: device setup, uploads, shadow pass, main pass, download, Blender display.

### 4. Materials and image textures — NOT YET IMPLEMENTED
- Blender: inspect material node graph and follow Base Color links for supported Principled BSDF materials.
- Export per-triangle UVs (seams must be respected), material indices and image data; handle texture color space.
- Extend Vulkan vertex attributes, descriptors, samplers and fragment shader; test with basic UV image texture.
- HumanGen requires explicit support/testing for nested shader groups, procedural nodes, multiple hair/eye/skin materials, UDIM and normal/roughness mapping. Never assume simple Base Color reproduces the full skin shader.

## Release gates
1. Windows Release CMake build succeeds and SPIR-V shaders are staged beside the DLL.
2. Static geometry with camera motion renders without flicker; shadow coverage matches Blender/CPU approximately.
3. All existing Shadow Map, Softness, Bias controls visibly work.
4. No invisible shadow bypass: route log reports GPU shadow or CPU fallback truthfully.
5. Benchmark apples-to-apples at 1584×887 / 100% and identical Sun settings, then compare native_ms and total_ms.
6. Keep CPU shadow fallback until the above checks pass.
