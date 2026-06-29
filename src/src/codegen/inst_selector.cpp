#include "inst_selector.hpp"

#include <cassert>
#include <memory>

#include "codegen/asm_emitter.hpp"

std::string InstSelector::new_temp() {
  return "@tmp" + std::to_string(temp_counter++);
}

void InstSelector::select(Module& mod) {
  for (auto& func : mod.functions) {
    select(func);
  }
}

void InstSelector::select(FunctionPtr& func) {
  param_index = 0; // 重要：每次进入一个新函数时，重置参数计数器
  func->alloc_temp(112); // 为 outgoing 参数溢出区预留空间(覆盖 35 参数场景)
  current_func = func;
  for (auto& block : func->blocks) {
    block->asm_code = select(block->ir_code);
  }
}

ASM::Code InstSelector::select(const IR::Code& ir_code) {
  ASM::Code asm_code;
  for (const auto& node : ir_code) {
    auto code = select(node);
    asm_code.insert(asm_code.end(), code.begin(), code.end());
  }
  return asm_code;
}

ASM::Code InstSelector::select(const IR::NodePtr& node) {
#define SELECT_NODE(type)                                   \
  if (auto p = std::dynamic_pointer_cast<IR::type>(node)) { \
    return select##type(p);                                 \
  }

  // 对于每种不同类型的 IR 节点，调用相应的 select 函数
  // 如果你添加了新的 IR 节点类型，记得在这里添加对应的 select 函数
  SELECT_NODE(LoadImm)
  SELECT_NODE(Assign)
  SELECT_NODE(Binary)
  SELECT_NODE(Unary)
  SELECT_NODE(Label)
  SELECT_NODE(Goto)
  SELECT_NODE(If)
  SELECT_NODE(Function)
  SELECT_NODE(Call)
  SELECT_NODE(Arg)
  SELECT_NODE(Return)
  SELECT_NODE(LoadAddr)
  SELECT_NODE(Load)
  SELECT_NODE(Store)
  SELECT_NODE(LoadOffset)
  SELECT_NODE(StoreOffset)
  SELECT_NODE(Dec)
  SELECT_NODE(Param)


  assert(false && "Unknown IR node type");
}

ASM::Code InstSelector::selectLoadImm(const IR::LoadImmPtr& node) {
  ASM::Code code;
  // a = #t	-> li reg(a), t
  code.push_back(ASM::Li::create(ASM::Reg(node->x), node->k));

  return code;
}

ASM::Code InstSelector::selectAssign(const IR::AssignPtr& node) {
  ASM::Code code;
  // a = b	-> mv reg(a), reg(b)
  code.push_back(ASM::Mv::create(ASM::Reg(node->x), ASM::Reg(node->y)));
  return code;
}

