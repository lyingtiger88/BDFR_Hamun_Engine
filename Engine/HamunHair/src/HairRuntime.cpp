#include <Hamun/Hair/HairRuntime.hpp>
#include <Hamun/Hair/TressFxSdkBridge.hpp>

namespace Hamun::Hair {

HairRuntimePlan BuildRuntimePlan(
    const RHI::IBackend& backend) noexcept
{
    HairRuntimePlan plan;

    const RHI::Capabilities& caps =
        backend.Caps();

    plan.computeSupported =
        caps.compute;

    plan.asyncComputeSupported =
        caps.asyncCompute;

    plan.bindlessSupported =
        caps.bindless;

    plan.apiSupportedByTressFX =
        backend.Type() ==
            RHI::BackendType::D3D12 ||
        backend.Type() ==
            RHI::BackendType::Vulkan;

    plan.tressfxSdkLinked =
        QueryTressFxSdkInfo().compiled;

    plan.strandSimulationAvailable =
        plan.apiSupportedByTressFX &&
        plan.computeSupported &&
        plan.tressfxSdkLinked;

    plan.strandRenderingAvailable =
        plan.strandSimulationAvailable;

    plan.selectedPath =
        plan.strandRenderingAvailable
            ? HairRenderPath::Strand
            : HairRenderPath::Cards;

    return plan;
}

std::string_view HairRenderPathName(
    HairRenderPath path) noexcept
{
    switch (path) {
        case HairRenderPath::Strand:
            return "Strand";

        case HairRenderPath::Cards:
            return "Cards";
    }

    return "Cards";
}

} // namespace Hamun::Hair
