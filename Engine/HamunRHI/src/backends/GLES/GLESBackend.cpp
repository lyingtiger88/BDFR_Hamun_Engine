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
            "OpenGL ES compatibility RHI selected; native resource implementation is pending.");
        initialized_ = true;
        return true;
#else
        return false;
#endif
    }

    void Shutdown() override { initialized_ = false; }

    std::unique_ptr<IBuffer> CreateBuffer(const BufferDesc&) override { return {}; }
    std::unique_ptr<IShader> CreateShader(const ShaderDesc&) override { return {}; }
    std::unique_ptr<IPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc&) override { return {}; }

    ICommandList* BeginFrame() override { return nullptr; }
    bool SubmitFrame() override { return false; }
    ISwapChain* SwapChain() noexcept override { return nullptr; }
    IFence* FrameFence() noexcept override { return nullptr; }

private:
    Capabilities caps_{};
    bool initialized_ = false;
};

std::unique_ptr<IBackend> CreateGLESBackend()
{
    return std::make_unique<GLESBackend>();
}

} // namespace Hamun::RHI
