#include <Hamun/Renderer/FrameResources.hpp>

namespace Hamun::Renderer {

bool FrameResources::Initialize(
    RHI::IBackend& backend,
    const FrameResourcesDesc& desc)
{
    Reset();

    if (desc.frameCount == 0 ||
        desc.constantBufferCount == 0 ||
        desc.constantBufferSize == 0) {
        return false;
    }

    frameCount_ = desc.frameCount;
    constantBufferCount_ =
        desc.constantBufferCount;
    constantBufferSize_ =
        desc.constantBufferSize;

    const std::size_t totalBufferCount =
        static_cast<std::size_t>(
            frameCount_) *
        constantBufferCount_;

    constantBuffers_.reserve(
        totalBufferCount);

    RHI::BufferDesc bufferDesc;
    bufferDesc.size =
        constantBufferSize_;
    bufferDesc.usage =
        RHI::BufferUsage::Constant;

    for (std::size_t i = 0;
         i < totalBufferCount;
         ++i) {
        auto buffer =
            backend.CreateBuffer(
                bufferDesc);

        if (!buffer) {
            Reset();
            return false;
        }

        constantBuffers_.push_back(
            std::move(buffer));
    }

    return true;
}

void FrameResources::Reset() noexcept
{
    constantBuffers_.clear();
    frameCount_ = 0;
    activeFrameIndex_ = 0;
    constantBufferCount_ = 0;
    constantBufferSize_ = 0;
}

bool FrameResources::SelectFrame(
    std::uint32_t frameIndex) noexcept
{
    if (frameIndex >= frameCount_)
        return false;

    activeFrameIndex_ =
        frameIndex;

    return true;
}

std::size_t FrameResources::BufferIndex(
    std::size_t slot) const noexcept
{
    return
        static_cast<std::size_t>(
            activeFrameIndex_) *
        constantBufferCount_ +
        slot;
}

RHI::IBuffer* FrameResources::ConstantBuffer(
    std::size_t slot) noexcept
{
    if (slot >= constantBufferCount_)
        return nullptr;

    const std::size_t index =
        BufferIndex(slot);

    if (index >= constantBuffers_.size())
        return nullptr;

    return constantBuffers_[index].get();
}

const RHI::IBuffer* FrameResources::ConstantBuffer(
    std::size_t slot) const noexcept
{
    if (slot >= constantBufferCount_)
        return nullptr;

    const std::size_t index =
        BufferIndex(slot);

    if (index >= constantBuffers_.size())
        return nullptr;

    return constantBuffers_[index].get();
}

bool FrameResources::UpdateConstantBuffer(
    std::size_t slot,
    const void* data,
    std::uint64_t size,
    std::uint64_t offset)
{
    RHI::IBuffer* buffer =
        ConstantBuffer(slot);

    if (!buffer ||
        size == 0 ||
        offset + size >
            constantBufferSize_) {
        return false;
    }

    return buffer->Update(
        data,
        size,
        offset);
}

} // namespace Hamun::Renderer
