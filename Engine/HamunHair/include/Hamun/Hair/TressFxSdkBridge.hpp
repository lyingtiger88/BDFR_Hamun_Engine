#pragma once

#include <cstdint>

namespace Hamun::Hair {

struct TressFxSdkInfo {
    bool compiled = false;
    std::uint32_t headerMajor = 0;
    std::uint32_t headerMinor = 0;
    std::uint32_t headerPatch = 0;
};

TressFxSdkInfo QueryTressFxSdkInfo() noexcept;

} // namespace Hamun::Hair
