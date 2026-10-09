#include <Hamun/Graph/VM.hpp>
#include <stdexcept>
#include <vector>
namespace Hamun::Graph {
double VM::AsNumber(const Value& v) {
  if(auto p=std::get_if<double>(&v)) return *p;
  if(auto p=std::get_if<std::int64_t>(&v)) return double(*p);
  throw std::runtime_error("HamunGraph: value is not numeric");
}
bool VM::AsBool(const Value& v) {
  if(auto p=std::get_if<bool>(&v)) return *p;
  throw std::runtime_error("HamunGraph: value is not boolean");
}
Value VM::Execute(const Program& p) {
  // A graph may contain loops, but a broken graph must not hang the editor.
  constexpr std::size_t maxInstructions = 1000000;
  constexpr std::size_t maxStackValues = 65536;
  std::vector<Value> stack; std::size_t ip=0, executed=0;
  auto pop=[&](){ if(stack.empty()) throw std::runtime_error("HamunGraph: stack underflow"); auto v=std::move(stack.back()); stack.pop_back(); return v; };
  auto jump=[&](std::uint32_t target) {
    if(target>=p.code.size()) throw std::runtime_error("HamunGraph: invalid jump target");
    ip=target;
  };
  while(ip<p.code.size()) {
    if(++executed>maxInstructions) throw std::runtime_error("HamunGraph: instruction budget exceeded");
    auto ins=p.code[ip++];
    switch(ins.op) {
      case OpCode::PushConstant:
        if(ins.operand>=p.constants.size()) throw std::runtime_error("HamunGraph: bad constant index");
        if(stack.size()>=maxStackValues) throw std::runtime_error("HamunGraph: stack limit exceeded");
        stack.push_back(p.constants[ins.operand]); break;
      case OpCode::Add: case OpCode::Subtract: case OpCode::Multiply: case OpCode::Divide: {
        double rhs=AsNumber(pop()), lhs=AsNumber(pop()), result=0;
        if(ins.op==OpCode::Add) result=lhs+rhs;
        else if(ins.op==OpCode::Subtract) result=lhs-rhs;
        else if(ins.op==OpCode::Multiply) result=lhs*rhs;
        else { if(rhs==0) throw std::runtime_error("HamunGraph: division by zero"); result=lhs/rhs; }
        stack.emplace_back(result); break;
      }
      case OpCode::Less: { double rhs=AsNumber(pop()),lhs=AsNumber(pop()); stack.emplace_back(lhs<rhs); break; }
      case OpCode::JumpIfFalse: if(!AsBool(pop())) jump(ins.operand); break;
      case OpCode::Jump: jump(ins.operand); break;
      case OpCode::Return: return stack.empty()?Value{}:pop();
      default: throw std::runtime_error("HamunGraph: unknown opcode");
    }
    if(stack.size()>maxStackValues) throw std::runtime_error("HamunGraph: stack limit exceeded");
  }
  return stack.empty()?Value{}:stack.back();
}
}
