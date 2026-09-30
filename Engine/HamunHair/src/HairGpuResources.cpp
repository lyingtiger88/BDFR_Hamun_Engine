#include <Hamun/Hair/HairGpuResources.hpp>

namespace Hamun::Hair {

bool HairGpuResources::Initialize(
    RHI::IBackend& backend,
    const TfxAsset& asset)
{
    Reset();

    if (asset.positions.empty() ||
        asset.guideStrandCount == 0 ||
        asset.verticesPerStrand == 0) {
        return false;
    }

    const std::uint64_t positionBytes =
        static_cast<std::uint64_t>(
            asset.positions.size() *
            sizeof(asset.positions.front()));

    RHI::BufferDesc guideDesc;
    guideDesc.size =
        positionBytes;
    guideDesc.usage =
        RHI::BufferUsage::Storage;
    guideDesc.initialData =
        asset.positions.data();
    guideDesc.stride =
        sizeof(asset.positions.front());

    guidePositions_ =
        backend.CreateBuffer(
            guideDesc);

    RHI::BufferDesc simulatedDesc =
        guideDesc;

    simulatedPositions_ =
        backend.CreateBuffer(
            simulatedDesc);

    if (!asset.strandUv.empty()) {
        RHI::BufferDesc uvDesc;
        uvDesc.size =
            static_cast<std::uint64_t>(
                asset.strandUv.size() *
                sizeof(asset.strandUv.front()));

        uvDesc.usage =
            RHI::BufferUsage::Storage;

        uvDesc.initialData =
            asset.strandUv.data();

        uvDesc.stride =
            sizeof(asset.strandUv.front());

        strandUv_ =
            backend.CreateBuffer(
                uvDesc);
    }

    if (!guidePositions_ ||
        !simulatedPositions_ ||
        (!asset.strandUv.empty() &&
         !strandUv_)) {
        Reset();
        return false;
    }

    vertexCount_ =
        asset.positions.size();

    strandCount_ =
        asset.guideStrandCount;

    return true;
}

void HairGpuResources::Reset() noexcept
{
    guidePositions_.reset();
    simulatedPositions_.reset();
    strandUv_.reset();

    vertexCount_ = 0;
    strandCount_ = 0;
}

} // namespace Hamun::Hair
