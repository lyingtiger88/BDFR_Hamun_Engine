#include <Hamun/Renderer/TemporalGpuResources.hpp>

#include <algorithm>

namespace Hamun::Renderer {
namespace {

std::unique_ptr<RHI::ITexture> CreateTemporalTexture(
    RHI::IBackend& backend,
    std::uint32_t width,
    std::uint32_t height,
    RHI::TextureFormat format,
    RHI::TextureUsage usage =
        RHI::TextureUsage::ShaderResource |
        RHI::TextureUsage::Storage)
{
    RHI::TextureDesc desc;
    desc.width =
        std::max(
            width,
            1u);
    desc.height =
        std::max(
            height,
            1u);
    desc.format =
        format;
    desc.usage =
        usage;

    return
        backend.CreateTexture(
            desc);
}

} // namespace

bool TemporalGpuResources::Initialize(
    RHI::IBackend& backend,
    const TemporalGpuResourcesDesc& desc)
{
    Reset();

    desc_.renderWidth =
        std::max(
            desc.renderWidth,
            1u);

    desc_.renderHeight =
        std::max(
            desc.renderHeight,
            1u);

    desc_.displayWidth =
        std::max(
            desc.displayWidth,
            1u);

    desc_.displayHeight =
        std::max(
            desc.displayHeight,
            1u);

    sceneColor_ =
        CreateTemporalTexture(
            backend,
            desc_.renderWidth,
            desc_.renderHeight,
            RHI::TextureFormat::RGBA16_Float,
            RHI::TextureUsage::ShaderResource |
            RHI::TextureUsage::Storage |
            RHI::TextureUsage::RenderTarget);

    sceneDepth_ =
        CreateTemporalTexture(
            backend,
            desc_.renderWidth,
            desc_.renderHeight,
            RHI::TextureFormat::R32_Float,
            RHI::TextureUsage::ShaderResource |
            RHI::TextureUsage::DepthStencil);

    historyColor_ =
        CreateTemporalTexture(
            backend,
            desc_.displayWidth,
            desc_.displayHeight,
            RHI::TextureFormat::RGBA16_Float);

    motionVectors_ =
        CreateTemporalTexture(
            backend,
            desc_.renderWidth,
            desc_.renderHeight,
            RHI::TextureFormat::RG16_Float,
            RHI::TextureUsage::ShaderResource |
            RHI::TextureUsage::Storage |
            RHI::TextureUsage::RenderTarget);

    linearDepth_ =
        CreateTemporalTexture(
            backend,
            desc_.renderWidth,
            desc_.renderHeight,
            RHI::TextureFormat::R32_Float);

    reactiveMask_ =
        CreateTemporalTexture(
            backend,
            desc_.renderWidth,
            desc_.renderHeight,
            RHI::TextureFormat::RGBA8_UNorm);

    upscaledColor_ =
        CreateTemporalTexture(
            backend,
            desc_.displayWidth,
            desc_.displayHeight,
            RHI::TextureFormat::RGBA16_Float);

    if (!IsValid()) {
        Reset();
        return false;
    }

    return true;
}

void TemporalGpuResources::Reset() noexcept
{
    sceneColor_.reset();
    sceneDepth_.reset();
    historyColor_.reset();
    motionVectors_.reset();
    linearDepth_.reset();
    reactiveMask_.reset();
    upscaledColor_.reset();
    desc_ = {};
}

} // namespace Hamun::Renderer
