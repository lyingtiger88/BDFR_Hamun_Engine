#include <Hamun/Upscale/FsrRuntime.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(HAMUN_WITH_FSR_SDK) && defined(_WIN32)
#include <ffx_api.h>
#include <ffx_upscale.h>
#include <dx12/ffx_api_dx12.h>
#endif

namespace Hamun::Upscale {
namespace {

#if defined(_WIN32)

HMODULE LoadModule(
    const std::filesystem::path& path)
{
    return
        LoadLibraryW(
            path.wstring().c_str());
}

void UnloadModule(void*& module)
{
    if (!module)
        return;

    FreeLibrary(
        static_cast<HMODULE>(
            module));

    module = nullptr;
}

void* Resolve(
    void* module,
    const char* name)
{
    if (!module)
        return nullptr;

    return reinterpret_cast<void*>(
        GetProcAddress(
            static_cast<HMODULE>(
                module),
            name));
}

#endif

} // namespace

FsrRuntime::~FsrRuntime()
{
    Shutdown();
}

bool FsrRuntime::Probe(
    RHI::IBackend& backend,
    const std::filesystem::path& runtimeDirectory)
{
    Shutdown();

    status_.backendCompatible =
        backend.Type() ==
            RHI::BackendType::D3D12;

    status_.nativeInteropAvailable =
        backend.NativeDeviceHandle() != nullptr &&
        backend.NativeCommandQueueHandle() != nullptr;

#if defined(HAMUN_WITH_FSR_SDK)
    status_.sdkHeadersEnabled = true;
#else
    status_.sdkHeadersEnabled = false;
#endif

#if defined(_WIN32)
    if (!status_.backendCompatible ||
        !status_.nativeInteropAvailable) {
        status_.detail =
            "FSR runtime requires Hamun's Direct3D 12 backend with native interop.";
        return false;
    }

    const auto loaderPath =
        runtimeDirectory /
        "amd_fidelityfx_loader_dx12.dll";

    const auto upscalerPath =
        runtimeDirectory /
        "amd_fidelityfx_upscaler_dx12.dll";

    loaderModule_ =
        LoadModule(
            loaderPath);

    status_.loaderDllLoaded =
        loaderModule_ != nullptr;

    upscalerModule_ =
        LoadModule(
            upscalerPath);

    status_.upscalerDllLoaded =
        upscalerModule_ != nullptr;

    if (!status_.loaderDllLoaded ||
        !status_.upscalerDllLoaded) {
        status_.detail =
            "Official FSR loader/upscaler DLLs are not present beside the executable.";
        return false;
    }

    createContextFn_ =
        Resolve(
            loaderModule_,
            "ffxCreateContext");

    destroyContextFn_ =
        Resolve(
            loaderModule_,
            "ffxDestroyContext");

    dispatchFn_ =
        Resolve(
            loaderModule_,
            "ffxDispatch");

    queryFn_ =
        Resolve(
            loaderModule_,
            "ffxQuery");

    configureFn_ =
        Resolve(
            loaderModule_,
            "ffxConfigure");

    status_.apiFunctionsResolved =
        createContextFn_ &&
        destroyContextFn_ &&
        dispatchFn_ &&
        queryFn_ &&
        configureFn_;

    status_.detail =
        status_.apiFunctionsResolved
            ? "AMD FSR API runtime found and ABI entry points resolved."
            : "AMD FSR DLLs loaded, but required ABI entry points were missing.";

    return
        status_.apiFunctionsResolved;
#else
    (void)backend;
    (void)runtimeDirectory;

    status_.detail =
        "FSR runtime probing is currently implemented for Windows DX12.";

    return false;
#endif
}

bool FsrRuntime::CreateContext(
    RHI::IBackend& backend,
    const FsrContextDesc& desc)
{
    DestroyContext();

#if defined(HAMUN_WITH_FSR_SDK) && defined(_WIN32)
    if (!status_.apiFunctionsResolved ||
        backend.Type() !=
            RHI::BackendType::D3D12 ||
        !backend.NativeDeviceHandle()) {
        return false;
    }

    auto createContext =
        reinterpret_cast<
            PfnFfxCreateContext>(
                createContextFn_);

    if (!createContext)
        return false;

    ffxCreateBackendDX12Desc
        backendDesc{};

    backendDesc.header.type =
        FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;

    backendDesc.device =
        static_cast<ID3D12Device*>(
            backend.NativeDeviceHandle());

    ffxCreateContextDescUpscaleVersion
        versionDesc{};

    versionDesc.header.type =
        FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION;

    versionDesc.version =
        FFX_UPSCALER_VERSION;

    versionDesc.header.pNext =
        &backendDesc.header;

    ffxCreateContextDescUpscale
        upscaleDesc{};

    upscaleDesc.header.type =
        FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;

    upscaleDesc.header.pNext =
        &versionDesc.header;

    upscaleDesc.maxRenderSize.width =
        desc.dimensions.renderWidth;

    upscaleDesc.maxRenderSize.height =
        desc.dimensions.renderHeight;

    upscaleDesc.maxUpscaleSize.width =
        desc.dimensions.displayWidth;

    upscaleDesc.maxUpscaleSize.height =
        desc.dimensions.displayHeight;

    if (desc.highDynamicRange) {
        upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
    }

    if (desc.autoExposure) {
        upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
    }

    if (desc.depthInverted) {
        upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
    }

    if (desc.depthInfinite) {
        upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_DEPTH_INFINITE;
    }

    if (desc.dynamicResolution) {
        upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION;
    }

    if (desc.enableDebugChecking) {
        upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
    }

    ffxContext nativeContext =
        nullptr;

    const ffxReturnCode_t result =
        createContext(
            &nativeContext,
            &upscaleDesc.header,
            nullptr);

    if (result !=
        FFX_API_RETURN_OK) {
        status_.detail =
            "ffxCreateContext returned an error.";
        return false;
    }

    context_ =
        nativeContext;

    status_.contextCreated =
        true;

    status_.detail =
        "AMD FSR upscaler context created.";

    return true;
#else
    (void)backend;
    (void)desc;

    status_.contextCreated = false;

    if (status_.apiFunctionsResolved) {
        status_.detail =
            "FSR runtime is present; rebuild with HAMUN_WITH_FSR_SDK=ON and HAMUN_FSR_SDK_DIR to compile context descriptors.";
    } else {
        status_.detail =
            "FSR context unavailable because the runtime DLLs were not resolved.";
    }

    return false;
#endif
}

void FsrRuntime::DestroyContext() noexcept
{
#if defined(HAMUN_WITH_FSR_SDK) && defined(_WIN32)
    if (context_ &&
        destroyContextFn_) {
        auto destroyContext =
            reinterpret_cast<
                PfnFfxDestroyContext>(
                    destroyContextFn_);

        ffxContext nativeContext =
            context_;

        destroyContext(
            &nativeContext,
            nullptr);
    }
#endif

    context_ = nullptr;
    status_.contextCreated = false;
}

void FsrRuntime::Shutdown() noexcept
{
    DestroyContext();

#if defined(_WIN32)
    UnloadModule(
        upscalerModule_);

    UnloadModule(
        loaderModule_);
#else
    loaderModule_ = nullptr;
    upscalerModule_ = nullptr;
#endif

    createContextFn_ = nullptr;
    destroyContextFn_ = nullptr;
    dispatchFn_ = nullptr;
    queryFn_ = nullptr;
    configureFn_ = nullptr;

    status_ = {};
}

float UpscaleRatio(
    UpscaleQualityMode mode) noexcept
{
    switch (mode) {
        case UpscaleQualityMode::NativeAA:
            return 1.0f;

        case UpscaleQualityMode::Quality:
            return 1.5f;

        case UpscaleQualityMode::Balanced:
            return 1.7f;

        case UpscaleQualityMode::Performance:
            return 2.0f;

        case UpscaleQualityMode::UltraPerformance:
            return 3.0f;
    }

    return 1.0f;
}

UpscaleDimensions BuildUpscaleDimensions(
    std::uint32_t displayWidth,
    std::uint32_t displayHeight,
    UpscaleQualityMode mode) noexcept
{
    UpscaleDimensions dimensions;

    dimensions.displayWidth =
        std::max(
            displayWidth,
            1u);

    dimensions.displayHeight =
        std::max(
            displayHeight,
            1u);

    const float ratio =
        std::max(
            UpscaleRatio(mode),
            1.0f);

    dimensions.renderWidth =
        std::max(
            static_cast<std::uint32_t>(
                std::lround(
                    static_cast<float>(
                        dimensions.displayWidth) /
                    ratio)),
            1u);

    dimensions.renderHeight =
        std::max(
            static_cast<std::uint32_t>(
                std::lround(
                    static_cast<float>(
                        dimensions.displayHeight) /
                    ratio)),
            1u);

    return dimensions;
}

} // namespace Hamun::Upscale
