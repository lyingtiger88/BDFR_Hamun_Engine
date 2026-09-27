#pragma once
#include <cstdint>
#include <memory>
#include <string_view>
namespace Hamun::RHI {
enum class BackendType : std::uint8_t { D3D12, Vulkan, OpenGLES };
struct Capabilities {
  bool compute=false, asyncCompute=false, indirectDraw=false;
  bool bindless=false, meshShaders=false, rayTracing=false;
};
class IBackend {
public:
  virtual ~IBackend()=default;
  virtual std::string_view Name() const noexcept=0;
  virtual BackendType Type() const noexcept=0;
  virtual const Capabilities& Caps() const noexcept=0;
  virtual bool Initialize()=0;
  virtual void Shutdown()=0;
};
std::unique_ptr<IBackend> CreateBackend(BackendType type);
}
