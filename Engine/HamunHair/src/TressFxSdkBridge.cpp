#include <Hamun/Hair/TressFxSdkBridge.hpp>

#if defined(HAMUN_WITH_TRESSFX_SDK)
#include <TressFX/AMD_TressFX.h>
#include <TressFX/TressFXFileFormat.h>
#endif

namespace Hamun::Hair {

TressFxSdkInfo QueryTressFxSdkInfo() noexcept
{
    TressFxSdkInfo info;

#if defined(HAMUN_WITH_TRESSFX_SDK)
    info.compiled = true;
    info.headerMajor =
        AMD_TRESSFX_VERSION_MAJOR;
    info.headerMinor =
        AMD_TRESSFX_VERSION_MINOR;
    info.headerPatch =
        AMD_TRESSFX_VERSION_PATCH;

    static_assert(
        sizeof(TressFXTFXFileHeader) >=
        sizeof(float) +
        7u * sizeof(unsigned int),
        "Unexpected TressFX .tfx header layout");
#endif

    return info;
}

} // namespace Hamun::Hair
