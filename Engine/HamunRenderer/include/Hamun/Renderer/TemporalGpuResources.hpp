#pragma once

#include <Hamun/RHI/RHI.hpp>

#include <cstdint>
#include <memory>

namespace Hamun::Renderer {

struct TemporalGpuResourcesDesc {
    std::uint32_t renderWidth = 1;
    std::uint32_t renderHeight = 1;
    std::uint32_t displayWidth = 1;
    std::uint32_t displayHeight = 1;
};

class TemporalGpuResources {
public:
    bool Initialize(
        RHI::IBackend& backend,
        const TemporalGpuResourcesDesc& desc);

    void Reset() noexcept;

    [[nodiscard]] RHI::ITexture* HistoryColor() noexcept
    {
        return historyColor_.get();
    }

    [[nodiscard]] RHI::ITexture* MotionVectors() noexcept
    {
        return motionVectors_.get();
    }

    [[nodiscard]] RHI::ITexture* LinearDepth() noexcept
    {
        return linearDepth_.get();
    }

    [[nodiscard]] RHI::ITexture* ReactiveMask() noexcept
    {
        return reactiveMask_.get();
    }

    [[nodiscard]] RHI::ITexture* UpscaledColor() noexcept
    {
        return upscaledColor_.get();
    }

    [[nodiscard]] const TemporalGpuResourcesDesc& Desc() const noexcept
    {
        return desc_;
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return
            historyColor_ &&
            motionVectors_ &&
            linearDepth_ &&
            reactiveMask_ &&
            upscaledColor_;
    }

private:
    TemporalGpuResourcesDesc desc_{};

    std::unique_ptr<RHI::ITexture>
        historyColor_;

    std::unique_ptr<RHI::ITexture>
        motionVectors_;

    std::unique_ptr<RHI::ITexture>
        linearDepth_;

    std::unique_ptr<RHI::ITexture>
        reactiveMask_;

    std::unique_ptr<RHI::ITexture>
        upscaledColor_;
};

} // namespace Hamun::Renderer
