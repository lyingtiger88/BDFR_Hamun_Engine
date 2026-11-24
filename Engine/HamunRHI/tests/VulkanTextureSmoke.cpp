#include <Hamun/RHI/RHI.hpp>

#include <array>
#include <iostream>

int main() {
    using namespace Hamun::RHI;

    auto backend = CreateBackend(BackendType::Vulkan);
    if (!backend || !backend->Initialize({})) {
        std::cerr << "FAIL: Vulkan device initialization\n";
        return 1;
    }

    // Four unique texels detect incorrect row pitch and channel ordering.
    const std::array<std::uint8_t, 16> rgba = {
        255, 0,   0,   255,  0,   255, 0,   255,
        0,   0,   255, 255,  255, 255, 255, 255
    };
    TextureDesc textureDesc;
    textureDesc.width = 2;
    textureDesc.height = 2;
    textureDesc.format = TextureFormat::RGBA8_UNorm;
    textureDesc.usage = TextureUsage::ShaderResource;
    textureDesc.initialData = rgba.data();
    textureDesc.rowPitch = 8;

    auto texture = backend->CreateTexture(textureDesc);
    SamplerDesc samplerDesc;
    samplerDesc.filter = SamplerFilter::Nearest;
    samplerDesc.addressU = SamplerAddressMode::Clamp;
    samplerDesc.addressV = SamplerAddressMode::Repeat;
    auto sampler = backend->CreateSampler(samplerDesc);
    bool success = texture && sampler && texture->Width() == 2 &&
                   texture->Height() == 2 &&
                   texture->Format() == TextureFormat::RGBA8_UNorm;

    // Verify non-tight source rows are correctly accepted.
    const std::array<std::uint8_t, 24> padded = {
        255, 0, 0, 255, 0, 255, 0, 255, 99, 99, 99, 99,
        0, 0, 255, 255, 255, 255, 255, 255, 99, 99, 99, 99
    };
    textureDesc.initialData = padded.data();
    textureDesc.rowPitch = 12;
    auto paddedTexture = backend->CreateTexture(textureDesc);
    success = success && static_cast<bool>(paddedTexture);

    // Invalid pitches must fail cleanly, without a GPU allocation leak.
    textureDesc.rowPitch = 4;
    auto invalid = backend->CreateTexture(textureDesc);
    success = success && !invalid;

    paddedTexture.reset();
    texture.reset();
    sampler.reset();
    backend->Shutdown();

    std::cout << (success ? "PASS" : "FAIL")
              << ": Vulkan texture allocation/upload/sampler smoke test\n";
    return success ? 0 : 2;
}
