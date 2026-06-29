# Lab 4: RISC-V 目标代码生成

!!! info "开始之前" 请务必在 **2026-6-21 23:59** 前完成实验并推送到你自己对应的远程仓库的 `lab4` 分支中，否则将会按照评分标准扣分。

```
建议你在开始前，先使用 `git checkout -b lab4 lab3` 从 `lab3` 分支创建一个新的 `lab4` 分支，并使用 `git push -u origin lab4` 在远程仓库中创建该分支，这样你就可以在实验过程中随时使用 `git push origin lab4` 提交代码，而不会误交到其他分支。
```

!!! abstract "[Checklist: 测试 Bug 排查](./lab0.md#bug-checklist)"

??? note "模版代码使用指南"

````
=== "Zero IR"
如果你选用 Zero IR 作为中间代码，Lab 4 部分的模板代码位于仓库中的 `template/lab4-zero` 分支下，你可以将其合并到自己的 `lab4` 分支中:

```console
$ git merge upstream/template/lab4-zero
```

如果出现冲突，Git 会提示你解决冲突，手动编辑冲突的文件并在解决后执行：

```console
$ git add &lt;conflict-file&gt;
$ git commit
```

合并完成后，你可以先把模板代码推送到远程仓库：

```console
$ git push origin lab4
```

然后开始修改模板代码，完成你的编译器。

Lab 4 框架的文件结构如下，高亮部分为新增和改动的文件：

```plaintext hl_lines="5-6 10-17 25"
src
├── analysis
│   ├── cfg_builder.cpp
│   ├── cfg_builder.hpp
│   ├── control_flow.cpp
│   └── control_flow.hpp
├── ast
│   ├── tree.cpp
│   └── tree.hpp
├── codegen
│   ├── asm.hpp             // ASM 指令定义
│   ├── asm_emitter.cpp     // 汇编代码生成
│   ├── asm_emitter.hpp
│   ├── inst_selector.cpp   // 指令选择
│   ├── inst_selector.hpp
│   ├── reg_allocator.cpp   // 寄存器分配
│   └── reg_allocator.hpp
├── common.hpp
├── ir
│   ├── ir.hpp
│   ├── ir_translator.cpp
│   └── ir_translator.hpp
├── lexer
│   └── lexer.l
├── main.cpp
├── parser
│   └── parser.y
└── semantic
    ├── symbol_table.cpp
    ├── symbol_table.hpp
    ├── type.cpp
    ├── type.hpp
    ├── type_checker.cpp
    └── type_checker.hpp
```=== "Accipit IR"如果你选用 Accipit IR 作为中间代码，Lab 4 部分的模板代码位于仓库中的 `template/lab4-accipit` 分支下，你可以将其合并到自己的 `lab4` 分支中，你可以将其合并到自己的 `lab4` 分支中:

```console
$ git merge upstream/template/lab4-accipit
```

如果出现冲突，Git 会提示你解决冲突，手动编辑冲突的文件并在解决后执行：

```console
$ git add &lt;conflict-file&gt;
$ git commit
```

合并完成后，你可以先把模板代码推送到远程仓库：

```console
$ git push origin lab4
```

然后开始修改模板代码，完成你的编译器。

Lab 4 框架的文件结构如下，高亮部分为新增和改动的文件：

```plaintext hl_lines="5-13 22"
src
├── ast
│&nbsp;&nbsp; ├── tree.cpp
│&nbsp;&nbsp; └── tree.hpp
├── codegen
│&nbsp;&nbsp; ├── asm.def // ASM 指令定义
│&nbsp;&nbsp; ├── asm.hpp
│&nbsp;&nbsp; ├── asm_emitter.cpp // 汇编代码生成
│&nbsp;&nbsp; ├── asm_emitter.hpp
│&nbsp;&nbsp; ├── inst_selector.cpp // 指令选择
│&nbsp;&nbsp; ├── inst_selector.hpp
│&nbsp;&nbsp; ├── reg_allocator.cpp // 寄存器分配
│&nbsp;&nbsp; └── reg_allocator.hpp
├── common.hpp
├── ir
│&nbsp;&nbsp; ├── ir.def
│&nbsp;&nbsp; ├── ir.hpp
│&nbsp;&nbsp; ├── ir_translator.cpp
│&nbsp;&nbsp; └── ir_translator.hpp
├── lexer
│&nbsp;&nbsp; └── lexer.l
├── main.cpp
├── parser
│&nbsp;&nbsp; └── parser.y
└── semantic
    ├── symbol_table.cpp
    ├── symbol_table.hpp
    ├── type.cpp
    ├── type.hpp
    ├── type_checker.cpp
    └── type_checker.hpp
```??? note "OCaml 模板"OCaml 模板提供了两种 codegen 路径，分别在 `template/ocaml/lab4-zero` 和 `template/ocaml/lab4-accipit` 分支。新增的文件结构：

```text
codegen
└── gen_zero.ml         # Zero IR -&gt; RISC-V (或 gen_accipit.ml)
```

OCaml 模板采用「全部 spill 到栈」的朴素寄存器分配策略：每个 SSA 值/变量分配一个栈槽，每条指令先从栈加载操作数到 `t0`-`t6`，计算后将结果存回栈。

模板已给出：

- 输出缓冲区（`buf`、`emit`、`emitf`）
- 栈帧信息类型（`frame_info`：`slot_map`、`frame_size`、`ra_offset`）
- 全局变量收集、Venus mode 标志
- Zero 路径：`split_functions` 函数（将平坦指令列表拆分为函数块）

