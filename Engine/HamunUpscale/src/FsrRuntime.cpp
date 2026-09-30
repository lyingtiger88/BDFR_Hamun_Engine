#include <Hamun/Upscale/FsrRuntime.hpp>
#include <Hamun/Core/Log.hpp>

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

#if defined(HAMUN_WITH_FSR_SDK) && defined(_WIN32)
void FsrMessageCallback(
    uint32_t type,
    const wchar_t* message)
{
    if (!message)
        return;

    const int length =
        WideCharToMultiByte(
            CP_UTF8,
            0,
            message,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);

    if (length <= 1)
        return;

    std::string utf8(
        static_cast<std::size_t>(length),
        '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        message,
        -1,
        utf8.data(),
        length,
        nullptr,
        nullptr);

    if (!utf8.empty() &&
        utf8.back() == '\0') {
        utf8.pop_back();
    }

    Core::Log(
        type == FFX_API_MESSAGE_TYPE_ERROR
            ? Core::LogLevel::Error
            : Core::LogLevel::Warning,
        std::string("FSR: ") + utf8);
}

const char* FsrReturnCodeName(
    ffxReturnCode_t code) noexcept
{
    switch (code) {
        case FFX_API_RETURN_OK:
            return "OK";
        case FFX_API_RETURN_ERROR:
            return "ERROR";
        case FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE:
            return "UNKNOWN_DESCTYPE";
        case FFX_API_RETURN_ERROR_RUNTIME_ERROR:
            return "RUNTIME_ERROR";
        case FFX_API_RETURN_NO_PROVIDER:
            return "NO_PROVIDER";
        case FFX_API_RETURN_ERROR_MEMORY:
            return "MEMORY";
        case FFX_API_RETURN_ERROR_PARAMETER:
            return "PARAMETER";
        case FFX_API_RETURN_PROVIDER_NO_SUPPORT_NEW_DESCTYPE:
            return "PROVIDER_NO_SUPPORT_NEW_DESCTYPE";
        default:
            return "UNKNOWN";
    }
}
#endif

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

struct FsrRuntime::Impl {
#if defined(HAMUN_WITH_FSR_SDK) && defined(_WIN32)
    ffxCreateBackendDX12Desc backendDesc{};
    ffxCreateContextDescUpscaleVersion versionDesc{};
    ffxCreateContextDescUpscale upscaleDesc{};
#endif
};

