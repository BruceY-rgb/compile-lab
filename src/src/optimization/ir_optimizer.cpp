#include "optimization/ir_optimizer.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

bool parse_imm(const std::string& value, int& out) {
  if (value.empty() || value[0] != '#') {
    return false;
  }
  out = std::stoi(value.substr(1));
  return true;
}

std::optional<int> lookup_const(
    const std::string& value,
    const std::unordered_map<std::string, int>& constants) {
  int imm = 0;
  if (parse_imm(value, imm)) {
    return imm;
  }
  auto it = constants.find(value);
  if (it == constants.end()) {
    return std::nullopt;
  }
  return it->second;
}

int eval_binary(int lhs, BinaryOp op, int rhs) {
  switch (op) {
    case BinaryOp::Add:
      return lhs + rhs;
    case BinaryOp::Sub:
      return lhs - rhs;
    case BinaryOp::Mul:
      return lhs * rhs;
    case BinaryOp::Div:
      return rhs == 0 ? 0 : lhs / rhs;
    case BinaryOp::Mod:
      return rhs == 0 ? 0 : lhs % rhs;
    case BinaryOp::Lt:
      return lhs < rhs;
    case BinaryOp::Gt:
      return lhs > rhs;
    case BinaryOp::Le:
      return lhs <= rhs;
    case BinaryOp::Ge:
      return lhs >= rhs;
    case BinaryOp::Eq:
      return lhs == rhs;
    case BinaryOp::Neq:
      return lhs != rhs;
    case BinaryOp::And:
      return (lhs != 0) && (rhs != 0);
    case BinaryOp::Or:
      return (lhs != 0) || (rhs != 0);
  }
  return 0;
}

int eval_unary(UnaryOp op, int value) {
  switch (op) {
    case UnaryOp::Pos:
      return value;
    case UnaryOp::Neg:
      return -value;
    case UnaryOp::Lnot:
      return value == 0;
  }
  return value;
}

bool eval_cond(int lhs, BinaryOp op, int rhs) {
  return eval_binary(lhs, op, rhs) != 0;
}

void kill_def(const IR::NodePtr& node,
              std::unordered_map<std::string, int>& constants) {
  auto erase = [&](const std::string& name) { constants.erase(name); };

  if (auto p = std::dynamic_pointer_cast<IR::LoadImm>(node)) erase(p->x);
  else if (auto p = std::dynamic_pointer_cast<IR::Assign>(node)) erase(p->x);
  else if (auto p = std::dynamic_pointer_cast<IR::Binary>(node)) erase(p->x);
  else if (auto p = std::dynamic_pointer_cast<IR::Unary>(node)) erase(p->x);
  else if (auto p = std::dynamic_pointer_cast<IR::Call>(node)) {
    if (!p->x.empty()) erase(p->x);
  } else if (auto p = std::dynamic_pointer_cast<IR::Param>(node)) erase(p->x);
  else if (auto p = std::dynamic_pointer_cast<IR::LoadAddr>(node)) erase(p->x);
  else if (auto p = std::dynamic_pointer_cast<IR::Load>(node)) erase(p->x);
  else if (auto p = std::dynamic_pointer_cast<IR::LoadOffset>(node)) erase(p->x);
  else if (auto p = std::dynamic_pointer_cast<IR::Dec>(node)) erase(p->x);
}

bool is_pure_def(const IR::NodePtr& node, std::string& def) {
  if (auto p = std::dynamic_pointer_cast<IR::LoadImm>(node)) {
    def = p->x;
    return true;
  }
  if (auto p = std::dynamic_pointer_cast<IR::Assign>(node)) {
    def = p->x;
    return true;
  }
  if (auto p = std::dynamic_pointer_cast<IR::Binary>(node)) {
    def = p->x;
    return true;
  }
  if (auto p = std::dynamic_pointer_cast<IR::Unary>(node)) {
    def = p->x;
    return true;
  }
  return false;
}

