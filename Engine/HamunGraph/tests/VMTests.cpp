#include <Hamun/Graph/VM.hpp>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
using namespace Hamun::Graph;
static bool fails(const Program& p, const std::string& expected) {
  try { (void)VM{}.Execute(p); }
  catch(const std::runtime_error& e) { return std::string(e.what()).find(expected)!=std::string::npos; }
  return false;
}
int main() {
  {
    Program p{{2.0,3.0},{{OpCode::PushConstant,0},{OpCode::PushConstant,1},{OpCode::Multiply,0},{OpCode::Return,0}}};
    assert(std::abs(std::get<double>(VM{}.Execute(p))-6.0)<1e-9);
  }
  {
    Program p{{true,42.0},{{OpCode::PushConstant,0},{OpCode::JumpIfFalse,4},{OpCode::PushConstant,1},{OpCode::Return,0},{OpCode::Return,0}}};
    assert(std::get<double>(VM{}.Execute(p))==42.0);
  }
  assert(fails(Program{{},{{OpCode::Jump,999}}},"invalid jump target"));
  assert(fails(Program{{},{{OpCode::Jump,0}}},"instruction budget exceeded"));
  assert(fails(Program{{1.0},{{OpCode::PushConstant,0},{OpCode::Jump,0}}},"stack limit exceeded"));
  assert(fails(Program{{},{{OpCode::Add,0}}},"stack underflow"));
  assert(fails(Program{{1.0,0.0},{{OpCode::PushConstant,0},{OpCode::PushConstant,1},{OpCode::Divide,0}}},"division by zero"));
  assert(fails(Program{{},{{static_cast<OpCode>(255),0}}},"unknown opcode"));
  return 0;
}
