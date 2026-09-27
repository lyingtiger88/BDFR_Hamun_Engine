#pragma once
#include <string_view>
namespace Hamun::Core {
enum class LogLevel { Trace, Info, Warning, Error };
void Log(LogLevel level, std::string_view message);
}
