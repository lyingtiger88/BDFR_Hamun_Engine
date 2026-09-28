#include <Hamun/Core/Log.hpp>
#include <Hamun/Platform/Window.hpp>

namespace Hamun::Platform {

namespace {

class StubWindow final : public IWindow {
public:
    void* NativeHandle() const noexcept override { return nullptr; }
    std::uint32_t Width() const noexcept override { return 0; }
    std::uint32_t Height() const noexcept override { return 0; }

    bool IsKeyDown(Key) const noexcept override { return false; }

    bool IsMouseButtonDown(
        MouseButton) const noexcept override
    {
        return false;
    }

    MouseDelta ConsumeMouseDelta() noexcept override
    {
        return {};
    }

    bool PumpEvents() override { return false; }
};

} // namespace

std::unique_ptr<IWindow> CreateNativeWindow(
    const WindowDesc&)
{
    Core::Log(
        Core::LogLevel::Warning,
        "Native window creation is not implemented for this platform yet.");
    return {};
}

} // namespace Hamun::Platform
