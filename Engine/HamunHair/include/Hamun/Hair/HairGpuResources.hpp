#pragma once

#include <Hamun/Hair/TfxAsset.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <cstddef>
#include <memory>

namespace Hamun::Hair {

class HairGpuResources {
public:
    bool Initialize(
        RHI::IBackend& backend,
        const TfxAsset& asset);

    void Reset() noexcept;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return
            guidePositions_ &&
            simulatedPositions_;
    }

    [[nodiscard]] RHI::IBuffer* GuidePositions() noexcept
    {
        return guidePositions_.get();
    }

    [[nodiscard]] RHI::IBuffer* SimulatedPositions() noexcept
    {
        return simulatedPositions_.get();
    }

    [[nodiscard]] RHI::IBuffer* StrandUv() noexcept
    {
        return strandUv_.get();
    }

    [[nodiscard]] std::size_t VertexCount() const noexcept
    {
        return vertexCount_;
    }

    [[nodiscard]] std::size_t StrandCount() const noexcept
    {
        return strandCount_;
    }

private:
    std::unique_ptr<RHI::IBuffer>
        guidePositions_;

    std::unique_ptr<RHI::IBuffer>
        simulatedPositions_;

    std::unique_ptr<RHI::IBuffer>
        strandUv_;

    std::size_t vertexCount_ = 0;
    std::size_t strandCount_ = 0;
};

} // namespace Hamun::Hair
