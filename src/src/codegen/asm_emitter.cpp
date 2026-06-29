#include "asm_emitter.hpp"

void ASMEmitter::emit(const Module& mod) {
  // 输出全局变量
  if (!mod.globals.empty()) {
    output << ".data" << std::endl;
    for (auto& g : mod.globals) {
      output << "    .globl " << g->x << std::endl;
      output << g->x << ":" << std::endl;
      if (g->init_values.empty()) {
        for (int j = 0; j < g->size / 4; j++)
          output << "    .word 0" << std::endl;
      } else {
        for (int v : g->init_values) {
          output << "    .word " << v << std::endl;
        }
      }
    }
  }

  output << ".text" << std::endl;

  // 添加 Venus 的 read 和 write 系统调用
  if (use_venus) {
    output << R"(
    .globl read
read:
    li a0, 6
    ecall
    ret

    .globl write
write:
    mv a1, a0
    li a0, 1
    ecall
    ret

)";
  }

  for (const auto& func : mod.functions) {
    emit(func);
  }
}

void ASMEmitter::emit(const FunctionPtr& func) {
  int frame_size = func->temp_stack_size + func->reg_stack_size +
                    func->high_temp_size;
  frame_size = (frame_size + 15) & ~15;
  reg_map = func->reg_map;

  // 收集实际被分配使用的 callee-saved 寄存器（s1-s11），按数值顺序排序
  const std::set<std::string> callee_regs = {
      "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"};
  std::vector<std::string> used_callees;
  for (const auto& reg_name : callee_regs) {
    bool used = false;
    for (auto& [vreg, preg] : func->reg_map) {
      if (preg.name == reg_name) {
        used = true;
        break;
      }
    }
    if (used) {
      used_callees.push_back(reg_name);
    }
  }

  output << "    .globl " << func->name << std::endl;
  output << func->name << ":" << std::endl;

  // Prologue — 处理大立即数
  auto emit_sp_adjust = [&](int delta) {
    if (delta >= -2048 && delta <= 2047) {
      output << "    addi sp, sp, " << delta << std::endl;
    } else {
      output << "    li t3, " << abs(delta) << std::endl;
      output << "    sub sp, sp, t3" << std::endl;
    }
  };
  auto emit_sw_base = [&](const char* rs2, int off) {
    if (off <= 2047) {
      output << "    sw " << rs2 << ", " << off << "(sp)" << std::endl;
    } else {
      output << "    li t3, " << off << std::endl;
      output << "    add t3, sp, t3" << std::endl;
      output << "    sw " << rs2 << ", 0(t3)" << std::endl;
    }
  };
  auto emit_lw_base = [&](const char* rd, int off) {
    if (off <= 2047) {
      output << "    lw " << rd << ", " << off << "(sp)" << std::endl;
    } else {
      output << "    li t3, " << off << std::endl;
      output << "    add t3, sp, t3" << std::endl;
      output << "    lw " << rd << ", 0(t3)" << std::endl;
    }
  };

  emit_sp_adjust(-frame_size);
  emit_sw_base("ra", frame_size - 4);
  emit_sw_base("fp", frame_size - 8);

  int callee_offset = func->temp_stack_size;
  for (const auto& reg_name : used_callees) {
    emit_sw_base(reg_name.c_str(), callee_offset);
    callee_offset += 4;
  }

  if (frame_size - 8 <= 2047)
    output << "    addi fp, sp, " << (frame_size - 8) << std::endl;
  else {
    output << "    li t3, " << (frame_size - 8) << std::endl;
    output << "    add fp, sp, t3" << std::endl;
  }

  // Body: 跳过 exit block
  for (size_t i = 0; i < func->blocks.size() - 1; i++)
    emit(func->blocks[i]);

  // Epilogue
  output << func->name << ".ret:" << std::endl;
  emit_lw_base("ra", frame_size - 4);
  emit_lw_base("fp", frame_size - 8);

  callee_offset = func->temp_stack_size;
  for (const auto& reg_name : used_callees) {
    emit_lw_base(reg_name.c_str(), callee_offset);
    callee_offset += 4;
  }

  if (frame_size <= 2047)
    output << "    addi sp, sp, " << frame_size << std::endl;
  else {
    output << "    li t3, " << frame_size << std::endl;
    output << "    add sp, sp, t3" << std::endl;
  }
  output << "    ret" << std::endl;
}

void ASMEmitter::emit(const BasicBlockPtr& block) {
  for (const auto& inst : block->asm_code) {
    emit(inst);
  }
}

void ASMEmitter::emit(const ASM::InstPtr& inst) {
  inst->replace_all(reg_map);

  // 大偏移量分解：Load (lui+add+base-offset，仅正偏移用lui)
  if (auto load = std::dynamic_pointer_cast<ASM::Load>(inst)) {
    int off = load->offset;
    if (off < -2048 || off > 2047) {
      if (off >= 0) {
        int hi = off >> 12, lo = off & 0xFFF;
        if (lo >= 0x800) { hi++; lo -= 0x1000; }
        output << "    lui t3, " << hi << std::endl;
        output << "    add t3, " << load->rs1.name << ", t3" << std::endl;
        output << "    lw " << load->rd.name << ", " << lo << "(t3)"
               << std::endl;
      } else {
        output << "    li t3, " << off << std::endl;
        output << "    add t3, " << load->rs1.name << ", t3" << std::endl;
        output << "    lw " << load->rd.name << ", 0(t3)" << std::endl;
      }
      return;
    }
  }
  // 大偏移量分解：Store
  if (auto store = std::dynamic_pointer_cast<ASM::Store>(inst)) {
    int off = store->offset;
    if (off < -2048 || off > 2047) {
      if (off >= 0) {
        int hi = off >> 12, lo = off & 0xFFF;
        if (lo >= 0x800) { hi++; lo -= 0x1000; }
        output << "    lui t3, " << hi << std::endl;
        output << "    add t3, " << store->rs1.name << ", t3" << std::endl;
        output << "    sw " << store->rs2.name << ", " << lo << "(t3)"
               << std::endl;
      } else {
        output << "    li t3, " << off << std::endl;
        output << "    add t3, " << store->rs1.name << ", t3" << std::endl;
        output << "    sw " << store->rs2.name << ", 0(t3)" << std::endl;
      }
      return;
    }
  }
  // 大立即数分解：ArithImm
  if (auto ai = std::dynamic_pointer_cast<ASM::ArithImm>(inst)) {
    int imm = ai->imm;
    if (imm < -2048 || imm > 2047) {
      if (imm >= 0) {
        int hi = imm >> 12, lo = imm & 0xFFF;
        if (lo >= 0x800) { hi++; lo -= 0x1000; }
        output << "    lui t3, " << hi << std::endl;
        output << "    add " << ai->rd.name << ", " << ai->rs1.name
               << ", t3" << std::endl;
        if (lo != 0)
          output << "    addi " << ai->rd.name << ", " << ai->rd.name << ", "
                 << lo << std::endl;
      } else {
        output << "    li t3, " << imm << std::endl;
        output << "    add " << ai->rd.name << ", " << ai->rs1.name
               << ", t3" << std::endl;
      }
      return;
    }
  }

  if (type_of<ASM::Label>(inst)) {
    output << inst->to_string() << std::endl;
  } else {
    output << "    " << inst->to_string() << std::endl;
  }
}