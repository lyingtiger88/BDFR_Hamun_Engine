#pragma once
// Hamun Material Core v1: backend-independent authoring/render contract.
// This header intentionally does not depend on Vulkan, Direct3D, or Blender.
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace hamun::material {
enum class AlphaMode : uint32_t { Opaque=0, Mask=1, Blend=2 };
enum class ColorEncoding : uint32_t { Linear=0, SRGB=1 };
enum class TextureSlot : uint32_t {
    BaseColor=0, MetallicRoughness=1, Normal=2, Occlusion=3, Emissive=4, Count=5
};
constexpr uint32_t InvalidTexture=0xffffffffu;
struct TextureBinding {
    uint32_t texture=InvalidTexture;
    uint32_t uvSet=0;
    float scale=1.0f; // Normal strength or occlusion strength
    ColorEncoding encoding=ColorEncoding::Linear;
};
struct MaterialDesc {
    std::array<float,4> baseColor{1.f,1.f,1.f,1.f};
    float metallic=0.0f;
    float roughness=0.5f;
    std::array<float,3> emissive{0.f,0.f,0.f};
    float emissionStrength=1.f;
    float alphaCutoff=0.5f;
    AlphaMode alphaMode=AlphaMode::Opaque;
    bool doubleSided=false;
    std::array<TextureBinding,static_cast<size_t>(TextureSlot::Count)> maps{};
};
inline MaterialDesc sanitize(MaterialDesc m) {
    for(float& v:m.baseColor)v=std::isfinite(v)?std::clamp(v,0.f,1.f):1.f;
    m.metallic=std::isfinite(m.metallic)?std::clamp(m.metallic,0.f,1.f):0.f;
    m.roughness=std::isfinite(m.roughness)?std::clamp(m.roughness,0.045f,1.f):0.5f;
    for(float& v:m.emissive)v=std::isfinite(v)?std::max(0.f,v):0.f;
    m.emissionStrength=std::isfinite(m.emissionStrength)?std::max(0.f,m.emissionStrength):1.f;
    m.alphaCutoff=std::isfinite(m.alphaCutoff)?std::clamp(m.alphaCutoff,0.f,1.f):0.5f;
    for(auto& b:m.maps){
        b.uvSet=std::min(b.uvSet,7u);
        b.scale=std::isfinite(b.scale)?std::max(0.f,b.scale):1.f;
    }
    m.maps[static_cast<size_t>(TextureSlot::BaseColor)].encoding=ColorEncoding::SRGB;
    m.maps[static_cast<size_t>(TextureSlot::Emissive)].encoding=ColorEncoding::SRGB;
    m.maps[static_cast<size_t>(TextureSlot::MetallicRoughness)].encoding=ColorEncoding::Linear;
    m.maps[static_cast<size_t>(TextureSlot::Normal)].encoding=ColorEncoding::Linear;
    m.maps[static_cast<size_t>(TextureSlot::Occlusion)].encoding=ColorEncoding::Linear;
    return m;
}
// All backends consume the same semantics; backend bindings may differ.
static_assert(static_cast<uint32_t>(TextureSlot::Count)==5);
}