std::vector<std::string> uses_of(const IR::NodePtr& node) {
  auto add_if_var = [](std::vector<std::string>& uses, const std::string& name) {
    if (!name.empty() && name[0] != '#') {
      uses.push_back(name);
    }
  };

  std::vector<std::string> uses;
  if (auto p = std::dynamic_pointer_cast<IR::Assign>(node)) {
    add_if_var(uses, p->y);
  } else if (auto p = std::dynamic_pointer_cast<IR::Binary>(node)) {
    add_if_var(uses, p->y);
    add_if_var(uses, p->z);
  } else if (auto p = std::dynamic_pointer_cast<IR::Unary>(node)) {
    add_if_var(uses, p->y);
  } else if (auto p = std::dynamic_pointer_cast<IR::If>(node)) {
    add_if_var(uses, p->x);
    add_if_var(uses, p->y);
  } else if (auto p = std::dynamic_pointer_cast<IR::Call>(node)) {
    (void)p;
  } else if (auto p = std::dynamic_pointer_cast<IR::Arg>(node)) {
    add_if_var(uses, p->x);
  } else if (auto p = std::dynamic_pointer_cast<IR::Return>(node)) {
    add_if_var(uses, p->x);
  } else if (auto p = std::dynamic_pointer_cast<IR::Load>(node)) {
    add_if_var(uses, p->y);
  } else if (auto p = std::dynamic_pointer_cast<IR::Store>(node)) {
    add_if_var(uses, p->x);
    add_if_var(uses, p->y);
  } else if (auto p = std::dynamic_pointer_cast<IR::LoadOffset>(node)) {
    add_if_var(uses, p->y);
  } else if (auto p = std::dynamic_pointer_cast<IR::StoreOffset>(node)) {
    add_if_var(uses, p->x);
    add_if_var(uses, p->y);
  }
  return uses;
}

IR::Code remove_local_dead_defs(const IR::Code& code) {
  std::vector<IR::NodePtr> nodes(code.begin(), code.end());
  std::vector<bool> keep(nodes.size(), true);
  std::set<std::string> live;

  auto flush_liveness = [&]() { live.clear(); };

  for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; --i) {
    auto node = nodes[i];

    if (type_of<IR::Label>(node) || type_of<IR::Function>(node)) {
      flush_liveness();
      continue;
    }

    if (type_of<IR::Goto>(node) || type_of<IR::If>(node) ||
        type_of<IR::Call>(node) || type_of<IR::Store>(node) ||
        type_of<IR::StoreOffset>(node) || type_of<IR::Return>(node)) {
      for (auto& use : uses_of(node)) {
        live.insert(use);
      }
      continue;
    }

    std::string def;
    if (is_pure_def(node, def)) {
      for (auto& use : uses_of(node)) {
        live.insert(use);
      }
      if (!live.count(def)) {
        keep[i] = false;
      } else {
        live.erase(def);
      }
      continue;
    }

    for (auto& use : uses_of(node)) {
      live.insert(use);
    }
  }

  IR::Code result;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (keep[i]) {
      result.push_back(nodes[i]);
    }
  }
  return result;
}

IR::Code remove_unreachable_after_jumps(const IR::Code& code) {
  IR::Code result;
  bool unreachable = false;
  for (auto& node : code) {
    if (type_of<IR::Label>(node) || type_of<IR::Function>(node)) {
      unreachable = false;
    }
    if (!unreachable) {
      result.push_back(node);
    }
    if (type_of<IR::Goto>(node) || type_of<IR::Return>(node)) {
      unreachable = true;
    }
  }
  return result;
}

IR::Code remove_jump_to_next_label(const IR::Code& code) {
  std::vector<IR::NodePtr> nodes(code.begin(), code.end());
  std::vector<bool> keep(nodes.size(), true);
  for (size_t i = 0; i + 1 < nodes.size(); ++i) {
    auto go = std::dynamic_pointer_cast<IR::Goto>(nodes[i]);
    auto label = std::dynamic_pointer_cast<IR::Label>(nodes[i + 1]);
    if (go && label && go->label == label->label) {
      keep[i] = false;
    }
  }

  IR::Code result;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (keep[i]) result.push_back(nodes[i]);
  }
  return result;
}

IR::Code local_lle(const IR::Code& code) {
  std::unordered_map<std::string, std::string> load_cache;
  IR::Code result;

  for (auto& node : code) {
    if (type_of<IR::Label>(node) || type_of<IR::Function>(node) ||
        type_of<IR::Goto>(node) || type_of<IR::If>(node) ||
        type_of<IR::Return>(node)) {
      load_cache.clear();
      result.push_back(node);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::Load>(node)) {
      if (p->y.size() >= 5 && p->y.substr(p->y.size() - 5) == ".addr") {
        auto it = load_cache.find(p->y);
        if (it != load_cache.end()) {
          result.push_back(IR::Assign::create(p->x, it->second));
          continue;
        } else {
          load_cache[p->y] = p->x;
        }
      }
    }

    if (auto p = std::dynamic_pointer_cast<IR::Store>(node)) {
      if (p->x.size() >= 5 && p->x.substr(p->x.size() - 5) == ".addr") {
        load_cache[p->x] = p->y;
      }
    }

    result.push_back(node);
  }
  return result;
}