你需要实现的 TODO：

- `load_imm`、`load_sp`、`store_sp`、`adjust_sp`：RISC-V 汇编辅助函数（注意大立即数分解）
- `compute_frame`：计算栈帧布局
- `gen_instr`：每条 IR 指令的代码生成
- `gen_func`（Accipit 路径）：函数 prologue/epilogue 生成
- `.data` 段的全局变量输出
````

本次实验的目标是从实验 3 生成的中间表示出发，生成 RISC-V 32 的汇编代码，并在 Venus 模拟器或 QEMU 上运行。 如果你对 RISC-V 汇编不那么熟悉，可以参考 [Crash Course](../appendix/riscv.md)。

回顾一下，我们已经可以将源代码转换为一种自定义的中间代码表示，它可以在一个自定义虚拟机器上解释执行。和实际机器不同，虚拟机允许你使用无限寄存器，并给你提供了一些方便的关于函数调用的指令。 而面对一个实际的机器，如在本实验使用的 RISC-V 32 指令集下，这些假设都不再成立。为了生成最终目标代码，有以下三个问题需要你解决：

## 指令选择

首先我们注意到中间代码和汇编代码格式是不一样的，我们要对每一种中间代码找到一个特定的模式，翻译成对应的目标代码，这个过程就是指令选择。 考虑到在本实验中实现的中间表示是线性的，最直接的方式就是将每一条中间代码翻译成一条或多条目标代码。 下表是部分中间代码与 RISC-V 32 指令对应的一个示例，大部分的逻辑都是显然的：

=== "Zero IR"

