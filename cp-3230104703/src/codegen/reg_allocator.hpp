#ifndef CODEGEN_REG_ALLOCATOR_HPP
#define CODEGEN_REG_ALLOCATOR_HPP

class ASMEmitter;

#include "analysis/control_flow.hpp"

class RegAllocator {
 public:
  void allocate(Module& mod);
  void allocate(FunctionPtr& func);

 private:
};

#endif  // CODEGEN_REG_ALLOCATOR_HPP