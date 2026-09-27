#pragma once
#include <Hamun/Graph/Bytecode.hpp>
namespace Hamun::Graph {
class VM {
public:
  Value Execute(const Program& program);
private:
  static double AsNumber(const Value& value);
  static bool AsBool(const Value& value);
};
}
