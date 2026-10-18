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
HAMUN_API const char* hamun_renderer_last_error();
}