FsrRuntime::FsrRuntime()
    : impl_(
        std::make_unique<Impl>())
{
}

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

    impl_->backendDesc = {};
    impl_->versionDesc = {};
    impl_->upscaleDesc = {};

    impl_->backendDesc.header.type =
        FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;

    impl_->backendDesc.device =
        static_cast<ID3D12Device*>(
            backend.NativeDeviceHandle());

    impl_->versionDesc.header.type =
        FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION;

    impl_->versionDesc.version =
        FFX_UPSCALER_VERSION;

    impl_->versionDesc.header.pNext =
        &impl_->backendDesc.header;

    impl_->upscaleDesc.header.type =
        FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;

    impl_->upscaleDesc.header.pNext =
        &impl_->versionDesc.header;

    impl_->upscaleDesc.maxRenderSize.width =
        desc.dimensions.renderWidth;

    impl_->upscaleDesc.maxRenderSize.height =
        desc.dimensions.renderHeight;

    impl_->upscaleDesc.maxUpscaleSize.width =
        desc.dimensions.displayWidth;

    impl_->upscaleDesc.maxUpscaleSize.height =
        desc.dimensions.displayHeight;

    if (desc.highDynamicRange) {
        impl_->upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
    }

    if (desc.autoExposure) {
        impl_->upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
    }

    if (desc.depthInverted) {
        impl_->upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
    }

    if (desc.depthInfinite) {
        impl_->upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_DEPTH_INFINITE;
    }

    if (desc.dynamicResolution) {
        impl_->upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION;
    }

    if (desc.enableDebugChecking) {
        impl_->upscaleDesc.flags |=
            FFX_UPSCALE_ENABLE_DEBUG_CHECKING;

        impl_->upscaleDesc.fpMessage =
            FsrMessageCallback;
    }

    ffxContext nativeContext =
        nullptr;

    const ffxReturnCode_t result =
        createContext(
            &nativeContext,
            &impl_->upscaleDesc.header,
            nullptr);

    if (result !=
        FFX_API_RETURN_OK) {
        status_.detail =
            std::string(
                "ffxCreateContext failed: ") +
            FsrReturnCodeName(result) +
            " (" +
            std::to_string(result) +
            ").";

        Core::Log(
            Core::LogLevel::Error,
            status_.detail);

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

void FsrRuntime::ConfigureDispatch(
    const FsrDispatchDesc& desc) noexcept
{
    dispatchDesc_ =
        desc;

    dispatchConfigured_ =
        desc.color &&
        desc.depth &&
        desc.motionVectors &&
        desc.output;
}

bool FsrRuntime::Execute(
    RHI::ICommandList& commands)
{
    status_.lastDispatchSucceeded =
        false;

#if defined(HAMUN_WITH_FSR_SDK) && defined(_WIN32)
    if (!ReadyForDispatch()) {
        status_.detail =
            "FSR dispatch skipped: runtime context is not ready.";

        Core::Log(
            Core::LogLevel::Error,
            status_.detail);

        return false;
    }

    if (!dispatchConfigured_) {
        status_.detail =
            "FSR dispatch skipped: required frame resources are incomplete.";

        Core::Log(
            Core::LogLevel::Error,
            status_.detail);

        return false;
    }

    if (!commands.NativeCommandListHandle()) {
        status_.detail =
            "FSR dispatch skipped: native DX12 command list handle is unavailable.";

        Core::Log(
            Core::LogLevel::Error,
            status_.detail);

        return false;
    }

    commands.PrepareTextureForExternalRead(
        *dispatchDesc_.color);

    commands.PrepareTextureForExternalRead(
        *dispatchDesc_.depth);

    commands.PrepareTextureForExternalRead(
        *dispatchDesc_.motionVectors);

    if (dispatchDesc_.reactiveMask) {
        commands.PrepareTextureForExternalRead(
            *dispatchDesc_.reactiveMask);
    }

    commands.PrepareTextureForExternalWrite(
        *dispatchDesc_.output);

    auto dispatch =
        reinterpret_cast<
            PfnFfxDispatch>(
                dispatchFn_);

    if (!dispatch) {
        status_.detail =
            "ffxDispatch entry point is unavailable.";

        Core::Log(
            Core::LogLevel::Error,
            status_.detail);

        return false;
    }

    ffxDispatchDescUpscale
        nativeDesc{};

    nativeDesc.header.type =
        FFX_API_DISPATCH_DESC_TYPE_UPSCALE;

    nativeDesc.commandList =
        commands.NativeCommandListHandle();

    nativeDesc.color =
        ffxApiGetResourceDX12(
            static_cast<ID3D12Resource*>(
                dispatchDesc_.color
                    ->NativeResourceHandle()),
            FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    nativeDesc.depth =
        ffxApiGetResourceDX12(
            static_cast<ID3D12Resource*>(
                dispatchDesc_.depth
                    ->NativeResourceHandle()),
            FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ,
            FFX_API_RESOURCE_USAGE_DEPTHTARGET);

    nativeDesc.motionVectors =
        ffxApiGetResourceDX12(
            static_cast<ID3D12Resource*>(
                dispatchDesc_.motionVectors
                    ->NativeResourceHandle()),
            FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    nativeDesc.exposure =
        ffxApiGetResourceDX12(
            nullptr,
            FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    nativeDesc.output =
        ffxApiGetResourceDX12(
            static_cast<ID3D12Resource*>(
                dispatchDesc_.output
                    ->NativeResourceHandle()),
            FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);

    if (dispatchDesc_.reactiveMask) {
        nativeDesc.reactive =
            ffxApiGetResourceDX12(
                static_cast<ID3D12Resource*>(
                    dispatchDesc_.reactiveMask
                        ->NativeResourceHandle()),
                FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    }

    nativeDesc.transparencyAndComposition =
        ffxApiGetResourceDX12(
            nullptr,
            FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    nativeDesc.jitterOffset.x =
        -dispatchDesc_.jitterOffsetX;

    nativeDesc.jitterOffset.y =
        -dispatchDesc_.jitterOffsetY;

    nativeDesc.motionVectorScale.x =
        static_cast<float>(
            dispatchDesc_.dimensions.renderWidth);

    nativeDesc.motionVectorScale.y =
        static_cast<float>(
            dispatchDesc_.dimensions.renderHeight);

    nativeDesc.reset =
        dispatchDesc_.reset;

    nativeDesc.enableSharpening =
        dispatchDesc_.enableSharpening;

    nativeDesc.sharpness =
        dispatchDesc_.sharpness;

    nativeDesc.frameTimeDelta =
        std::max(
            dispatchDesc_.frameTimeDeltaMs,
            1.0f);

    nativeDesc.preExposure =
        dispatchDesc_.preExposure;

    nativeDesc.renderSize.width =
        dispatchDesc_.dimensions.renderWidth;

    nativeDesc.renderSize.height =
        dispatchDesc_.dimensions.renderHeight;

    nativeDesc.upscaleSize.width =
        dispatchDesc_.dimensions.displayWidth;

    nativeDesc.upscaleSize.height =
        dispatchDesc_.dimensions.displayHeight;

    nativeDesc.cameraFovAngleVertical =
        dispatchDesc_.cameraFovYRadians;

    nativeDesc.cameraFar =
        dispatchDesc_.cameraFar;

    nativeDesc.cameraNear =
        dispatchDesc_.cameraNear;

    nativeDesc.viewSpaceToMetersFactor =
        dispatchDesc_.viewSpaceToMetersFactor;

    nativeDesc.flags =
        0;

    const ffxReturnCode_t result =
        dispatch(
            reinterpret_cast<
                ffxContext*>(
                    &context_),
            &nativeDesc.header);

    commands.RestoreBackendBindings();

    status_.lastDispatchSucceeded =
        result ==
        FFX_API_RETURN_OK;

    status_.detail =
        status_.lastDispatchSucceeded
            ? "AMD FSR upscale dispatch completed."
            : (
                std::string(
                    "ffxDispatch failed: ") +
                FsrReturnCodeName(result) +
                " (" +
                std::to_string(result) +
                ")."
              );

    if (!status_.lastDispatchSucceeded) {
        Core::Log(
            Core::LogLevel::Error,
            status_.detail);
    }

    return
        status_.lastDispatchSucceeded;
#else
    (void)commands;

    status_.detail =
        "FSR live dispatch requires HAMUN_WITH_FSR_SDK on Windows DX12.";

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

    dispatchDesc_ = {};
    dispatchConfigured_ = false;
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
