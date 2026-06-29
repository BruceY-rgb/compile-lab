# Lab4 RISC-V 代码生成实现计划

## Context

编译原理 lab4 任务：将 Zero IR 转换为 RISC-V 32 汇编代码。模板代码提供了骨架，6 个文件中留有 `#warning` 标记，需要逐一实现。项目 pipeline：源码 → AST → IR → CFG(Module/Function/BasicBlock) → InstSelector → RegAllocator → ASMEmitter。

## 实现顺序

### 阶段 1：基础设施（4 个独立任务）

**Task 1.1 — 扩展 `src/codegen/asm.hpp`**
- `Arith::Op` 增加 `Mul, Div, Rem, Slt, Sltu`
- 实现 `ArithImm` 类（addi/slti/sltiu/xori，含 get_uses/get_defs/replace_*）
- 实现 `La` 类（`la rd, label`）
- 实现 `Branch` 类（Beq/Bne/Blt/Bge 四种分支操作，含 get_uses/get_defs/replace_*）

**Task 1.2 — 实现 `src/analysis/control_flow.cpp`**
- `alloc_temp(size)`: 返回当前 temp_stack_size，然后累加
- `alloc_reg(size)`: 返回当前 reg_stack_size，然后累加

**Task 1.3 — 全局变量支持**
- `control_flow.hpp` 的 Module 添加 `std::vector<IR::GlobalPtr> globals`
- `cfg_builder.cpp` 的 `build()` 方法中拦截 Global 节点存入 mod.globals

**Task 1.4 — 配置 `config.toml`**
- `use_qemu = true`（lab 要求）

### 阶段 2：指令选择（核心）

**Task 2 — 实现 `src/codegen/inst_selector.cpp` + `.hpp`**

`.hpp` 新增声明：`selectIf`, `selectParam`, `selectDec`, `selectLoadAddr`, `selectLoad`, `selectStore`, `selectLoadOffset`, `selectStoreOffset`，以及辅助方法 `new_temp()`、成员变量 `param_index`、`temp_counter`。

`.cpp` 实现所有 select 函数：

| IR 节点 | ASM 指令 |
|---------|---------|
| `LoadImm: x = #k` | `li reg(x), k` |
| `Assign: x = y` | `mv reg(x), reg(y)`（已完成） |
| `Binary: x = y op z` | 依 op 选择 add/sub/mul/div/rem/slt；关系运算用 slt+sltiu 组合产生 0/1 |
| `Unary: x = -y` | `sub reg(x), zero, reg(y)` |
| `Unary: x = !y` | `sltiu reg(x), reg(y), 1`（即 seqz） |
| `Unary: x = +y` | `mv reg(x), reg(y)` |
| `Label: LABEL l:` | `l:` |
| `Goto: GOTO l` | `j l` |
| `If: IF x op y GOTO l` | 依 op 选 beq/bne/blt/bge（含操作数交换） |
| `Function: FUNCTION f:` | `f:` |
| `Call: x = CALL f` | `call f`; `mv reg(x), a0` |
| `Call: CALL f` | `call f` |
| `Arg: ARG x` | `mv a{k}, reg(x)` |
| `Param: PARAM x` | k<8→`mv reg(x), a{k}`; k>=8→`lw reg(x), 8+(k-8)*4(fp)` |
| `Dec: DEC x #k` | alloc_temp(k); `addi reg(x), sp, offset` |
| `Load: x = *y` | `lw reg(x), 0(reg(y))` |
| `Store: *x = y` | `sw reg(y), 0(reg(x))` |
| `LoadOffset: x = *(y + #k)` | `lw reg(x), k(reg(y))` |
| `StoreOffset: *(x + #k) = y` | `sw reg(y), k(reg(x))` |
| `LoadAddr: x = &y` | `la reg(x), y` |
| `Return` | 空（由 CFG/emitter 处理） |

关键注意点：
- 操作数为 `#k` 格式时，用 `Li` 加载到临时虚拟寄存器（`new_temp()` 生成 `@tmpN`）
- Binary 关系运算在表达式上下文中用 `slt`/`sltiu` 产生 0/1 值
- If 分支条件操作数交换：Gt→Blt(交换)，Le→Bge(交换)

### 阶段 3：寄存器分配

**Task 3 — 实现 `src/codegen/reg_allocator.cpp`**

朴素 spill-all-to-stack 策略：
1. 调用 `func->alloc_reg(8)` 为 ra/fp 保留空间
2. 遍历所有指令，收集所有非物理寄存器名
3. 每个虚拟寄存器分配栈槽 `alloc_temp(4)`
4. 遍历每条指令：
   - 对每个虚拟寄存器 use：指令前插入 `lw tN, offset(sp)`
   - 替换指令中的虚拟寄存器 → tN
   - 对每个虚拟寄存器 def：指令后插入 `sw tN, offset(sp)`
5. 使用 t0/t1/t2 三个临时物理寄存器轮转

### 阶段 4：汇编输出

**Task 4 — 实现 `src/codegen/asm_emitter.cpp`**

- **全局变量**：`.data` 段，`.globl` 声明，`.word` 初始化值或 `.zero` 零填充
- **函数 prologue**：
  ```
  addi sp, sp, -frame_size
  sw ra, frame_size-4(sp)
  sw fp, frame_size-8(sp)
  addi fp, sp, frame_size-8
  ```
- **函数 epilogue**（放在所有 block 之后）：
  ```
  lw ra, frame_size-4(sp)
  lw fp, frame_size-8(sp)
  addi sp, sp, frame_size
  ret
  ```
- frame_size 向上取整到 16 字节对齐
- 每个函数前加 `.globl func_name`
- block 循环改为遍历所有 block（不再跳过 exit block）

## 验证

1. `make` 编译通过
2. `python3 sp26-tests/test.py lab4 .` 运行 lab4 测试
3. 先用 Venus 模式测试（`use_qemu = false`），再切 QEMU（`use_qemu = true`）
4. 检查生成的汇编代码：`./compiler test.sy output.S --venus`