```
| 中间代码                | 目标代码                      | 中间代码                 | 目标代码                      |
|-----------------------|------------------------------|------------------------|------------------------------|
| `a = b + c`           | `add reg(a), reg(b), reg(c)` | `a = b - c`            | `sub reg(a), reg(b), reg(c)` |
| `a = b + #t`          | `addi reg(a), reg(b), t`     | `a = b - #t`           | `addi  reg(a), reg(b), -t`   |
| `a = b`               | `mv reg(a), reg(b)`          | `a = #t`               | `li reg(a), t`               |
| `LABEL label:`        | `label:`                     | `GOTO label`           | `j label`                    |
| `a = CALL f`          | `jal f ; move reg(a), a0`    | `RETURN a`             | `mv a0, reg(a) ; ret`        |
| `x = *y`              | `lw reg(x), 0(reg(y))`       | `*x = y`               | `sw reg(y), 0(reg(x))`       |
| `x = *(y + #k)`       | `lw reg(x), k(reg(y))`       | `*(x + #k) = y`        | `sw reg(y), k(reg(x))`       |
| `IF x > y GOTO label` | `bgt reg(x), reg(y), label`  | `IF x <= y GOTO label` | `ble reg(x), reg(y), label`  |
| `x = &y`              | `la reg(x), y`               | `GLOBAL x: #k`         | `x:`, `.zero k`/`.byte 0`*k  |
| `FUNCTION f:`         | `f:`                         | `GLOBAL x: #k = #v1, #v2, ...` | `x:`, `.word v1, v2, ...` |
!!! tip "Venus 可能不支持某些伪指令，例如 .zero k 在 Venus 中需要使用 .word 0, 0, ... 来替代。"
```

=== "Accipit IR"

```
| 中间代码                | 目标代码                      |
|-----------------------|------------------------------|
| `let %x = add %y, %z` | `add reg(x), reg(y), reg(z)` |
| `let %x = add %y, n`  | `addi reg(x), reg(y), n` |
| `let %x = add n, %y`  | `addi reg(x), reg(y), n`     |
| `let %x = add n1, n2`           | `li reg(x), n1 + n2`   |
| `let %x = call @fun1, %arg1`    | `mv a0, reg(arg1) ...; call fun1; mv reg(x), a0`          |
| `let %addr = offset type, %array, [%a < 3], [%b < 4], [%c < 5]`   | `mv reg(addr), reg(array); addi reg(addr), offset`               |
| `let %addr = alloc type n`  |  分配栈上的地址  |
| `let %x = load %addr`  | `lw reg(x), addr.offset(sp)`/`lw reg(x), 0(reg(addr))` |
| `let %x = store %addr, %x`  | `sw reg(x), addr.offset(sp)`/`sw reg(x), 0(reg(addr))` |
| `br %value, label %L1, label %L2`               | `bnez reg(value), L1; j L2`               |
| `jmp label %L1`               | `j L1`               |
| `ret %value`               | `mv a0, reg(value); ret`               | 
| `fn @f(#p1: t1, #p2: t2...) -> t {basic block list}`               | `f: ...` |
| `%Label:`        | `Label:`                     |
| `@x: region t1, n`        | `x: .word 0, 0 ...`                     |
| `@x: region t1, n = [v1, v2 ... vn]`        | `x: .word v1, v2, ... vn`                     |
```

RISC-V 32 仍有许多其他繁杂注意事项，如调用参数多于 8 个时用栈传参、部分二元运算符没有直接对应的汇编指令、部分指令没有立即数版本、立即数的大小限制、偏移量的大小限制等等，你需要在实现中考虑这些问题。

!!! tip "善用一些伪指令可以帮你节省许多工作。"

你可能已经发现了，我们在目标代码中对于某一个中间代码中的变量 `a` 使用的是虚拟寄存器 `reg(a)`，意为储存该变量的寄存器。所以，这其实仍然不是最终的汇编代码，而是一种 Machine-level IR。我们接下来需要进行寄存器分配，将这些虚拟寄存器映射到真实的物理寄存器上。

那么我们怎么知道每个虚拟寄存器应该用哪个物理寄存器储存呢？这也就引出了下一个问题：寄存器分配。

??? note "模板代码中的 ASM 结构定义与指令选择"

````
与 AST 和 IR 类似，我们需要定义一些结构体来表示汇编指令，包括指令的类型、操作数等。这些结构体定义在 `codegen/asm.hpp` 中。其中，基类 `ASM::Inst` 定义如下：
class Inst;
using InstPtr = std::shared_ptr&lt;Inst&gt;;
class Inst {
public:
virtual std::string to_string() const = 0;
virtual ~Inst() = default;  // make the class polymorphic

/// @brief 获取指令使用的寄存器
/// @return 使用的寄存器集合
virtual std::set&lt;Reg&gt; get_uses() const = 0;

/// @brief 获取指令定义的寄存器
/// @return 定义的寄存器集合
virtual std::set&lt;Reg&gt; get_defs() const = 0;

/// @brief 替换指令使用的寄存器
/// @param reg_map 寄存器映射 (old_reg -&gt; new_reg)
virtual void replace_uses(const RegMap &amp;reg_map) = 0;

/// @brief 替换指令定义的寄存器
/// @param reg_map 寄存器映射 (old_reg -&gt; new_reg)
virtual void replace_defs(const RegMap &amp;reg_map) = 0;

/// @brief 替换指令使用和定义的寄存器
/// @param reg_map 寄存器映射 (old_reg -&gt; new_reg)
virtual void replace_all(const RegMap &amp;reg_map) {
    replace_uses(reg_map);
    replace_defs(reg_map);
}
};其中，to_string 函数用于将指令转换为字符串，get_uses 和 get_defs 函数用于获取指令使用和定义的寄存器，replace_uses 和 replace_defs 函数用于替换指令使用和定义的寄存器。这些方法可以方便你实现寄存器分配。你可能已经发现了，我们使用 Reg 类对虚拟和物理寄存器进行管理。Reg 类定义如下：class Reg {
public:
static const Reg zero, ra, sp, /* more physical regs */ t5, t6;

std::string name;

Reg(std::string name) : name(name), physical(false) {
    static const std::set&lt;std::string&gt; physical_registers = {
        "zero", "ra", "sp", /* more physical regs */ "t5", "t6"};
    if (physical_registers.find(name) != physical_registers.end()) {
    physical = true;
    }
}

bool operator==(const Reg &amp;other) const {
    return name == other.name &amp;&amp; physical == other.physical;
}
bool operator!=(const Reg &amp;other) const { return !(*this == other); }
bool operator&lt;(const Reg &amp;other) const {
    return name &lt; other.name ||
        (name == other.name &amp;&amp; physical &lt; other.physical);
}
bool is_phys() const { return physical; }

private:
bool physical;
Reg(std::string name, bool physical) : name(name), physical(physical) {}
};接下来，我们需要将 IR 代码转换为 ASM 代码。=== "Zero IR"`InstSelector` 类负责将 Zero IR 中的代码节点转换为汇编指令。每个 IR 节点都有对应的 `select` 函数。以下是如何将一个 `Assign` 节点转换为汇编指令的示例：

```cpp
ASM::Code InstSelector::selectAssign(const IR::AssignPtr &amp;node) {
ASM::Code code;
// a = b	-&gt; mv reg(a), reg(b)
code.push_back(ASM::Mv::create(ASM::Reg(node-&gt;x), ASM::Reg(node-&gt;y)));
return code;
}
```

每个 IR 类型（如 `Assign`, `Load`, `Branch` 等）都需要实现类似的选择函数。通过 `InstSelector::select` 函数，你可以将不同类型的 IR 节点转换为相应的汇编指令。

为了保存生成的 ASM 指令，我们需要修改 `BasicBlock` 类，添加一个 `asm_code` 成员变量：

```cpp hl_lines="3" title="src/analysis/control_flow.hpp"
class BasicBlock {
public:
ASM::Code asm_code;

// ... 其他成员变量和函数 ... //
};
```

在 `InstSelector` 类中，将生成的 ASM 指令添加到 `BasicBlock` 的 `asm_code` 中即可。=== "Accipit IR"我们使用 `InstSelector` 类来将 Accipit IR 中的代码节点转换为汇编指令。每个 IR 节点都有对应的 `select` 函数。

```cpp
class InstSelector {
public:
    void select(IR::ModulePtr mod);

private:
    int temp_count = 0;
    // 用来存储翻译出来的所有 ASM
    ASM::Module module;
    // 当前翻译的基本块和函数
    ASM::BasicBlockPtr curr_bb;
    ASM::FunctionPtr curr_fn;

