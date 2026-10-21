#pragma once
#include <windows.h>
#include <cstdint>
#ifdef HAMUN_RENDERER_BUILD
#define HAMUN_API __declspec(dllexport)
#else
#define HAMUN_API __declspec(dllimport)
#endif
extern "C" {
struct HamunRendererInfo { uint32_t abiVersion; uint32_t vulkanVersion; uint32_t gpuVendor; char deviceName[256]; };
HAMUN_API int hamun_renderer_probe(HamunRendererInfo* info);
// Offscreen Vulkan F12 integration proof: opaque clear to RGBA8, no scene geometry yet.
HAMUN_API int hamun_renderer_offscreen_clear(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount);
HAMUN_API int hamun_renderer_mesh_preview(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount);
HAMUN_API int hamun_renderer_mesh_camera(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,const float* viewProjection16);
// Light layout: 12 floats per light: type(0 sun,1 point,2 spot,3 area),
// position xyz, direction xyz, color rgb, energy, range.
// Initial CPU lighting bridge; shader-side Vulkan PBR is still pending.
HAMUN_API int hamun_renderer_mesh_lit(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* viewProjection16,const float* lights12,uint32_t lightCount);
// Optional 3-float world-space normal per triangle, indexed by triangle order.
// Existing hamun_renderer_mesh_lit ABI is unchanged.
HAMUN_API int hamun_renderer_mesh_lit_normals(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* viewProjection16,const float* lights12,uint32_t lightCount,const float* faceNormals3);
// GPU rasterized scene with depth testing, direct Sun lighting and RGBA readback.
// Error buffer receives a diagnostic when Vulkan cannot initialize.
HAMUN_API int hamun_gpu_viewport_render(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* viewProjection16,const float* lights12,uint32_t lightCount,const float* faceNormals3,
    char* error,uint32_t errorCapacity);
// Hardware-rasterized Sun shadows. Geometry pass and PCF run on Vulkan GPU.
// CPU shadow renderer remains an error fallback while this path is validated.
HAMUN_API int hamun_gpu_viewport_render_shadow(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* viewProjection16,const float* lights12,uint32_t lightCount,const float* faceNormals3,
    char* error,uint32_t errorCapacity,uint32_t shadowResolution,float softness,float biasScale);
// Versioned UV-capable shadow bridge (UV pairs indexed by the expanded vertex buffer).
// Existing render entry points remain available as a fallback.
HAMUN_API int hamun_gpu_viewport_render_shadow_uv(uint32_t width,uint32_t height,
    uint8_t* rgba,uint64_t byteCount,const float* positions,uint32_t vertexCount,
    const uint32_t* indices,uint32_t indexCount,const float* viewProjection16,
    const float* lights12,uint32_t lightCount,const float* faceNormals3,
    const float* vertexUV2,char* error,uint32_t errorCapacity,
    uint32_t shadowResolution,float softness,float biasScale);
// Optional selectable shadow-map quality; existing renderer ABI is preserved.
HAMUN_API int hamun_renderer_mesh_lit_normals_quality(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* viewProjection16,const float* lights12,uint32_t lightCount,const float* faceNormals3,
    uint32_t shadowResolution);
HAMUN_API int hamun_renderer_mesh_lit_normals_settings(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* viewProjection16,const float* lights12,uint32_t lightCount,const float* faceNormals3,
    uint32_t shadowResolution,float softness,float biasScale);
HAMUN_API const char* hamun_renderer_last_error();
}
