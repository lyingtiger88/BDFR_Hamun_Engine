#include <Hamun/Graph/VM.hpp>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace Hamun::Graph;
static void check(bool value,const char* message) {
  if(!value) throw std::runtime_error(message);
}
static bool fails(const Program& p,const std::string& expected) {
  try { (void)VM{}.Execute(p); }
  catch(const std::runtime_error& e) { return std::string(e.what()).find(expected)!=std::string::npos; }
  return false;
}
int main() {
  try {
    Program multiply{{2.0,3.0},{{OpCode::PushConstant,0},{OpCode::PushConstant,1},{OpCode::Multiply,0},{OpCode::Return,0}}};
    check(std::abs(std::get<double>(VM{}.Execute(multiply))-6.0)<1e-9,"multiply");
    Program branch{{true,42.0},{{OpCode::PushConstant,0},{OpCode::JumpIfFalse,4},{OpCode::PushConstant,1},{OpCode::Return,0},{OpCode::Return,0}}};
    check(std::get<double>(VM{}.Execute(branch))==42.0,"branch");
    check(fails(Program{{},{{OpCode::Jump,999}}},"invalid jump target"),"invalid jump");
    check(fails(Program{{},{{OpCode::Jump,0}}},"instruction budget exceeded"),"infinite loop");
    check(fails(Program{{1.0},{{OpCode::PushConstant,0},{OpCode::Jump,0}}},"stack limit exceeded"),"stack overflow");
    check(fails(Program{{},{{OpCode::Add,0}}},"stack underflow"),"stack underflow");
    check(fails(Program{{1.0,0.0},{{OpCode::PushConstant,0},{OpCode::PushConstant,1},{OpCode::Divide,0}}},"division by zero"),"divide by zero");
    check(fails(Program{{},{{static_cast<OpCode>(255),0}}},"unknown opcode"),"unknown opcode");
    std::cout<<"HamunGraph VM tests passed\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr<<"HamunGraph VM test failure: "<<e.what()<<"\n";
    return 1;
  }
}
