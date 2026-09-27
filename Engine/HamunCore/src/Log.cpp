#include <Hamun/Core/Log.hpp>
#include <iostream>
#include <mutex>
namespace Hamun::Core {
void Log(LogLevel level, std::string_view message) {
  static std::mutex mutex;
  std::scoped_lock lock(mutex);
  const char* tag="INFO";
  switch(level) {
    case LogLevel::Trace: tag="TRACE"; break;
    case LogLevel::Info: tag="INFO"; break;
    case LogLevel::Warning: tag="WARN"; break;
    case LogLevel::Error: tag="ERROR"; break;
  }
  std::clog << "[Hamun][" << tag << "] " << message << '\n';
}
}