    void select(const std::shared_ptr&lt;IR::Node&gt; node);
    void selectFunction(const IR::FunctionPtr node);
    // 其他 select 函数
};
```

一般来说， select 函数只要将对应的 ASM 指令插入到 `curr_bb` 中即可。对于像 `alloc` 这样操纵栈空间的指令，我们需要调用当前函数 `curr_fn` 的 `alloc_temp` 来为其分配空间。
````

## 寄存器分配

对于中间代码中的全局变量，我们可以直接映射到目标代码的上，这是比较简单的。 但是除此之外，我们还使用了数目不受限的变量和临时变量，但处理器所拥有的寄存器数量是有限的。因此我们需要将中间代码中的变量映射到寄存器上，这个过程就是寄存器分配。

在本次实验中，你只需要完成一个朴素的寄存器分配。最朴素的寄存器分配方法当然是把几乎所有临时变量都存储在栈上。 每翻译一条中间代码之前我们把要用到的变量先加载到寄存器中，得到计算结果后又将结果写回内存。这种方法的确能将中间代码翻译成可以正常运行的目标代码，而且实现和调试都特别容易。比如说，对于下面这种还包含 `reg(a)` 的代码：

```asm
add reg(a), reg(b), reg(c)
```

假设你为 `a` `b` `c` 分配的空间在 `sp + 4` `sp + 8` `sp + 12`，那么你可以生成下面的代码：

```asm
lw t1, 8(sp)        # load b
lw t2, 12(sp)       # load c
add t0, t1, t2      # add reg(a), reg(b), reg(c)
sw t0, 4(sp)        # store a
```

实际上，这种方法就是对所有临时变量都做了 spilling。

<!-- Accipit IR 因为 `alloc` `load` `store` 三条指令的使用，直接对应到了这种寄存器分配方法。你只需要为那些临时变量做好寄存器分配即可。 -->

!!! tip "更高级的寄存器分配算法"

```
朴素的寄存器分配方法虽然简单，但是生成的代码效率很低。我们可以考虑使用一些更加高级的寄存器分配算法来优化生成的代码。
同学们已经在课程中学习过了基于图染色的寄存器分配方法，在对程序进行活跃变量分析之后，我们可以根据活跃变量的信息构造一个冲突图，然后使用图染色算法来进行寄存器分配。除此之外，还有更多高级的寄存器分配算法可以使用。具体可以参阅 Bonus 3 的 寄存器分配 部分。
```

??? note "模板代码中的寄存器分配"

```
寄存器分配阶段需要实现一个 `RegAllocator` 类，用于将虚拟寄存器分配到物理寄存器上。对于每一个函数，我们在完成寄存器分配后，可以得到一个 `RegMap`，将虚拟寄存器映射到物理寄存器。我们可以将其保存在 `Function` 类中，方便在后续的汇编指令发射中使用。
如果你只希望实现朴素的寄存器分配，也即所有要用的变量都从栈上存取，那么你也有两种选择，一种是以 RISC-V 指令为单位的朴素寄存器分配。在这种实现下你在 InstSelector 中使用虚拟寄存器完成指令选择得到 MLIR ，然后在 RegAllocator 中遍历所有的指令，对于每条汇编指令，为其中的虚拟寄存器分配空间并在语句前后添加 lw 和 sw 指令。这样的解耦对于后续实现更高级的寄存器分配算法会很有帮助。又或者，你可以以 IR 指令为单位进行朴素的寄存器分配，也就是对于每一个 IR 指令需要用到和存储的外部变量都从栈上存取，但是 IR 指令内部的运算可以使用寄存器来传递。对于 Accipit IR 这样存在比较复杂指令的中间代码，这样的分配可以减少不少栈访问。不过由于 IR 结构在指令选择后就消失了，这样的寄存器分配要和指令选择结合在一起实现。
```

## 栈帧管理

在朴素的寄存器分配方法中，所有临时变量存储在栈上。除此之外，函数调用还需要保存一些关键的寄存器值，例如保存返回地址的 `ra` 寄存器等。因此，每次函数调用都会创建一个栈帧，用于保存必要的数据。

### 栈帧布局

栈是从高地址向低地址增长的，一个最基本的栈帧结构如下：

```
                    │               │ ▲ High Address
                    │               │ │
                    │      ...      │
                    ├───────────────┤
                    │  argument n   │
        incoming    │      ...      │    previous
        arguments   │  argument 2   │     frame
                    │  argument 1   │
   frame pointer ──►├───────────────┼────────────────
                    │    return     │
                    │    address    │
                    ├───────────────┤
                    │ callee-saved  │
                    │   registers   │
                    ├───────────────┤
                    │    loacal     │    current
                    │   variables   │     frame
                    │       &       │
                    │  temporaries  │
                    ├───────────────┤
                    │  argument m   │
        outgoing    │      ...      │
        arguments   │  argument 2   │
                    │  argument 1   │
   stack pointer ──►├───────────────┼────────────────
                    │               │
                    │               │     next
                    │               │     frame
                    │               │ │
                    │               │ ▼ Low Address