IR::Code copy_prop(const IR::Code& code) {
  std::unordered_map<std::string, std::string> aliases;
  IR::Code result;

  auto get_alias = [&](const std::string& var) -> std::string {
    if (var.empty() || var[0] == '#') return var;
    auto it = aliases.find(var);
    if (it != aliases.end()) {
      return it->second;
    }
    return var;
  };

  for (auto& node : code) {
    if (type_of<IR::Label>(node) || type_of<IR::Function>(node)) {
      aliases.clear();
      result.push_back(node);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::Assign>(node)) {
      p->y = get_alias(p->y);
      aliases[p->x] = p->y;
    } else if (auto p = std::dynamic_pointer_cast<IR::Binary>(node)) {
      p->y = get_alias(p->y);
      p->z = get_alias(p->z);
    } else if (auto p = std::dynamic_pointer_cast<IR::Unary>(node)) {
      p->y = get_alias(p->y);
    } else if (auto p = std::dynamic_pointer_cast<IR::If>(node)) {
      p->x = get_alias(p->x);
      p->y = get_alias(p->y);
    } else if (auto p = std::dynamic_pointer_cast<IR::Arg>(node)) {
      p->x = get_alias(p->x);
    } else if (auto p = std::dynamic_pointer_cast<IR::Return>(node)) {
      p->x = get_alias(p->x);
    } else if (auto p = std::dynamic_pointer_cast<IR::Load>(node)) {
      p->y = get_alias(p->y);
    } else if (auto p = std::dynamic_pointer_cast<IR::Store>(node)) {
      p->x = get_alias(p->x);
      p->y = get_alias(p->y);
    } else if (auto p = std::dynamic_pointer_cast<IR::LoadOffset>(node)) {
      p->y = get_alias(p->y);
    } else if (auto p = std::dynamic_pointer_cast<IR::StoreOffset>(node)) {
      p->x = get_alias(p->x);
      p->y = get_alias(p->y);
    }

    result.push_back(node);
  }
  return result;
}

struct Expr {
  std::string op;
  std::string arg1;
  std::string arg2;
  bool operator<(const Expr& o) const {
    if (op != o.op) return op < o.op;
    if (arg1 != o.arg1) return arg1 < o.arg1;
    return arg2 < o.arg2;
  }
};

IR::Code local_cse(const IR::Code& code) {
  std::map<Expr, std::string> cse_map;
  IR::Code result;

  auto kill_exprs_using = [&](const std::string& var) {
    if (var.empty()) return;
    for (auto it = cse_map.begin(); it != cse_map.end(); ) {
      if (it->first.arg1 == var || it->first.arg2 == var) {
        it = cse_map.erase(it);
      } else {
        ++it;
      }
    }
  };

  for (auto& node : code) {
    if (type_of<IR::Label>(node) || type_of<IR::Function>(node)) {
      cse_map.clear();
      result.push_back(node);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::LoadImm>(node)) {
      Expr expr = {"#", std::to_string(p->k), ""};
      auto it = cse_map.find(expr);
      if (it != cse_map.end()) {
        result.push_back(IR::Assign::create(p->x, it->second));
      } else {
        result.push_back(node);
        cse_map[expr] = p->x;
      }
      kill_exprs_using(p->x);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::LoadAddr>(node)) {
      Expr expr = {"&", p->label, ""};
      auto it = cse_map.find(expr);
      if (it != cse_map.end()) {
        result.push_back(IR::Assign::create(p->x, it->second));
      } else {
        result.push_back(node);
        cse_map[expr] = p->x;
      }
      kill_exprs_using(p->x);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::Unary>(node)) {
      Expr expr = {op_to_string(p->op), p->y, ""};
      auto it = cse_map.find(expr);
      if (it != cse_map.end()) {
        result.push_back(IR::Assign::create(p->x, it->second));
      } else {
        result.push_back(node);
        cse_map[expr] = p->x;
      }
      kill_exprs_using(p->x);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::Binary>(node)) {
      std::string arg1 = p->y;
      std::string arg2 = p->z;
      if (p->op == BinaryOp::Add || p->op == BinaryOp::Mul ||
          p->op == BinaryOp::Eq || p->op == BinaryOp::Neq ||
          p->op == BinaryOp::And || p->op == BinaryOp::Or) {
        if (arg1 > arg2) {
          std::swap(arg1, arg2);
        }
      }
      Expr expr = {op_to_string(p->op), arg1, arg2};
      auto it = cse_map.find(expr);
      if (it != cse_map.end()) {
        result.push_back(IR::Assign::create(p->x, it->second));
      } else {
        result.push_back(node);
        cse_map[expr] = p->x;
      }
      kill_exprs_using(p->x);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::Assign>(node)) {
      kill_exprs_using(p->x);
    } else if (auto p = std::dynamic_pointer_cast<IR::Call>(node)) {
      if (!p->x.empty()) kill_exprs_using(p->x);
    } else if (auto p = std::dynamic_pointer_cast<IR::Param>(node)) {
      kill_exprs_using(p->x);
    } else if (auto p = std::dynamic_pointer_cast<IR::Load>(node)) {
      kill_exprs_using(p->x);
    } else if (auto p = std::dynamic_pointer_cast<IR::LoadOffset>(node)) {
      kill_exprs_using(p->x);
    } else if (auto p = std::dynamic_pointer_cast<IR::Dec>(node)) {
      kill_exprs_using(p->x);
    }

    result.push_back(node);
  }
  return result;
}

}  // namespace

