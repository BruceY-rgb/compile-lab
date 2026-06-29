#ifndef OPTIMIZATION_PEEPHOLE_OPTIMIZER_HPP
#define OPTIMIZATION_PEEPHOLE_OPTIMIZER_HPP

#include "analysis/control_flow.hpp"

class PeepholeOptimizer {
 public:
  void optimize(Module& mod);

 private:
  void optimize(FunctionPtr& func);
};

#endif  // OPTIMIZATION_PEEPHOLE_OPTIMIZER_HPP
