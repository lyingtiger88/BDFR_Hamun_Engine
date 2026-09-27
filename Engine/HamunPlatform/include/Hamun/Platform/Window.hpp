#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Hamun::Platform {

struct WindowDesc {
    std::string title = "BDFR Hamun Engine";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
};

class IWindow {
public:
    virtual ~IWindow() = default;

    [[nodiscard]] virtual void* NativeHandle() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t Width() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t Height() const noexcept = 0;

    // Returns false after the application receives a quit request.
    virtual bool PumpEvents() = 0;
};

std::unique_ptr<IWindow> CreateWindow(const WindowDesc& desc);

} // namespace Hamun::Platform
