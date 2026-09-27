#include <Hamun/Core/Log.hpp>
#include <Hamun/Platform/Window.hpp>

namespace Hamun::Platform {

std::unique_ptr<IWindow> CreateNativeWindow(const WindowDesc&)
{
    Core::Log(Core::LogLevel::Warning,
        "Native window creation is not implemented for this platform yet.");
    return {};
}

} // namespace Hamun::Platform