```

每一个函数的栈帧中，从高地址到低地址依次包括：

1. \*\*返回地址：\*\*保存调用者的返回地址 `ra`，用于函数执行完成后跳转回调用点。
2. \*\*保存的寄存器：\*\*某些寄存器需要由被调用者保存，如 `s0-s11`（保存跨函数调用的数据）。
3. \*\*局部变量和临时变量：\*\*函数中定义的局部变量和临时变量，例如调用者负责保存的临时寄存器 `t0-t6`。
4. \*\*参数溢出空间：\*\*如果传递给函数的参数超过寄存器能容纳的数量（例如超过 `a0-a7` 的 8 个参数），多余的参数会存储在栈帧中。

当前函数的栈帧的地址范围由栈指针 `sp` 和帧指针 `s0/fp` 确定，为 `[sp, fp)`。对于较低的地址，你可以用 `sp` 加上偏移量来访问栈帧中的变量；对于较高的地址则可以使用 `fp`。因此，在处理函数调用过程中，我们要维护好这两个指针。

### 函数调用流程

在 RISC-V 的函数调用中，调用者（Caller）和被调用者（Callee）需要分工协作，维护栈帧并完成寄存器管理。调用者需要完成以任务：

1. 将参数放入寄存器 `a0-a7` 中。如果参数数量超过了 8 个，多余的参数需要存放在自己栈帧的参数溢出空间中。
2. 保存必要的的 Caller-saved 临时寄存器到栈上，以防被调用者覆盖。例如你用到了 `t0` `t1`，则需要将这两个寄存器的值保存到栈上。
3. 跳转到被调用函数的入口地址。
4. 被调用函数执行完毕后，从 `a0` 中获取返回值，恢复之前保存的临时寄存器。

被调用者则需要完成以下任务：

1. 创建自己的栈帧：
   1. 调整栈指针 `sp`，为自己的栈帧分配空间。
   2. 将 `ra` 和必要的 Callee-saved 寄存器保存到栈上。例如，如果你在函数中只用了 `s0/fp` `s1`，则只需要将这两个寄存器的值保存到栈上。
   3. 将帧指针 `fp` 设置为当前栈顶，用于访问局部变量和保存的寄存器。当然，如果当前函数栈帧较小，用不到 `fp`，也可以不设置。
2. 从参数寄存器 `a0-a7` 中读取参数。如果参数数量超过了 8 个，多余的参数需要使用 `fp` 从前一个栈帧的参数溢出空间中读取。
3. 执行函数体， 完成函数逻辑。
4. 返回结果到 `a0` 中。
5. 销毁当前的栈帧：
   1. 恢复 `ra` 和 Callee-saved 寄存器。
   2. 调整栈指针 `sp`，释放当前栈帧的空间。
   3. 返回到调用者。

以下是一个简单的例子，展示了完整的栈帧的创建和销毁：

```asm
# add(int x, int y) -> return x + y
f:
    # 保存返回地址和帧指针
    addi sp, sp, -16        # 为栈帧分配空间
    sw ra, 12(sp)           # 保存返回地址到栈
    sw fp, 8(sp)            # 保存帧指针到栈
    addi fp, sp, 16         # 设置帧指针
# 函数体（操作局部变量、参数等）
add a0, a0, a1          # 计算 x + y

# 恢复帧指针和返回地址
lw ra, 12(sp)           # 恢复返回地址
lw fp, 8(sp)            # 恢复帧指针
addi sp, sp, 16         # 释放栈帧

ret                     # 返回调用者
```

??? tip "栈帧管理与函数调用中可能存在的优化"

```
显然，在上面这个例子中，可以完全不分配栈帧，直接做以下操作：
f:
    add a0, a0, a1
    ret甚至，你可以将它直接内嵌到原先的函数中。例如原来的函数是这样的：main:
    li a0, 1
    li a1, 2
    call f
    ret现在可以直接优化为：main:
    li a0, 1
    li a1, 2
    add a0, a0, a1
    ret
