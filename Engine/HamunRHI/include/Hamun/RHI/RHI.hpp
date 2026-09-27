#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace Hamun::RHI {

enum class BackendType : std::uint8_t {
    D3D12,
    Vulkan,
    OpenGLES
};

struct Capabilities {
    bool compute = false;
    bool asyncCompute = false;
    bool indirectDraw = false;
    bool bindless = false;
    bool meshShaders = false;
    bool rayTracing = false;
};

struct BackendCreateInfo {
    void* nativeWindowHandle = nullptr;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool enableValidation = true;
};

class IBackend {
public:
    virtual ~IBackend() = default;

    [[nodiscard]] virtual std::string_view Name() const noexcept = 0;
    [[nodiscard]] virtual BackendType Type() const noexcept = 0;
    [[nodiscard]] virtual const Capabilities& Caps() const noexcept = 0;

    virtual bool Initialize(const BackendCreateInfo& createInfo) = 0;
    virtual bool RenderFrame() = 0;
    virtual void Shutdown() = 0;
};

std::unique_ptr<IBackend> CreateBackend(BackendType type);

} // namespace Hamun::RHI
