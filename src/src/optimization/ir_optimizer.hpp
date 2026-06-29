#ifndef OPTIMIZATION_IR_OPTIMIZER_HPP
#define OPTIMIZATION_IR_OPTIMIZER_HPP

#include "ir/ir.hpp"

class IROptimizer {
 public:
  IR::Code optimize(const IR::Code& code);

 private:
  IR::Code optimize_function(const IR::Code& code);
};

#endif  // OPTIMIZATION_IR_OPTIMIZER_HPP
