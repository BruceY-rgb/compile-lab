#ifndef ANALYSIS_CONTROL_FLOW_HPP
#define ANALYSIS_CONTROL_FLOW_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "codegen/asm.hpp"
#include "ir/ir.hpp"

class BasicBlock;
using BasicBlockPtr = std::shared_ptr<BasicBlock>;
using BasicBlockWeakPtr = std::weak_ptr<BasicBlock>;
class BasicBlock {
 public:
  std::string label;
  IR::Code ir_code;
  ASM::Code asm_code;
  std::vector<BasicBlockWeakPtr> successors;
  std::vector<BasicBlockWeakPtr> predecessors;

  BasicBlock(std::string label, IR::Code ir_code = {})
      : label(label), ir_code(ir_code) {}

  static BasicBlockPtr create(std::string label, IR::Code ir_code = {}) {
    return std::make_shared<BasicBlock>(label, ir_code);
  }
};

class Function;
using FunctionPtr = std::shared_ptr<Function>;
class Function {
 public:
  std::string name;
  std::vector<BasicBlockPtr> blocks;

  /// @brief stack size for temporary variables, in bytes (near sp)
  int temp_stack_size = 0;

  /// @brief stack size for large allocations, in bytes (near fp)
  int high_temp_size = 0;

  /// @brief stack size for saved registers, in bytes
  int reg_stack_size = 0;

  /// @brief map virtual registers to physical registers
  ASM::RegMap reg_map;

  Function(std::string name, std::vector<BasicBlockPtr> blocks = {})
      : name(name), blocks(blocks) {}

  static FunctionPtr create(std::string name,
                            std::vector<BasicBlockPtr> blocks = {}) {
    return std::make_shared<Function>(name, blocks);
  }

  /// @brief allocate space for temporary variables
  /// @param size size of the variable, in bytes
  /// @return the offset from sp
  int alloc_temp(int size = 4);

  /// @brief allocate space for saved registers
  /// @param size size of the register, in bytes
  /// @return the offset from fp
  int alloc_reg(int size = 4);

  /// @brief allocate space for large objects (arrays) near fp
  /// @param size size in bytes
  /// @return the offset from fp (positive, to be subtracted)
  int alloc_temp_high(int size = 4);
};

class Module {
 public:
  std::vector<FunctionPtr> functions;
  std::vector<IR::GlobalPtr> globals;

  Module() = default;
  Module(std::vector<FunctionPtr> functions) : functions(functions) {}

  IR::Code get_ir() const;
};

#endif  // ANALYSIS_CONTROL_FLOW_HPP