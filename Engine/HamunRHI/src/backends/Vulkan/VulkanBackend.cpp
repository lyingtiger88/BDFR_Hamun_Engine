#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>
namespace Hamun::RHI {
class VulkanBackend final : public IBackend {
  Capabilities caps_{true,true,true,true,false,false};
public:
  std::string_view Name() const noexcept override { return "Vulkan"; }
  BackendType Type() const noexcept override { return BackendType::Vulkan; }
  const Capabilities& Caps() const noexcept override { return caps_; }
  bool Initialize() override {
#if defined(HAMUN_ENABLE_VULKAN)
    Core::Log(Core::LogLevel::Info,"Vulkan RHI foundation selected.");
    return true;
#else
    Core::Log(Core::LogLevel::Warning,"Vulkan backend disabled.");
    return false;
#endif
  }
  void Shutdown() override {}
};
std::unique_ptr<IBackend> CreateVulkanBackend(){ return std::make_unique<VulkanBackend>(); }
}