```

!!! note "当然，你也可以不遵守这套函数调用的栈帧管理约定，只要你的代码能够正确运行即可。"

??? note "模板代码中的栈帧管理"

```
对于像 `DEC` （Accipit IR 中为 `alloca`）这样的指令，我们需要在栈上为其分配空间，因此，我们可以在 `Function` 类中添加若干方法来维护函数的栈帧：
class Function {
    int temp_stack_size = 0;      // 临时变量栈帧大小
    int reg_stack_size = 0;       // 寄存器栈帧大小
    int alloc_temp(int size = 4); // 为临时变量等分配栈帧内低地址空间，返回 sp offset
    int alloc_reg(int size = 4);  // 为寄存器等分配栈帧内高地址空间，返回 fp offset
    // ... 其他成员变量和函数 ... //
};当然，如果你有需要，你也可以存一个偏移量映射表来记录每个变量在栈上的偏移量，例如一个参考的实现如下：class Function {
    int temp_stack_size = 0;      // 临时变量栈帧大小
    int reg_stack_size = 0;       // 寄存器栈帧大小
    std::map&lt;Reg, int&gt; temp_offset; // 临时变量偏移量映射表
    std::map&lt;Reg, int&gt; reg_offset;  // 寄存器偏移量映射表
    int alloc_temp(int size = 4, Reg reg); // 为临时变量等分配栈帧内低地址空间，返回 sp offset
    int alloc_reg(int size = 4, Reg reg);  // 为寄存器等分配栈帧内高地址空间，返回 fp offset
    // ... 其他成员变量和函数 ... //
};在指令选择和寄存器分配过程中，你可以使用 alloc_temp 和 alloc_reg 方法来增大函数的栈空间并获取分配到的空间的 sp/fp offset。这里 temp 和 reg 的函数命名可能会让你觉得有些疑惑。你可以当作 alloc_sp 和 alloc_fp 来理解，即一个用于分配靠近 sp 的栈空间（通常为变量使用的栈空间），一个用于分配靠近 fp 的栈空间（通常为寄存器使用的栈空间）。将两个函数分开是因为我们在分配空间时并不知道总的栈空间大小，但又需要将一部分内容储存在靠近 sp 的栈空间、另一部分内容储存在靠近 fp 的栈空间。
```

## 执行汇编代码

本实验提供了两种运行环境供你测试和调试：**Venus Simulator** 和 **qemu-riscv32 Emulator**。两者各有优势，可以根据需求选择适合的工具。

- [Venus](https://github.com/ThaumicMekanism/venus) 是一个 RISC-V 模拟器，支持 RV32M 指令集，即在基础的 RV32I 指令集加上乘法，除法和取模指令。Venus 有一个简单的 GUI，可以方便地进行输入输出交互、查看寄存器和内存的值，但需要你自行使用系统调用来处理输入输出和结束程序。
- [QEMU](https://www.qemu.org/) 是一个通用的模拟器，支持多种指令集，包括 RISC-V。我们使用的 qemu-riscv32 是集成了 RISC-V 32 用户态环境的模拟器，它的优点在于可以模拟更加真实的硬件环境，但需要使用 GDB 进行调试，如果你对 GDB 不熟悉，可能会有一些困难。

!!! tip "[Bonus 3: 编译优化](../bonus/optimization.md) 实验会要求你使用 QEMU 进行测试，建议想要做 Bonus 3 的同学选择使用 QEMU 来模拟运行。"

在开始介绍这两个模拟器之前，我们先来看一下 GCC 是怎么编译一个简单的 C 程序的：

1. \*\*预处理（Preprocessing）：\*\*GCC 使用预处理器（cpp）处理源文件，完成宏替换、头文件包含、条件编译等操作。例如使用 `gcc -E main.c -o main.i` 输出的文件 `main.i` 就是预处理后的文件。
2. \*\*编译（Compilation）：\*\*预处理后的代码通过编译器前端转换为汇编代码。这个过程解析 C 代码语法并生成目标架构的汇编代码，这也是我们的编译器需要做到的内容。例如使用 `gcc -S main.i -o main.s` 输出的文件 `main.s` 就是编译后的汇编代码。
3. \*\*汇编（Assembly）：\*\*GCC 调用汇编器（as）将汇编代码转换为目标文件（object file）。目标文件包含二进制的机器指令，例如使用 `gcc -c main.s -o main.o`，输出的文件 `main.o` 是编译后的中间目标文件，包含程序逻辑的机器代码，但还未链接。
4. \*\*链接（Linking）：\*\*GCC 调用链接器（ld）将目标文件和必要的库（如 C 标准库 libc）链接为可执行文件。例如使用 `gcc main.o -o main` 输出的文件 `main` 就是可执行文件。在这一步，GCC 默认参数下会自动链接的一些启动文件（通常是 `.o` 文件），例如：`crt1.o` （C runtime）、`crti.o` 和 `crtn.o`（初始化和终止部分的代码）。这些文件定义了程序启动和退出的流程，最终会调用 `main` 函数。在这其中，定义了程序的入口点为 `_start`，在其中会完成像设置全局指针 `gp`、清零 `.bss` 段等初始化操作，并调用 `main` 函数，在 `main` 函数返回后调用 `exit` 函数结束程序。

??? question "如果你好奇为什么不用设置栈指针" 如果你对 OS 实验有印象，应该记得我们在创建一个新的进程时，会自动为其创建一个位于高地址的用户栈，并将栈指针 `sp` 指向栈顶。在 Venus 和 QEMU 中也是如此，系统会自动为我们创建一个栈并设置栈指针 `sp`，我们直接使用 `sp` 即可。

### Venus 模拟器

Venus 支持一些基础的环境调用，我们可以方便进行输入输出交互。如果我们想要调用一个环境调用，将 `ID` 加载到寄存器 `a0` 中，将所需要的参数加载到 `a1` 至 `a7` 中，然后执行 `ecall` 指令即可。返回值都同样存储在这些寄存器中。Venus 支持下列环境调用:

| Syscall ID (`a0`) | 名称 | 描述 |
| --- | --- | --- |
| 1 | print_int | 打印 `a1` 中的整数值 |
| 4 | print_string | 打印以 `a1` 为起始地址的空字符结尾的字符串 |
| 6 | read_int | 读取一个整数值到 `a0` 中 |
| 9 | sbrk | 分配 `a1` 字节的内存，并返回分配的内存的起始地址给 `a0` |
| 10 | exit | 结束程序 |
| 11 | print_character | 打印 `a1` 中的字符值 |
| 17 | exit2 | 结束程序，以 `a1` 中的值作为返回值 |
| ... | ... | ... |

!!! note "`sbrk` 这一调用与 `malloc` 相似，和我们中间代码中为局部变量分配空间的语句并没有什么关系。本实验的所有局部变量都是在栈上分配的，你不应该调用 `sbrk`。"

<!-- 由于 Venus 在读入数据段后会从代码段的第一条指令开始执行，并且不会像 GCC 一样为我们准备一些启动文件，因此，我们可以像 GCC 那样设置一个 `_start` 函数，将其放在代码段的开头作为程序的入口点，之后调用 `main` 函数。像 `read` `write` 这两个特殊的函数，我们也可以像链接其他文件一样，嵌入到我们最终生成的代码中。这样，在我们的编译器将源代码翻译为中间代码时，就不需要关注这些细节了。 -->

Venus 在执行汇编时，会寻找声明为 `.globl` 的 `main` 函数作为程序的入口点。因此，在你生成的汇编中，最好对每个函数都声明为 `.globl`，以便 Venus 正确执行。像 `read` `write` 这两个特殊的函数，我们也可以像 gcc 链接库文件一样，嵌入到我们最终生成的代码中。这样，在我们的编译器将源代码翻译为中间代码时，就不需要关注这些细节了。

??? example "针对 Venus 的汇编代码示例"

```
例如，对于下面这个代码：
int array[4] = {1,2,3,4};
int a = 1;
int main () {
    array[2] = 5;
    a = 2;
    write(array[2]);
    return 0;
}我们可以生成下面的汇编代码：    .data
array:
    .word   1
    .word   2
    .word   3
    .word   4
