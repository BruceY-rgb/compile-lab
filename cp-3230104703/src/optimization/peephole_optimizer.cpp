#include "optimization/peephole_optimizer.hpp"

#include <memory>

void PeepholeOptimizer::optimize(Module& mod) {
  for (auto& func : mod.functions) {
    optimize(func);
  }
}

void PeepholeOptimizer::optimize(FunctionPtr& func) {
  for (auto& block : func->blocks) {
    ASM::Code new_code;
    for (auto& inst : block->asm_code) {
      if (auto mv = std::dynamic_pointer_cast<ASM::Mv>(inst)) {
        if (mv->rd == mv->rs) {
          continue;
        }
      }
      new_code.push_back(inst);
    }
    block->asm_code = new_code;
  }

  for (size_t i = 0; i + 1 < func->blocks.size(); ++i) {
    auto& code = func->blocks[i]->asm_code;
    if (code.empty()) {
      continue;
    }

    auto jump = std::dynamic_pointer_cast<ASM::Jump>(code.back());
    if (!jump) {
      continue;
    }

    auto& next_code = func->blocks[i + 1]->asm_code;
    if (next_code.empty()) {
      continue;
    }

    auto label = std::dynamic_pointer_cast<ASM::Label>(next_code.front());
    if (label && label->label == jump->label) {
      code.pop_back();
    }
  }
}
