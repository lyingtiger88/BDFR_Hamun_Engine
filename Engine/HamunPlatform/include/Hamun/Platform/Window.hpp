#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Hamun::Platform {

enum class Key : std::uint8_t {
    W,
    A,
    S,
    D,
    Q,
    E,
    LeftShift,
    Escape
};

enum class MouseButton : std::uint8_t {
    Left,
    Right,
    Middle
};

struct MouseDelta {
    float x = 0.0f;
    float y = 0.0f;
};

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

    [[nodiscard]] virtual bool IsKeyDown(
        Key key) const noexcept = 0;

    [[nodiscard]] virtual bool IsMouseButtonDown(
        MouseButton button) const noexcept = 0;

    virtual MouseDelta ConsumeMouseDelta() noexcept = 0;
    virtual bool PumpEvents() = 0;
};

std::unique_ptr<IWindow> CreateNativeWindow(
    const WindowDesc& desc);

} // namespace Hamun::Platform
