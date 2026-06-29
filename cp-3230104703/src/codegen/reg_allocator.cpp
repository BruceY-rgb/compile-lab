#include "reg_allocator.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <memory>
#include <set>
#include <vector>

void RegAllocator::allocate(Module& mod) {
  for (auto& func : mod.functions) {
    allocate(func);
  }
}

void RegAllocator::allocate(FunctionPtr& func) {
  // 展平所有指令并编号
  struct Indexed { int idx; ASM::InstPtr inst; };
  std::vector<Indexed> indexed;
  for (auto& block : func->blocks)
    for (auto& inst : block->asm_code)
      indexed.push_back({(int)indexed.size(), inst});

  // 活度分析：计算每个虚拟寄存器的 [first_def, last_use] 和访问频度
  struct Info { int first = INT_MAX, last = -1, freq = 0; };
  std::map<std::string, Info> ranges;
  for (auto& item : indexed) {
    for (auto& r : item.inst->get_uses())
      if (!r.is_phys()) { auto& rg = ranges[r.name]; rg.first = std::min(rg.first, item.idx); rg.last = std::max(rg.last, item.idx); rg.freq++; }
    for (auto& r : item.inst->get_defs())
      if (!r.is_phys()) { auto& rg = ranges[r.name]; rg.first = std::min(rg.first, item.idx); rg.last = std::max(rg.last, item.idx); rg.freq++; }
  }

  // 处理循环 back-edge：反向跳转时延长循环体内所有虚拟寄存器的 last_use
  std::map<std::string, int> label_idx;
  for (auto& item : indexed)
    if (auto lab = std::dynamic_pointer_cast<ASM::Label>(item.inst))
      label_idx[lab->label] = item.idx;
  for (auto& item : indexed) {
    if (auto jmp = std::dynamic_pointer_cast<ASM::Jump>(item.inst)) {
      auto it = label_idx.find(jmp->label);
      if (it != label_idx.end() && it->second < item.idx) {
        int loop_start = it->second, loop_end = item.idx;
        for (auto& [name, rg] : ranges)
          if (rg.first < loop_end && rg.last >= loop_start)
            rg.last = std::max(rg.last, loop_end);
      }
    }
  }

  // 按频度降序排序 → 热 VR 优先分配物理寄存器
  struct VReg { std::string name; int first, last, freq; };
  std::vector<VReg> vregs;
  for (auto& [n, r] : ranges) vregs.push_back({n, r.first, r.last, r.freq});
  std::sort(vregs.begin(), vregs.end(), [](auto& a, auto& b) {
    return a.freq > b.freq || (a.freq == b.freq && a.first < b.first);
  });

  // 分配物理寄存器（s1-s11）
  std::vector<std::string> phys_regs = {
      "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"};
  std::map<std::string, std::vector<VReg>> phys_assignments;
  std::map<std::string, std::string> vreg_to_phys; // 虚拟寄存器名 → 物理寄存器名

  for (auto& vr : vregs) {
    std::string assigned_phys = "";
    for (auto& pr : phys_regs) {
      bool conflict = false;
      for (auto& assigned_vr : phys_assignments[pr]) {
        if (std::max(vr.first, assigned_vr.first) <= std::min(vr.last, assigned_vr.last)) {
          conflict = true;
          break;
        }
      }
      if (!conflict) {
        assigned_phys = pr;
        break;
      }
    }
    if (!assigned_phys.empty()) {
      vreg_to_phys[vr.name] = assigned_phys;
      phys_assignments[assigned_phys].push_back(vr);
    }
  }

  // 统计实际使用的物理寄存器数量，并为它们在 reg_stack 中分配空间
  std::set<std::string> used_callees;
  for (auto& [vname, pname] : vreg_to_phys) {
    used_callees.insert(pname);
  }

  // 重置/分配 reg_stack_size: ra/fp 占 8 字节，其余使用的 callee-saved 寄存器每个占 4 字节
  func->reg_stack_size = 0;
  func->alloc_reg(8);
  for (size_t i = 0; i < used_callees.size(); ++i) {
    func->alloc_reg(4);
  }

  // 贪心栈槽位分配：对未分配到物理寄存器的虚拟寄存器共享槽位
  int base = func->temp_stack_size;  // outgoing 预留区已经占了 sp 低位
  std::vector<int> slot_free_at;      // slot_free_at[i] = 该槽位 occupant 的 last_use
  std::map<std::string, int> vreg_offset;  // 虚拟寄存器名 → sp 偏移

  for (auto& vr : vregs) {
    if (vreg_to_phys.count(vr.name)) {
      continue;
    }
    bool reused = false;
    for (int s = 0; s < (int)slot_free_at.size(); s++) {
      if (slot_free_at[s] < vr.first) {
        slot_free_at[s] = vr.last;
        vreg_offset[vr.name] = base + s * 4;
        reused = true;
        break;
      }
    }
    if (!reused) {
      int s = (int)slot_free_at.size();
      slot_free_at.push_back(vr.last);
      vreg_offset[vr.name] = base + s * 4;
    }
  }

  // 更新帧大小（只增加实际需要的槽位）
  func->alloc_temp((int)slot_free_at.size() * 4);

  // 填充 func->reg_map
  func->reg_map = {};
  for (auto& [vname, pname] : vreg_to_phys) {
    func->reg_map.insert({ASM::Reg(vname), ASM::Reg(pname)});
  }

  // 指令展开：如果被分配到物理寄存器则直接替换，否则 lw→指令→sw
  ASM::Reg scratch[] = {ASM::Reg::t0, ASM::Reg::t1, ASM::Reg::t2};
  for (auto& block : func->blocks) {
    ASM::Code new_code;
    for (auto& inst : block->asm_code) {
      int si = 0;
      ASM::RegMap use_map, def_map;

      for (auto& reg : inst->get_uses()) {
        if (!reg.is_phys()) {
          if (vreg_to_phys.count(reg.name)) {
            use_map.insert({reg, ASM::Reg(vreg_to_phys[reg.name])});
          } else {
            ASM::Reg sr = scratch[si++ % 3];
            new_code.push_back(
                ASM::Load::create(sr, ASM::Reg::sp, vreg_offset[reg.name]));
            use_map.insert({reg, sr});
          }
        }
      }
      inst->replace_uses(use_map);

      for (auto& reg : inst->get_defs()) {
        if (!reg.is_phys()) {
          if (vreg_to_phys.count(reg.name)) {
            def_map.insert({reg, ASM::Reg(vreg_to_phys[reg.name])});
          } else {
            ASM::Reg sr = scratch[si++ % 3];
            def_map.insert({reg, sr});
          }
        }
      }
      inst->replace_defs(def_map);
      new_code.push_back(inst);

      for (auto& [vreg, sreg] : def_map) {
        if (!vreg_to_phys.count(vreg.name)) {
          new_code.push_back(
              ASM::Store::create(ASM::Reg::sp, sreg, vreg_offset[vreg.name]));
        }
      }
    }
    block->asm_code = new_code;
  }
}