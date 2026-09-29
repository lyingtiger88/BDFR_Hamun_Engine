#pragma once

#include <Hamun/RHI/RHI.hpp>

#include <cstdint>
#include <filesystem>
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

struct FsrRuntimeStatus {
    bool backendCompatible = false;
    bool nativeInteropAvailable = false;
    bool loaderDllLoaded = false;
    bool upscalerDllLoaded = false;
    bool apiFunctionsResolved = false;
    bool sdkHeadersEnabled = false;
    bool contextCreated = false;

    std::string detail;
};

class FsrRuntime {
public:
    FsrRuntime() = default;
    ~FsrRuntime();

    FsrRuntime(const FsrRuntime&) = delete;
    FsrRuntime& operator=(const FsrRuntime&) = delete;

    bool Probe(
        RHI::IBackend& backend,
        const std::filesystem::path& runtimeDirectory);

    bool CreateContext(
        RHI::IBackend& backend,
        const FsrContextDesc& desc);

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
    void* loaderModule_ = nullptr;
    void* upscalerModule_ = nullptr;

    void* createContextFn_ = nullptr;
    void* destroyContextFn_ = nullptr;
    void* dispatchFn_ = nullptr;
    void* queryFn_ = nullptr;
    void* configureFn_ = nullptr;

    void* context_ = nullptr;

    FsrRuntimeStatus status_{};
};

float UpscaleRatio(
    UpscaleQualityMode mode) noexcept;

UpscaleDimensions BuildUpscaleDimensions(
    std::uint32_t displayWidth,
    std::uint32_t displayHeight,
    UpscaleQualityMode mode) noexcept;

} // namespace Hamun::Upscale