a:
    .word   1

    .text
# 手动添加 read 和 write 函数
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
# 编译出的 .text 接在这后面
    .globl main
main:
    addi    sp,sp,-4
    sw      ra,0(sp)
    # array[2] = 5
    la      a5,array
    li      a4,5
    sw      a4,8(a5)
    # a = 2
    la      a5,a
    li      a4,2
    sw      a4,0(a5)
    # write(array[2])
    la      a0,array
    addi    a0,a0,8
    lw      a0,0(a0)
    call write
    # write(a)
    la      a0,a
    lw      a0,0(a0)
    call write
    # return 0
    li a0, 0
    lw      ra,0(sp)
    addi    sp,sp,4
    ret
```

你可以使用在线的 [Venus](https://venus.cs61c.org/) 来调试你的程序。在 VSCode 中你也可以安装[相应插件](https://marketplace.visualstudio.com/items?itemName=hm.riscv-venus)来方便地调试你的程序。

但是请注意，\*\*一切测试结果都以我们提供的 jar 包为准。上述调试工具并不包含 `read_int` 的环境调用。\*\*我们魔改了 Venus（见[这里](https://git.zju.edu.cn/compiler/venus)），使得它支持了 `read_int` 环境调用，所以你可以在我们提供的 jar 包中使用 `read_int` 来读取输入。

### QEMU 模拟器

我们使用的 qemu-riscv32 已集成了一个用户态环境，这意味着可以通过 Linux RV32 的系统调用直接实现输入、输出等功能。利用 `ecall` 指令，可以方便地调用系统调用来完成这些任务。

如果你对上学期 OS 课程还有印象，应该知道 Linux 的系统调用 `sys_read` `sys_write` 都只能输出一段字符串，而不能直接输出一个整数。这要求我们在输出整数前，先将其转换为字符串。不过，在本实验中，为了简化操作，已为你提供了一个实现了 `read` 和 `write` 函数的 IO 文件（`sp26-tests/libs/io.c`）。你可以直接使用这些函数，无需额外编写代码。在汇编代码中，只需使用 `call read` 和 `call write` 即可调用这些函数，链接器会在编译时自动将 IO 文件中的函数链接到你的程序中。

除了 IO 文件外，riscv32-unknown-elf-gcc 和其他版本的 gcc 一样，会默认包含 `crt0.o` 这个标准库文件，这个文件会在程序开始时初始化一些全局变量，然后调用 `main` 函数。所以你的编译器生成的汇编代码应该和你往常写的 C 代码一样，从 `main` 函数开始，将 `main` 函数当做一个普通的函数来处理即可。

??? example "针对 RISC-V 32 工具链的汇编代码示例"

```
例如，对于下面这个代码：
int array[4] = {1,2,3,4};
int a = 1;
int main () {
    array[2] = 5;
    a = 2;
    write(array[2]);
    return 0;
}我们可以生成下面的汇编代码：    .data
array:
    .word   1
    .word   2
    .word   3
    .word   4
a:
    .word   1

    .text
    .globl main
main:
    addi    sp,sp,-4
    sw      ra,0(sp)
    # array[2] = 5
    la      a5,array
    li      a4,5
    sw      a4,8(a5)
    # a = 2
    la      a5,a
    li      a4,2
    sw      a4,0(a5)
    # write(array[2])
    la      a0,array
    addi    a0,a0,8
    lw      a0,0(a0)
    call write
    # write(a)
    la      a0,a
    lw      a0,0(a0)
    call write
    # return 0
    li a0, 0
    lw      ra,0(sp)
    addi    sp,sp,4
    ret
```

更多关于 RISC-V 工具链的安装和使用，请参考 [附录 E: RV32 工具链使用指南](../appendix/rv32_toolchain.md)。其中包含了工具链的安装步骤以及相关使用方法。此外，还提供了扩展内容，例如如何在编译时不使用默认的启动文件，自行实现 `_start` 函数等。

??? note "模板代码中的汇编代码生成" 最后，需要实现一个 `AsmEmitter` 类，用于将 ASM 指令转换为汇编代码并输出到文件中。这一部分也比较简单。实验框架已经帮你适配了 Venus 和 RV32 工具链 两种执行方式。

```
你需要在生成函数对应的汇编代码时，根据 `Function` 内存储的栈帧信息，完成 prologue 和 epilogue 的生成。对于每一条指令，利用寄存器分配后得到的 `RegMap`，将虚拟寄存器替换为物理寄存器。最后，将生成的汇编指令转化为字符串。
```

## 你的任务

你的任务就是在实验 3 中间代码的基础上，解决我们上面提到的三个问题，生成能够在 Venus 或 QEMU 模拟器上运行的 RISC-V 32 汇编代码。

例如，对于下面的代码：

```c
int factorial(int n) {
    int result = 1;
    while (n > 0) {
        result *= n;
        n -= 1;
    }
    return result;
}

int main() {
    int n;
    n = read();
    int result = factorial(n);
    write(result);
    return 0;
}
```

你可以生成如下的汇编代码：

=== "针对 RV32 工具链的编译器"

````
```asm linenums="1"
    .section .text
    .globl factorial
factorial:
    mv t0, a0 # Store loop counter in t0
    li t1, 1 # Store result in t1
factorial_loop:
    mul t1, t1, t0
    addi t0, t0, -1
    beqz t0, factorial_end
    j factorial_loop
factorial_end:
    mv a0, t1
    ret
