#pragma once

#include <Hamun/RHI/RHI.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Hamun::Renderer {

struct FrameResourcesDesc {
    std::uint32_t frameCount = 2;
    std::size_t constantBufferCount = 0;
    std::uint64_t constantBufferSize = 0;
};

class FrameResources {
public:
    bool Initialize(
        RHI::IBackend& backend,
        const FrameResourcesDesc& desc);

    void Reset() noexcept;

    bool SelectFrame(
        std::uint32_t frameIndex) noexcept;

    [[nodiscard]] std::uint32_t FrameCount() const noexcept
    {
        return frameCount_;
    }

    [[nodiscard]] std::size_t ConstantBufferCount() const noexcept
    {
        return constantBufferCount_;
    }

    [[nodiscard]] std::uint32_t ActiveFrameIndex() const noexcept
    {
        return activeFrameIndex_;
    }

    RHI::IBuffer* ConstantBuffer(
        std::size_t slot) noexcept;

    const RHI::IBuffer* ConstantBuffer(
        std::size_t slot) const noexcept;

    bool UpdateConstantBuffer(
        std::size_t slot,
        const void* data,
        std::uint64_t size,
        std::uint64_t offset = 0);

private:
    [[nodiscard]] std::size_t BufferIndex(
        std::size_t slot) const noexcept;

    std::uint32_t frameCount_ = 0;
    std::uint32_t activeFrameIndex_ = 0;
    std::size_t constantBufferCount_ = 0;
    std::uint64_t constantBufferSize_ = 0;

    std::vector<std::unique_ptr<RHI::IBuffer>>
        constantBuffers_;
};

} // namespace Hamun::Renderer