IR::Code IROptimizer::optimize(const IR::Code& code) {
  IR::Code result;
  IR::Code current_func;

  for (auto& node : code) {
    if (type_of<IR::Global>(node)) {
      result.push_back(node);
      continue;
    }

    if (type_of<IR::Function>(node) && !current_func.empty()) {
      auto optimized = optimize_function(current_func);
      std::move(optimized.begin(), optimized.end(), std::back_inserter(result));
      current_func.clear();
    }
    current_func.push_back(node);
  }

  if (!current_func.empty()) {
    auto optimized = optimize_function(current_func);
    std::move(optimized.begin(), optimized.end(), std::back_inserter(result));
  }

  return result;
}

IR::Code IROptimizer::optimize_function(const IR::Code& code) {
  std::unordered_map<std::string, int> constants;
  IR::Code folded;

  for (auto& node : code) {
    if (type_of<IR::Label>(node) || type_of<IR::Function>(node)) {
      constants.clear();
      folded.push_back(node);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::LoadImm>(node)) {
      constants[p->x] = p->k;
      folded.push_back(node);
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::Assign>(node)) {
      if (auto value = lookup_const(p->y, constants)) {
        constants[p->x] = *value;
        folded.push_back(IR::LoadImm::create(p->x, *value));
      } else {
        constants.erase(p->x);
        folded.push_back(node);
      }
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::Unary>(node)) {
      if (auto value = lookup_const(p->y, constants)) {
        int result = eval_unary(p->op, *value);
        constants[p->x] = result;
        folded.push_back(IR::LoadImm::create(p->x, result));
      } else {
        constants.erase(p->x);
        folded.push_back(node);
      }
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::Binary>(node)) {
      auto lhs = lookup_const(p->y, constants);
      auto rhs = lookup_const(p->z, constants);
      if (lhs && rhs && !((p->op == BinaryOp::Div || p->op == BinaryOp::Mod) && *rhs == 0)) {
        int result = eval_binary(*lhs, p->op, *rhs);
        constants[p->x] = result;
        folded.push_back(IR::LoadImm::create(p->x, result));
      } else {
        constants.erase(p->x);
        folded.push_back(node);
      }
      continue;
    }

    if (auto p = std::dynamic_pointer_cast<IR::If>(node)) {
      auto lhs = lookup_const(p->x, constants);
      auto rhs = lookup_const(p->y, constants);
      if (lhs && rhs) {
        if (eval_cond(*lhs, p->op, *rhs)) {
          folded.push_back(IR::Goto::create(p->label));
          constants.clear();
        }
      } else {
        folded.push_back(node);
      }
      continue;
    }

    kill_def(node, constants);
    if (type_of<IR::Goto>(node) || type_of<IR::Return>(node) ||
        type_of<IR::Call>(node)) {
      constants.clear();
    }
    folded.push_back(node);
  }

  int prev_size = 0;
  int iterations = 0;
  while (iterations < 10) {
    folded = local_lle(folded);
    folded = copy_prop(folded);
    folded = local_cse(folded);
    int curr_size = folded.size();
    if (curr_size == prev_size) break;
    prev_size = curr_size;
    iterations++;
  }
  folded = remove_unreachable_after_jumps(folded);
  folded = remove_jump_to_next_label(folded);
  folded = remove_local_dead_defs(folded);
  folded = remove_jump_to_next_label(folded);
  return folded;
}
