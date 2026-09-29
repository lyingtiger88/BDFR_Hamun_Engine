#pragma once

#include <Hamun/RHI/RHI.hpp>

#include <string_view>

namespace Hamun::Hair {

enum class HairRenderPath {
    Strand,
    Cards
};

struct HairRuntimePlan {
    bool apiSupportedByTressFX = false;
    bool computeSupported = false;
    bool asyncComputeSupported = false;
    bool bindlessSupported = false;
    bool tressfxSdkLinked = false;
    bool strandSimulationAvailable = false;
    bool strandRenderingAvailable = false;
    HairRenderPath selectedPath = HairRenderPath::Cards;
};

HairRuntimePlan BuildRuntimePlan(
    const RHI::IBackend& backend) noexcept;

std::string_view HairRenderPathName(
    HairRenderPath path) noexcept;

} // namespace Hamun::Hair
