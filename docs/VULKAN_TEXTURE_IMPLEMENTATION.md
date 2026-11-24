# Vulkan texture resource implementation (work in progress)

This branch adds real Vulkan device initialization and a synchronous texture
upload path under HamunRHI. It does **not** yet render textured triangles.

## Implemented
- Instance / physical device / logical graphics device / transfer command pool
- GPU-local VkImage with matching memory allocation and VkImageView
- Staging VkBuffer upload; rowPitch-aware copy into tightly packed staging memory
- UNDEFINED -> TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL transitions
- VkSampler creation (nearest/linear, clamp/repeat)
- Release of staging resources and per-texture RAII cleanup
- Early rejection for invalid dimensions, unsupported depth usage and invalid row pitch
- Headless smoke test executable: HamunVulkanTextureSmoke

## Build on Windows

Requires Vulkan SDK with VULKAN_SDK set and Visual Studio C++ tools.

```powershell
cmake -S . -B build-vulkan -G "Visual Studio 17 2022" -A x64 -DHAMUN_ENABLE_VULKAN=ON
cmake --build build-vulkan --config Release --target HamunVulkanTextureSmoke
.\build-vulkan\Engine\HamunRHI\Release\HamunVulkanTextureSmoke.exe
```

The smoke test validates allocation and the API's successful upload path;
it does not compare GPU-readback texels. Validation layers should be enabled
during the next round of testing.

## Still required before visible Vulkan rendering
1. Vulkan surface and swapchain with image acquire/present and resize handling.
2. Vulkan buffer allocation for vertex/index/constant buffers.
3. Shader compilation to SPIR-V, descriptor set layouts/pools and pipeline layout.
4. SetTexture/SetSampler combined-image-sampler descriptor updates.
5. Render pass and draw commands with proper vertex and UV inputs.
6. Actual pixel comparison tests using GPU readback or captured frames.
7. Depth/render-target support and correct resource state tracking.
8. Comprehensive Vulkan validation and device-loss handling.

**Important:** backend->Shutdown() must only be called after releasing textures
and samplers, and texture destructors must not run while submissions referencing
them are still executing.
