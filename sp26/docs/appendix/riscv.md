# App. D: RISC-V Crash Course

本节是对 RISC-V 指令集和汇编的简单介绍，从下面参考资料中摘录了一些实验可能会用到的内容：

- [RISC-V 设计规范](https://riscv.org/technical/specifications/)
- [RISC-V Assembly Programmer's Manual](https://github.com/riscv-non-isa/riscv-asm-manual/blob/main/src/asm-manual.adoc)
- [RISC-V Unprivileged Spec](https://github.com/riscv/riscv-isa-manual/releases/download/20240411/unpriv-isa-asciidoc.pdf)

## 寄存器

RV32I 指令集包含了 32 个寄存器，分别命名为 `x0` 到 `x31`。程序计数器 `PC` 和这些寄存器是分开的。在实际中，开发者一般不使用这样的命名方式。虽然 `x1` 到 `x31` 在处理器看来都是通用寄存器，但是按照惯例，某些寄存器被用于特殊的任务：

| 寄存器    | ABI 名称  | 描述                | 保存者    |
| -         | -         | -                   | -         |
| `x0`      | `zero`    | 恒为 0              | N/A       |
| `x1`      | `ra`      | 返回地址            | 调用者    |
| `x2`      | `sp`      | 栈指针              | 被调用者  |
| `x3`      | `gp`      | 全局指针            | N/A       |
| `x4`      | `tp`      | 线程指针            | N/A       |
| `x5`      | `t0`      | 临时/备用链接寄存器 | 调用者    |
| `x6-7`    | `t1-2`    | 临时寄存器          | 调用者    |
| `x8`      | `s0`/`fp` | 保存寄存器/帧指针   | 被调用者  |
| `x9`      | `s1`      | 保存寄存器          | 被调用者  |
| `x10-11`  | `a0-1`    | 函数参数/返回值     | 调用者    |
| `x12-17`  | `a2-7`    | 函数参数            | 调用者    |
| `x18-27`  | `s2-11`   | 保存寄存器          | 被调用者  |
| `x28-31`  | `t3-6`    | 临时寄存器          | 调用者    |

作为一种约定，保存寄存器 `s0` 到 `s11` 在函数调用中会被保留，而参数寄存器 `a0` 到 `a7` 和临时寄存器 `t0` 到 `t6` 则不会。

## 指令一览

RV32I 所有指令和伪指令可以参考 [RISCV 设计规范](https://riscv.org/technical/specifications/)。实际上本节内容也大多来源于此。我们为你列出一些：

### 控制流

`beqz/bnez`
: 
    * 汇编格式：`beqz/bnez rs, label`。
    * 行为:如果 `rs` 寄存器的值等于（`beqz`）或不等于（`bnez`）0，则转移到目标 `label`。

`j`
: 
    * 汇编格式：`j label`。
    * 行为:无条件转移到目标 `label`。

`call/ret`
: 
    * 汇编格式：`call label`、`ret`。
    * 行为：`call` 指令会将后一条指令的地址存入 `ra` 寄存器，并无条件转移到目标 `label`。`ret` 指令会无条件转移到 `ra` 寄存器中保存的地址处。

### 访存类

`lw`
: 
    * 汇编格式：`lw rs, imm12(rd)`。
    * 行为：计算 `rd` 寄存器的值与 `imm12` 相加的结果作为访存地址，从内存中读取 32-bit 的数据，存入 `rs` 寄存器。

`sw`
: 
    * 汇编格式：`sw rs2, imm12(rs1)`。
    * 行为：计算 `rs1` 寄存器的值与 `imm12` 相加的结果作为访存地址，将 `rs2` 寄存器的值（32-bit）存入内存。

### 数据流

`add/addi`
: 
    * 汇编格式：`add rd, rs1, rs2`、`addi rd, rs1, imm12`。
    * 行为：计算 `rs1` 寄存器和 `rs2` 寄存器（`add`）或 `imm12`（`addi`）相加的值，存入 `rd` 寄存器。

`sub`
: 
    * 汇编格式：`sub rd, rs1, rs2`。
    * 行为：计算 `rs1` 寄存器和 `rs2` 寄存器相减的值，存入 `rd` 寄存器。

`slt/sgt`
: 
    * 汇编格式：`slt/sgt rd, rs1, rs2`。
    * 行为：判断 `rs1` 寄存器是否小于（`slt`）或大于（`sgt`）`rs2` 寄存器，如果判断条件成立，则将 1 写入 `rd` 寄存器，否则写入 0。

`seqz/snez`
: 
    * 汇编格式：`seqz/snez rd, rs`。
    * 行为：判断 `rs` 寄存器是否等于（`seqz`）或不等于（`snez`）0，如果判断条件成立，则将 1 写入 `rd` 寄存器，否则写入 0。

`xor/xori`
: 
    * 汇编格式：`xor rd, rs1, rs2`、`xori rd, rs1, imm12`。
    * 行为：计算 `rs1` 寄存器和 `rs2` 寄存器（`xor`）或 `imm12`（`xori`）按位异或的值，存入 `rd` 寄存器。

`or/ori`
: 
    * 汇编格式：`or rd, rs1, rs2`、`ori rd, rs1, imm12`。
    * 行为：计算 `rs1` 寄存器和 `rs2` 寄存器（`or`）或 `imm12`（`ori`）按位或的值，存入 `rd` 寄存器。

`and/andi`
: 
    * 汇编格式：`and rd, rs1, rs2`、`andi rd, rs1, imm12`。
    * 行为：计算 `rs1` 寄存器和 `rs2` 寄存器（`and`）或 `imm12`（`andi`）按位与的值，存入 `rd` 寄存器。

`sll/srl/sra`
: 
    * 汇编格式：`sll/srl/sra rd, rs1, rs2`。
    * 行为：对寄存器 `rs1` 进行逻辑左移（`sll`），逻辑右移（`srl`）或算数右移（`sra`）运算，移位的位数为 `rs2` 寄存器的值，结果存入 `rd` 寄存器。

`mul/div/rem`
: 
    * 汇编格式：`mul/div/rem rd, rs1, rs2`。
    * 行为：计算寄存器 `rs1` 和寄存器 `rs2` 相乘（`mul`），除以（`div`）或取余（`rem`）的值，存入 `rd` 寄存器。

`li`
: 
    * 汇编格式：`li rd, imm`。
    * 行为：将立即数 `imm` 加载到寄存器 `rd` 中。

`la`
: 
    * 汇编格式：`la rd, label`。
    * 行为：将标号 `label` 的绝对地址加载到寄存器 `rd` 中。

`mv`
: 
    * 汇编格式：`mv rd, rs`。
    * 行为：将寄存器 `rs` 的值复制到寄存器 `rd`。

下面的程序计算斐波那契数列的第 9 项并打印出来：

```asm
.data
n: .word 9

.text
main:   add     t0, x0, x0
        addi    t1, x0, 1
        la      t3, n
        lw      t3, 0(t3)
fib:    beq     t3, x0, finish
        add     t2, t1, t0
        mv      t0, t1
        mv      t1, t2
        addi    t3, t3, -1
        j       fib
finish: addi    a0, x0, 1
        addi    a1, t0, 0
        ecall # print integer ecall
        addi    a0, x0, 10
        ecall # terminate ecall
```

## Assembler Directive

Assembler directive 是一种用来告诉汇编器如何处理代码的指令。它们以 `.` 开头，通常出现在代码的开头。你可以参考 [RISC-V Assembly Programmer's Manual: Pseudo Ops](https://github.com/riscv-non-isa/riscv-asm-manual/blob/main/src/asm-manual.adoc#pseudo-ops)，此处摘录一些你可能会用到的的指令：

| Directive   | Arguments | Effect                                                               |
|-------------|-----------|----------------------------------------------------------------------|
| `.section`  | `[{.text,.data,.rodata,.bss}]` | emit section (if not present, default `.text`) and make current |
| `.text`     | | emit `.text` section (if not present) and make current |
| `.data`     | | emit `.data` section (if not present) and make current |
| `.byte`     | `expression [, expression]*` | store listed values as 8-bit bytes. |
| `.asciz`    | `"string"` | store subsequent string in the data segment and add null-terminator. (alias for `.string`) |
| `.word`     | `expression [, expression]*` | store listed values as unaligned 32-bit words. |
| `.zero`     | `integer` | store the given number of zero bytes. (Venus does not support this)  |
| `.globl`    | `symbol_name` | emit symbol_name to symbol table (scope GLOBAL) |


<!-- | Directive   | Effect                                                               |
|-------------|----------------------------------------------------------------------|
| `.section`  | Store subsequent items in the specified section.                     |
| `.data`     | Store subsequent items in the static segment                         |
| `.text`     | Store subsequent instructions in the text segment                    |
| `.byte`     | Store listed values as 8-bit bytes.                                  |
| `.asciiz`   | Store subsequent string in the data segment and add null-terminator. |
| `.word`     | Store listed values as unaligned 32-bit words.                       |
| `.zero`     | Store the given number of zero bytes. (Venus does not support this)  |
| `.globl` `.global`    | Makes the given label global.                                        | -->