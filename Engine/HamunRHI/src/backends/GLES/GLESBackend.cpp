#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>
namespace Hamun::RHI {
class GLESBackend final : public IBackend {
  Capabilities caps_{true,false,false,false,false,false};
public:
  std::string_view Name() const noexcept override { return "OpenGL ES"; }
  BackendType Type() const noexcept override { return BackendType::OpenGLES; }
  const Capabilities& Caps() const noexcept override { return caps_; }
  bool Initialize() override {
#if defined(HAMUN_ENABLE_GLES)
    Core::Log(Core::LogLevel::Info,"OpenGL ES compatibility RHI foundation selected.");
    return true;
#else
    Core::Log(Core::LogLevel::Warning,"OpenGL ES backend disabled.");
    return false;
#endif
  }
  void Shutdown() override {}
};
std::unique_ptr<IBackend> CreateGLESBackend(){ return std::make_unique<GLESBackend>(); }
}