ASM::Code InstSelector::selectBinary(const IR::BinaryPtr& node) {
  ASM::Code code;
  // a = b + c	-> add reg(a), reg(b), reg(c)
  auto y_name = node->y;
  auto z_name = node->z;

  if (node->op == BinaryOp::Mul) {
    if (z_name[0] == '#') {
      int val = std::stoi(z_name.substr(1));
      if (val > 0 && (val & (val - 1)) == 0) {
        int shamt = 0;
        while ((1 << shamt) < val) shamt++;
        if (y_name[0] == '#') {
          auto tmp = new_temp();
          int y_val = std::stoi(y_name.substr(1));
          code.push_back(ASM::Li::create(ASM::Reg(tmp), y_val));
          y_name = tmp;
        }
        code.push_back(ASM::ArithImm::create(ASM::Reg(node->x), ASM::Reg(y_name), shamt, ASM::ArithImm::Op::Sll));
        return code;
      }
    }
    if (y_name[0] == '#') {
      int val = std::stoi(y_name.substr(1));
      if (val > 0 && (val & (val - 1)) == 0) {
        int shamt = 0;
        while ((1 << shamt) < val) shamt++;
        if (z_name[0] == '#') {
          auto tmp = new_temp();
          int z_val = std::stoi(z_name.substr(1));
          code.push_back(ASM::Li::create(ASM::Reg(tmp), z_val));
          z_name = tmp;
        }
        code.push_back(ASM::ArithImm::create(ASM::Reg(node->x), ASM::Reg(z_name), shamt, ASM::ArithImm::Op::Sll));
        return code;
      }
    }
  }

  if(y_name[0] == '#') {
    auto tmp = new_temp();
    auto val = std::stoi(y_name.substr(1));
    code.push_back(ASM::Li::create(ASM::Reg(tmp), val));
    y_name = tmp;
  }

  if(z_name[0] == '#') {
    auto tmp = new_temp();
    auto val = std::stoi(z_name.substr(1));
    code.push_back(ASM::Li::create(ASM::Reg(tmp), val));
    z_name = tmp;
  }

  switch (node->op) {
    case BinaryOp::Add:
      code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Add));
      break;
    case BinaryOp::Sub:
      code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Sub));
      break;
    case BinaryOp::Mul:
      code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Mul));
      break;
    case BinaryOp::Div:
      code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Div));
      break;
    case BinaryOp::Mod:
      code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Rem));
      break;
    case BinaryOp::Lt:
      code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Slt));
      break;
    case BinaryOp::Gt:
      // x = y > z  =>  x = z < y
      code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg(z_name),
                                        ASM::Reg(y_name), ASM::Arith::Op::Slt));
      break;
    case BinaryOp::Le: {
      // x = y <= z  =>  x = !(z < y)
      auto t = new_temp();
      code.push_back(ASM::Arith::create(ASM::Reg(t), ASM::Reg(z_name),
                                        ASM::Reg(y_name), ASM::Arith::Op::Slt));
      code.push_back(ASM::ArithImm::create(ASM::Reg(node->x), ASM::Reg(t), 1,
                                           ASM::ArithImm::Op::Sltu));
      break;
    }
    case BinaryOp::Ge: {
      // x = y >= z  =>  x = !(y < z)
      auto t = new_temp();
      code.push_back(ASM::Arith::create(ASM::Reg(t), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Slt));
      code.push_back(ASM::ArithImm::create(ASM::Reg(node->x), ASM::Reg(t), 1,
                                           ASM::ArithImm::Op::Sltu));
      break;
    }
    case BinaryOp::Eq: {
      // x = y == z  =>  sub t, y, z; sltiu x, t, 1  (seqz)
      auto t = new_temp();
      code.push_back(ASM::Arith::create(ASM::Reg(t), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Sub));
      code.push_back(ASM::ArithImm::create(ASM::Reg(node->x), ASM::Reg(t), 1,
                                           ASM::ArithImm::Op::Sltu));
      break;
    }
    case BinaryOp::Neq: {
      // x = y != z  =>  sub t, y, z; sltu x, zero, t  (snez)
      auto t = new_temp();
      code.push_back(ASM::Arith::create(ASM::Reg(t), ASM::Reg(y_name),
                                        ASM::Reg(z_name), ASM::Arith::Op::Sub));
      code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg::zero,
                                        ASM::Reg(t), ASM::Arith::Op::Sltu));
      break;
    }
    default:
      assert(false && "Unexpected binary op");
  }


  return code;
}

ASM::Code InstSelector::selectUnary(const IR::UnaryPtr& node) {
  ASM::Code code;
  // a = -b	-> sub reg(a), zero, reg(b)

  if (node->op == UnaryOp::Neg) {
    code.push_back(ASM::Arith::create(ASM::Reg(node->x), ASM::Reg::zero,
                                      ASM::Reg(node->y), ASM::Arith::Op::Sub));
  } else if (node->op == UnaryOp::Lnot) {
    code.push_back(ASM::ArithImm::create(ASM::Reg(node->x), ASM::Reg(node->y), 1,
                                         ASM::ArithImm::Op::Sltu));
  } else {
    code.push_back(ASM::Mv::create(ASM::Reg(node->x), ASM::Reg(node->y)));
  }
  return code;
}

ASM::Code InstSelector::selectLabel(const IR::LabelPtr& node) {
  ASM::Code code;
  // LABEL label:	-> label:
  code.push_back(ASM::Label::create(node->label));

  return code;
}

ASM::Code InstSelector::selectGoto(const IR::GotoPtr& node) {
  ASM::Code code;
  // GOTO label	-> j label
  code.push_back(ASM::Jump::create(node->label));

  return code;
}

ASM::Code InstSelector::selectFunction(const IR::FunctionPtr& node) {
  // 函数标签由 asm_emitter 在 prologue 中统一输出
  return {};
}

ASM::Code InstSelector::selectCall(const IR::CallPtr& node) {
  ASM::Code code;
  // CALL func	-> call func
  // x = CALL func	-> call func; mv reg(x), a0
  arg_index = 0;  // 重置实参计数器
  code.push_back(ASM::Call::create(node->func));
  if(node->x != "") {
    code.push_back(ASM::Mv::create(ASM::Reg(node->x), ASM::Reg("a0")));
  }

  return code;
}

ASM::Code InstSelector::selectArg(const IR::ArgPtr& node) {
  ASM::Code code;
  // ARG x  →  mv a{k}, reg(x)        (k < 8)
  //         →  sw reg(x), (k-8)*4(sp) (k >= 8, outgoing arg area)
  int k = arg_index++;
  if (k < 8) {
    code.push_back(
        ASM::Mv::create(ASM::Reg("a" + std::to_string(k)),
                        ASM::Reg(node->x)));
  } else {
    code.push_back(
        ASM::Store::create(ASM::Reg::sp, ASM::Reg(node->x),
                           (k - 8) * 4));
  }

  return code;
}

