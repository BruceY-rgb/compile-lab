#include "control_flow.hpp"

#include "common.hpp"

int Function::alloc_temp(int size) {
  // 为临时变量分配空间，临时变量通常分配在靠近 sp 的位置
  // 返回值是相对于 sp 的偏移

  int offset = temp_stack_size;
  temp_stack_size += size;
  return offset;
}

int Function::alloc_temp_high(int size) {
  // 从 fp 向下分配（用于大数组 DEC），避免把虚拟寄存器 sp 偏移挤到高位
  high_temp_size += size;
  return high_temp_size;  // 返回值是从 fp 向下的正偏移
}

int Function::alloc_reg(int size) {
  // 为保存寄存器分配空间，保存寄存器通常分配在靠近 fp 的位置
  // 返回值是相对于 fp 的偏移

  int offset = reg_stack_size;
  reg_stack_size += size;
  return offset;
}

IR::Code Module::get_ir() const {
  IR::Code code;
  for (const auto& func : functions) {
    for (const auto& block : func->blocks) {
      std::copy(block->ir_code.begin(), block->ir_code.end(),
                std::back_inserter(code));
    }
  }
  return code;
}