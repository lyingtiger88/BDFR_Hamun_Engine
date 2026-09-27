#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>
namespace Hamun::RHI {
class D3D12Backend final : public IBackend {
  Capabilities caps_{true,true,true,true,false,false};
public:
  std::string_view Name() const noexcept override { return "Direct3D 12"; }
  BackendType Type() const noexcept override { return BackendType::D3D12; }
  const Capabilities& Caps() const noexcept override { return caps_; }
  bool Initialize() override {
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    Core::Log(Core::LogLevel::Info,"D3D12 RHI foundation selected; device/queues/swapchain are the next renderer milestone.");
    return true;
#else
    Core::Log(Core::LogLevel::Warning,"D3D12 backend unavailable in this build.");
    return false;
#endif
  }
  void Shutdown() override {}
};
std::unique_ptr<IBackend> CreateD3D12Backend(){ return std::make_unique<D3D12Backend>(); }
}
