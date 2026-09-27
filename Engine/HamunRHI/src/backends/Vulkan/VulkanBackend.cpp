#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>

namespace Hamun::RHI {

class VulkanBackend final : public IBackend {
public:
    VulkanBackend()
    {
        caps_.compute = true;
        caps_.asyncCompute = true;
        caps_.indirectDraw = true;
        caps_.bindless = true;
    }

    std::string_view Name() const noexcept override { return "Vulkan"; }
    BackendType Type() const noexcept override { return BackendType::Vulkan; }
    const Capabilities& Caps() const noexcept override { return caps_; }

    bool Initialize(const BackendCreateInfo&) override
    {
#if defined(HAMUN_ENABLE_VULKAN)
        Core::Log(Core::LogLevel::Info,
            "Vulkan backend foundation selected; native device implementation is pending.");
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

std::unique_ptr<IBackend> CreateVulkanBackend()
{
    return std::make_unique<VulkanBackend>();
}

} // namespace Hamun::RHI
