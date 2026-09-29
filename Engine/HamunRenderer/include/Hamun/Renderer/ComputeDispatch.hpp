#pragma once

#include <Hamun/RHI/RHI.hpp>

#include <cstdint>
#include <vector>

namespace Hamun::Renderer {

struct ComputeDispatch {
    RHI::IPipeline* pipeline = nullptr;
    std::vector<RHI::IBuffer*> storageBuffers;
    std::uint32_t groupCountX = 1;
    std::uint32_t groupCountY = 1;
    std::uint32_t groupCountZ = 1;
};

} // namespace Hamun::Renderer
