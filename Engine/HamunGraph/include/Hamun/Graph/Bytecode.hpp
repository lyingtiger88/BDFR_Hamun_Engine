#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
namespace Hamun::Graph {
using Value=std::variant<std::monostate,bool,std::int64_t,double,std::string>;
enum class OpCode:std::uint8_t { PushConstant,Add,Subtract,Multiply,Divide,Less,JumpIfFalse,Jump,Return };
struct Instruction { OpCode op=OpCode::Return; std::uint32_t operand=0; };
struct Program { std::vector<Value> constants; std::vector<Instruction> code; };
}