ASM::Code InstSelector::selectLoadAddr(const IR::LoadAddrPtr& node) {
  ASM::Code code;
  // a = &b	-> la reg(a), b
  code.push_back(ASM::La::create(ASM::Reg(node->x), node->label));

  return code;
}

ASM::Code InstSelector::selectLoad(const IR::LoadPtr& node) {
  ASM::Code code;

  // a = b -> lw reg(a), 0(reg(b))
  code.push_back(ASM::Load::create(ASM::Reg(node->x), ASM::Reg(node->y), 0));
  return code;
}

ASM::Code InstSelector::selectStore(const IR::StorePtr& node) {
  ASM::Code code;
  // *b = a -> sw reg(a), 0(reg(b))
  code.push_back(ASM::Store::create(ASM::Reg(node->x), ASM::Reg(node->y), 0));

  return code;
}

ASM::Code InstSelector::selectLoadOffset(const IR::LoadOffsetPtr& node) {
  ASM::Code code;

  // a = *(b + offset) -> lw reg(a), offset(reg(b))
  code.push_back(ASM::Load::create(ASM::Reg(node->x), ASM::Reg(node->y), node->offset));
  return code;
}

ASM::Code InstSelector::selectStoreOffset(const IR::StoreOffsetPtr& node) {
  ASM::Code code;
  // *(a + offset) = b -> sw reg(b), offset(reg(a))
  code.push_back(ASM::Store::create(ASM::Reg(node->x), ASM::Reg(node->y), node->offset));

  return code;
}

ASM::Code InstSelector::selectIf(const IR::IfPtr& node) {
  ASM::Code code;
  // IF x op y GOTO label -> b<op> reg(x), reg(y), label

  auto x_name = node->x;
  auto y_name = node->y;

  if (x_name[0] == '#') {
    auto tmp = new_temp();
    auto val = std::stoi(x_name.substr(1));
    code.push_back(ASM::Li::create(ASM::Reg(tmp), val));
    x_name = tmp;
  }

  if (y_name[0] == '#') {
    auto tmp = new_temp();
    auto val = std::stoi(y_name.substr(1));
    code.push_back(ASM::Li::create(ASM::Reg(tmp), val));
    y_name = tmp;
  }

  switch (node->op) {
    case BinaryOp::Eq:
      code.push_back(ASM::Branch::create(ASM::Reg(x_name), ASM::Reg(y_name),
                                         node->label, ASM::Branch::Op::Beq));
      break;
    case BinaryOp::Neq:
      code.push_back(ASM::Branch::create(ASM::Reg(x_name), ASM::Reg(y_name),
                                         node->label, ASM::Branch::Op::Bne));
      break;
    case BinaryOp::Lt:
      code.push_back(ASM::Branch::create(ASM::Reg(x_name), ASM::Reg(y_name),
                                         node->label, ASM::Branch::Op::Blt));
      break;
    case BinaryOp::Gt:
      // x > y  =>  y < x
      code.push_back(ASM::Branch::create(ASM::Reg(y_name), ASM::Reg(x_name),
                                         node->label, ASM::Branch::Op::Blt));
      break;
    case BinaryOp::Le:
      // x <= y  =>  y >= x
      code.push_back(ASM::Branch::create(ASM::Reg(y_name), ASM::Reg(x_name),
                                         node->label, ASM::Branch::Op::Bge));
      break;
    case BinaryOp::Ge:
      code.push_back(ASM::Branch::create(ASM::Reg(x_name), ASM::Reg(y_name),
                                         node->label, ASM::Branch::Op::Bge));
      break;
    default:
      assert(false && "Unexpected branch op");
  }

  return code;
}

ASM::Code InstSelector::selectDec(const IR::Decptr& node) {
  ASM::Code code;
  // DEC x size: 从 fp 向下分配大对象，使虚拟寄存器 sp 偏移保持较小
  int offset = current_func->alloc_temp_high(node->k);
  code.push_back(ASM::ArithImm::create(ASM::Reg(node->x), ASM::Reg::fp, -offset,
                                       ASM::ArithImm::Op::Add));

  return code;
}

ASM::Code InstSelector::selectParam(const IR::ParamPtr& node) {
  ASM::Code code;
  // 前8个参数放在a0~a7寄存器中，超过8个参数放在栈上
  int k = param_index++;
  if (k < 8) {
    code.push_back(ASM::Mv::create(ASM::Reg(node->x),
                                   ASM::Reg("a" + std::to_string(k))));
  } else {
    code.push_back(ASM::Load::create(ASM::Reg(node->x), ASM::Reg::fp,
                                     8 + (k - 8) * 4));
  }

  return code;
}
ASM::Code InstSelector::selectReturn(const IR::ReturnPtr& node) {
  ASM::Code code;
  // 已经在 cfg builder 中统一为一个 exit call
  // 因此只有 exit block 里有 return 语句
  // 在 asm emitter 中对每个函数处理时
  // 会忽略 exit block 并添加 epilogue
  // 因此这里不需要处理

return code;
}
