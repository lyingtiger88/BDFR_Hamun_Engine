#pragma once

#include <Hamun/Renderer/IPostSceneProcessor.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace Hamun::Upscale {

enum class UpscaleQualityMode {
    NativeAA,
    Quality,
    Balanced,
    Performance,
    UltraPerformance
};

struct UpscaleDimensions {
    std::uint32_t renderWidth = 1;
    std::uint32_t renderHeight = 1;
    std::uint32_t displayWidth = 1;
    std::uint32_t displayHeight = 1;
};

struct FsrContextDesc {
    UpscaleDimensions dimensions{};

    bool highDynamicRange = true;
    bool autoExposure = true;
    bool depthInverted = false;
    bool depthInfinite = false;
    bool dynamicResolution = false;
    bool enableDebugChecking = true;
};

struct FsrDispatchDesc {
    RHI::ITexture* color = nullptr;
    RHI::ITexture* depth = nullptr;
    RHI::ITexture* motionVectors = nullptr;
    RHI::ITexture* reactiveMask = nullptr;
    RHI::ITexture* output = nullptr;

    UpscaleDimensions dimensions{};

    float jitterOffsetX = 0.0f;
    float jitterOffsetY = 0.0f;

    float frameTimeDeltaMs = 16.6667f;
    float preExposure = 1.0f;

    float cameraNear = 0.1f;
    float cameraFar = 1000.0f;
    float cameraFovYRadians = 1.0471975512f;
    float viewSpaceToMetersFactor = 1.0f;

    bool reset = false;
    bool enableSharpening = true;
    float sharpness = 0.2f;
};

struct FsrRuntimeStatus {
    bool backendCompatible = false;
    bool nativeInteropAvailable = false;
    bool loaderDllLoaded = false;
    bool upscalerDllLoaded = false;
    bool apiFunctionsResolved = false;
    bool sdkHeadersEnabled = false;
    bool contextCreated = false;
    bool lastDispatchSucceeded = false;

    std::uint64_t availableProviderCount = 0;
    std::uint64_t selectedProviderId = 0;
    std::string selectedProviderName;

    std::string detail;
};

class FsrRuntime final :
    public Renderer::IPostSceneProcessor {
public:
    FsrRuntime();
    ~FsrRuntime() override;

    FsrRuntime(const FsrRuntime&) = delete;
    FsrRuntime& operator=(const FsrRuntime&) = delete;

    bool Probe(
        RHI::IBackend& backend,
        const std::filesystem::path& runtimeDirectory);

    bool CreateContext(
        RHI::IBackend& backend,
        const FsrContextDesc& desc);

    void ConfigureDispatch(
        const FsrDispatchDesc& desc) noexcept;

    bool Execute(
        RHI::ICommandList& commands) override;

    void DestroyContext() noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] const FsrRuntimeStatus& Status() const noexcept
    {
        return status_;
    }

    [[nodiscard]] bool ReadyForDispatch() const noexcept
    {
        return
            status_.contextCreated &&
            status_.apiFunctionsResolved;
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void* loaderModule_ = nullptr;
    void* upscalerModule_ = nullptr;

    void* createContextFn_ = nullptr;
    void* destroyContextFn_ = nullptr;
    void* dispatchFn_ = nullptr;
    void* queryFn_ = nullptr;
    void* configureFn_ = nullptr;

    void* context_ = nullptr;

    FsrDispatchDesc dispatchDesc_{};
    bool dispatchConfigured_ = false;

    FsrRuntimeStatus status_{};
};

float UpscaleRatio(
    UpscaleQualityMode mode) noexcept;

UpscaleDimensions BuildUpscaleDimensions(
    std::uint32_t displayWidth,
    std::uint32_t displayHeight,
    UpscaleQualityMode mode) noexcept;

} // namespace Hamun::Upscale
