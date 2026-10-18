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
HAMUN_API const char* hamun_renderer_last_error();
}