.globl mainmain:
addi    sp,sp,-4
sw      ra,0(sp)  # Save return address
# Read Input
call read
call factorial
# Print Result
call write
# Exit
li a0, 0
lw      ra,0(sp)  # Restore return address
addi    sp,sp,4
ret
接下来，使用 riscv32-unknown-elf-gcc，配合我们提供的包含了 `read` `write` 函数的 IO 文件进行编译：

```console
$ riscv32-unknown-elf-gcc output.S sp26-tests/libs/io.c -o output最后使用 qemu-riscv32 来运行：$ qemu-riscv32 output你的编译器 compiler 必须支持两个命令行参数的情形，即：$ ./compiler &lt;input_file&gt; &lt;output_file&gt;该程序必须接受一个输入的源代码文件名作为参数，并且将输出的汇编代码写入到指定的 output_file 文件中，我们只会使用这种方式来测试你的编译器。在提交的时候，你需要将你的编译器放到 lab4 分支中，并且在 config.toml 中修改以下选项：use_qemu = true
````

=== "针对 Venus 模拟汇编的编译器"

````
```asm linenums="1"
    .text
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
    .globl factorial
factorial:
    mv t0, a0 # Store loop counter in t0
    li t1, 1 # Store result in t1
factorial_loop:
    mul t1, t1, t0
    addi t0, t0, -1
    beqz t0, factorial_end
    j factorial_loop
factorial_end:
    mv a0, t1
    ret
.globl mainmain:
addi    sp,sp,-4
sw      ra,0(sp)  # Save return address
# Read Input
call read
call factorial
# Print Result
call write
# Exit
li a0, 0
lw      ra,0(sp)  # Restore return address
addi    sp,sp,4
ret
通过我们提供的 jar 包，你可以直接运行上面的汇编代码：

```console
$ java -jar sp26-tests/venus.jar output.s
5
120你的编译器 compiler 必须支持三个命令行参数的情形，即：$ ./compiler &lt;input_file&gt; &lt;output_file&gt; --venus
````

!!! tip "在实践中学习" 如果你不知道生成的汇编可以是什么样的，我们推荐 [Compiler Explorer](https://godbolt.org/)，其中 RISC-V rv32gc clang (trunk) 版本最为相近。你可以输入一些简单的 C 代码，然后查看对应的 RISC-V 汇编代码。但要注意我们的 Venus 模拟器并不总是支持这个网站上的所有指令。

## 测试

运行以下命令来测试你的编译器：

```console
$ python3 sp26-tests/test.py lab4 .
```

我们只设置了正确性测试，没有性能测试。所以你不需要考虑生成的代码的效率问题，只需要保证生成的代码能够正确运行即可。

!!! tip "测试你在 Lab 0 中编写的程序"

```
恭喜你已经完成了一个简单的编译器的主要流程，它可以将一段源代码转化为 RISC-V 32 的汇编代码，这真不是一件容易的事。
在好好休息一下之前，请用你在实验 0 中编写的测试用例测试你的编译器，看看你的编译器是否能正确编译你编写的程序：
$ python3 sp26-tests/test.py lab4 . -f appends/lab0
```

## 实验提交

你需要将作为 Lab 4 评分的代码放到 `lab4` 分支中，并提交一个实验报告，**不得超过 3 页**，内容包括:

- **实现思路**：描述你完成实验的思路和方法，重点描述你是如何实现的。请不要照搬实验指导。
- **难点与亮点**：你在实现过程中遇到的难点？说说你是如何解决这些难点的；你实现中的亮点？重点描述你的实现中的亮点，你认为最个性化、最具独创性的内容，避免大段地向报告里贴代码。
- **心得体会（20%）**：做完这次实验后，有什么心得体会？说说你完成实验后的真实感悟、避坑指南或吐槽。
- **附加内容（不计入页数）**：
  - **建议（可选）**：你对本次实验有什么建议和想法？你可以提出对实验设计的建议，指出实验指导里描述含糊的地方，或者是对实验的任何想法。
  - **借鉴声明**：如果你复用借鉴了参考代码或其他资源，请明确写出你借鉴了哪些内容。如果你没有上述情况，也请在报告中明确声明。**即使你声明了代码借鉴，你也需要自己独立认真完成实验。**
  - **AI 工具使用记录**：如果你使用了 AI 工具辅助实验，请在报告中明确指出使用了什么工具、如何使用。例如：
    - 问答类（ChatGPT 等）：附上你与 AI 工具的对话截图或文字记录。
    - Tab Completion 类（如 Cursor Tab 等）：明确指出使用工具名称。
    - Agent 类（Claude Code 等）：请让 Agent 生成代码时一同生成 update notes，并在提交代码库保留所有 Agent 生成的辅助文档（例如 `AGENT.md`），在报告中附上相关截图或文字记录。
    - 上述 AI 工具使用的完整记录可存放于仓库的 `reports/appends/lab4` 文件夹中，并在实验报告中简要说明。

!!! danger "实验报告存在以下情况的，将由助教现场验收，并相应扣分" - 若实验报告中**缺乏实现思路**或**未真实描述 AI 工具使用情况**，将由助教现场验收，并相应扣分。 - 实验报告**禁止使用 AI**，包括但不仅限于直接生成报告内容、润色等。提交的报告将会使用 AI 检测工具进行检测。若 AI 生成的内容占比过高，或者检测出明显的 AI 生成痕迹，将由助教现场验收，并相应扣分。

我们只接受 PDF 格式的实验报告。你需要将报告放在仓库的 `reports/lab4.pdf` 中。