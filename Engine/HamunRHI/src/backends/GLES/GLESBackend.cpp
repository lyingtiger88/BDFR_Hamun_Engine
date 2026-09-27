#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>

namespace Hamun::RHI {

class GLESBackend final : public IBackend {
public:
    GLESBackend()
    {
        caps_.compute = true;
    }

    std::string_view Name() const noexcept override { return "OpenGL ES"; }
    BackendType Type() const noexcept override { return BackendType::OpenGLES; }
    const Capabilities& Caps() const noexcept override { return caps_; }

    bool Initialize(const BackendCreateInfo&) override
    {
#if defined(HAMUN_ENABLE_GLES)
        Core::Log(Core::LogLevel::Info,
            "OpenGL ES compatibility backend selected; native context implementation is pending.");
        initialized_ = true;
        return true;
#else
        return false;
#endif
    }

    bool RenderFrame() override { return initialized_; }
    void Shutdown() override { initialized_ = false; }

private:
    Capabilities caps_{};
    bool initialized_ = false;
};

std::unique_ptr<IBackend> CreateGLESBackend()
{
    return std::make_unique<GLESBackend>();
}

} // namespace Hamun::RHI
