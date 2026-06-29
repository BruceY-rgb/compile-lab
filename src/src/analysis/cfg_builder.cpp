#include "cfg_builder.hpp"

#include <algorithm>
#include <queue>
#include <set>
#include <unordered_map>

Module CFGBuilder::build(IR::Code code) {
  Module mod;
  IR::Code current_func;

  // 按函数分割IR代码，全局变量先收集
  for (const auto& inst : code) {
    if (auto g = std::dynamic_pointer_cast<IR::Global>(inst)) {
      mod.globals.push_back(g);
      continue;
    }
    if (auto func = std::dynamic_pointer_cast<IR::Function>(inst)) {
      if (!current_func.empty()) {
        mod.functions.push_back(build_single_func(current_func));
        current_func.clear();
      }
    }
    current_func.push_back(inst);
  }

  if (!current_func.empty()) {
    mod.functions.push_back(build_single_func(current_func));
  }

  return mod;
}

FunctionPtr CFGBuilder::build_single_func(IR::Code code) {
  std::string func_name;
  bool ret_void = true;  // 函数返回值是否为 void

  IR::Code normalized;
  for (const auto& inst : code) {
    if (auto func = std::dynamic_pointer_cast<IR::Function>(inst)) {
      func_name = func->name;
      normalized.push_back(inst);
    } else if (auto ret = std::dynamic_pointer_cast<IR::Return>(inst)) {
      if (ret->x.empty()) {
        normalized.push_back(IR::Goto::create(func_name + ".ret"));
      } else {
        normalized.push_back(IR::Assign::create("a0", ret->x));
        normalized.push_back(IR::Goto::create(func_name + ".ret"));
        ret_void = false;
      }
    } else {
      normalized.push_back(inst);
    }
  }

  std::vector<BasicBlockPtr> blocks;
  std::unordered_map<std::string, BasicBlockPtr> label_to_block;

  auto is_terminator = [](const IR::NodePtr& inst) {
    return type_of<IR::Goto>(inst) || type_of<IR::If>(inst);
  };

  auto finish_block = [&](std::string& label, IR::Code& current) {
    if (current.empty()) {
      return;
    }
    auto block = BasicBlock::create(label, current);
    blocks.push_back(block);
    label_to_block[label] = block;
    current.clear();
  };

  IR::Code current;
  std::string current_label = func_name + ".entry";
  for (const auto& inst : normalized) {
    if (auto func = std::dynamic_pointer_cast<IR::Function>(inst)) {
      current_label = func->name + ".entry";
      current.push_back(inst);
      continue;
    }

    if (auto label = std::dynamic_pointer_cast<IR::Label>(inst)) {
      finish_block(current_label, current);
      current_label = label->label;
      current.push_back(inst);
      continue;
    }

    current.push_back(inst);
    if (is_terminator(inst)) {
      finish_block(current_label, current);
      current_label = new_label();
    }
  }
  finish_block(current_label, current);

  auto exit_block = BasicBlock::create(func_name + ".ret");
  exit_block->ir_code.push_back(IR::Label::create(func_name + ".ret"));
  if (!ret_void) {
    exit_block->ir_code.push_back(IR::Return::create("a0"));
  } else {
    exit_block->ir_code.push_back(IR::Return::create());
  }
  blocks.push_back(exit_block);
  label_to_block[func_name + ".ret"] = exit_block;

  for (size_t i = 0; i < blocks.size(); ++i) {
    auto& block = blocks[i];
    if (block->ir_code.empty()) {
      continue;
    }
    auto last = block->ir_code.back();
    auto add_edge = [&](const BasicBlockPtr& to) {
      if (!to) return;
      block->successors.push_back(to);
      to->predecessors.push_back(block);
    };

    if (auto go = std::dynamic_pointer_cast<IR::Goto>(last)) {
      add_edge(label_to_block[go->label]);
    } else if (auto branch = std::dynamic_pointer_cast<IR::If>(last)) {
      add_edge(label_to_block[branch->label]);
      if (i + 1 < blocks.size()) {
        add_edge(blocks[i + 1]);
      }
    } else if (i + 1 < blocks.size()) {
      add_edge(blocks[i + 1]);
    }
  }

  if (!blocks.empty()) {
    std::set<BasicBlock*> reachable;
    std::queue<BasicBlockPtr> worklist;
    worklist.push(blocks.front());
    reachable.insert(blocks.front().get());

    while (!worklist.empty()) {
      auto block = worklist.front();
      worklist.pop();
      for (auto& weak_succ : block->successors) {
        if (auto succ = weak_succ.lock()) {
          if (reachable.insert(succ.get()).second) {
            worklist.push(succ);
          }
        }
      }
    }

    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                [&](const BasicBlockPtr& block) {
                                  return !reachable.count(block.get());
                                }),
                 blocks.end());
  }

  return Function::create(func_name, blocks);
}

std::string CFGBuilder::new_label() {
  static int label_count = 0;
  return "LN" + std::to_string(label_count++);
}
