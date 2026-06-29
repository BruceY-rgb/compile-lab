# Zero IR

??? note "模板代码使用指南"

    如果你选用 Zero IR 作为中间代码，Lab 3 部分的模板代码位于仓库中的 `template/lab3-zero` 分支下，你可以将其合并到自己的 `lab3` 分支中：
    
    ```console
    $ git merge upstream/template/lab3-zero
    ```

    如果出现冲突，Git 会提示你解决冲突，手动编辑冲突的文件并在解决后执行：

    ```console
    $ git add <conflict-file>
    $ git commit
    ```

    合并完成后，你可以先把模板代码推送到远程仓库：

    ```console
    $ git push origin lab3-zero
    ```

    然后开始修改模板代码，完成你的编译器。
    
    Lab 3 框架的文件结构如下，高亮部分为新增和改动的文件：

    ```text hl_lines="2-6 11-14 17"
    src
    ├── analysis
    │   ├── cfg_builder.cpp     // 控制流图构建
    │   ├── cfg_builder.hpp
    │   ├── control_flow.cpp    // IR 层级结构定义
    │   └── control_flow.hpp
    ├── ast
    │   ├── tree.cpp
    │   └── tree.hpp
    ├── common.hpp
    ├── ir
    │   ├── ir.hpp              // IR 语句定义
    │   ├── ir_translator.cpp   // IR 翻译实现
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
    ```

    ??? note "OCaml 中的 Zero IR"

        OCaml 模板的 Zero IR 定义在 `ir/zero_ir.ml` 中，使用 ADT 表示指令：

        ```ocaml
        type instr =
          | Label of string | Goto of string
          | Assign of string * operand | Binary of string * operand * binop * operand
          | Li of string * int | Param of string | Arg of operand
          | Return of operand | Call of string option * string
          | Function of string | Global of string * int * int list
          (* TODO: 添加条件跳转、数组分配、指针操作等指令 *)
        ```

        模板已给出所有已定义指令的打印函数 `print_instr`。你新增指令变体时需要同时补充对应的打印 case。

        翻译函数在 `ir/translate_zero.ml` 中。模板给出了 `IntConst` 和 `BinaryExp(Add)` 的翻译作为示例，其余表达式和所有语句的翻译函数标注为 `TODO`。

!!! warning "使用最新的模板代码"
    学期初版本的模板代码存在一些 bug，因此强烈建议同学们确认自己基于的模板代码是最新的。实验模板在实验正式开始前都可能有更新，在正式开始后一般不再更新，如果有更新会钉钉通知大家。

## 中间代码定义

我们先看一个阶乘函数的例子，直观感受一下本实验要求实现的中间代码长什么样子：

```c linenums="1"
FUNCTION factorial:
    PARAM n                     // parameter n
    t1 = n
    t2 = #1
    IF t1 == t2 GOTO Label1     // if n == 1
    GOTO Label2
LABEL Label1:                   // return 1
    A0 = #1
    GOTO factorial.ret
LABEL Label2:
    t5 = n
    t7 = t5 - #1                // n - 1
    ARG t7
    t6 = CALL factorial         // factorial(n - 1)
    A0 = t5 * t6                // n * factorial(n - 1)
    GOTO factorial.ret
LABEL factorial.ret:
    RETURN A0

FUNCTION main:
    n = CALL read
    t10 = n
    ARG t10
    result = CALL factorial
    t12 = result
    ARG t12
    CALL write
    A0 = #0
LABEL main.ret:
    RETURN A0
```

这段代码基本上是自明的，在 `main` 函数中我们调用 `read` 和 `write` 来完成输入输出，调用 `factorial` 完成阶乘计算。而在 `factorial` 函数中，我们根据参数 `n` 的值来判断是否需要递归调用自身，最后返回结果。其中，`CALL`、`ARG` 和 `RETURN` 是函数调用相关的指令，`GOTO` 和 `LABEL` 是控制流相关的指令，其余则是表达式运算相关的指令。

中间代码的定义如下：

| 类型    | 格式                   | 说明                             |
|--------|-----------------------|----------------------------------|
| 数据流指令  | `x = #k`              | 将常量 `k` 加载到 `x` 中                    |
|        | `x = y`               | 将 `y` 的值存放到 `x` 中                    |
|        | `x = y binop z`       | 将 `y` 和 `z` 的双目运算结果存放到 `x` 中           |
|        | `x = y binop #k`      | 将 `y` 和常量 `k` 的双目运算结果存放到 `x` 中        |
|        | `x = unop y`          | 将 `y` 的单目运算结果存放到 `x` 中               |
|        | `x = *y`              | 将 `y` 指向的位置的值存放到 `x` 中 |
|        | `x = *(y + #k)`       | 将 `y` 加上 `k` 字节偏移量后指向的位置的值存放到 `x` 中 |
|        | `*x = y`              | 将 `y` 的值存放到 `x` 指向的位置中 |
|        | `*(x + #k) = y`       | 将 `y` 的值存放到 `x` 加上 `k` 字节偏移量后指向的位置中 |
| 控制流指令  | `LABEL l:`            | 定义标签`l`                          |
|        | `GOTO L`              | 无条件跳转到`L`处                       |
|        | `IF x relop y GOTO L` | 如果 `x` 和`y`的运算结果满足`op`的关系，则跳转到`L`处 |
| 函数相关指令 | `FUNCTION f:`         | 定义函数 `f`                          |
|        | `x = CALL f`          | 调用函数 `f`，并将返回值存放到 `x` 中             |
|        | `CALL f`              | 调用函数 `f`，但不保存返回值                  |
|        | `PARAM x`             | 函数参数声明                           |
|        | `ARG x`               | 将 `x` 作为参数传递给函数                    |
|        | `RETURN x`            | 退出当前函数并返回 `x` 的值                   |
|        | `RETURN`              | 退出当前函数并返回`void`的值                |
|        | `DEC x #k`            | 声明 `k` 个字节的空间，并将首地址存放到 `x` 中      |
| 全局变量相关指令 | `GLOBAL x: #k` | 声明 `k` 个字节的全局变量 `x`，并全部初始化为 0 |
|        | `GLOBAL x: #k = #v1, #v2, ...` | 声明 `k` 个字节的全局变量 `x`，并初始化为常量 `v1`, `v2`, ...，其中等号后的值的个数应该为 `k // 4` |
|               | `x = &label` | 将全局变量 `label` 的地址存放到 `x` 中 |

我们支持的 `binop` 双目运算，`relop` 关系运算和 `unop` 单目运算如下表所示：

| 双目运算符 | 说明 | 关系运算符 | 说明   | 单目运算符 | 说明          |
|-------|----|-------|------|-------|-------------|
| `+`     | 加法 | `>`     | 大于   | `+`     | 正           |
| `-`     | 减法 | `>=`    | 大于等于 | `-`     | 取负          |
| `*`     | 乘法 | `<`     | 小于   |      |  |
| `/`     | 除法 | `<=`    | 小于等于 |      |        |
| `%`     | 模   | `!=`    | 不等于  |       |             |
|         |     | `==`    | 等于   |       |             |

我们有如下说明：
<!-- 
- 中间代码是大小写敏感的，如 `GOTO` 和 `goto` 是不同的，所以请大写所有关键字。
- `x`、`y`、`z` 都是临时变量或者标识符，而 `#k` 则是整型常量。在中间代码中同一函数内不存在作用域的概念，需要避免重复命名不同的变量；不同函数内的变量相互独立。
- 变量声明语句 `DEC` 用于为一个函数体内的局部变量声明其所需要的空间，该空间的大小以**字节**为单位。这个语句是专门为数组变量这类需要开辟一段连续的内存空间所准备的。例如，如果我们需要声明一个长度为 10 的 int 类型数组 `a`，则可以写成 `DEC a #40`。这样，我们的解释器会为 `a` 分配 40 字节的空间，并将空间的**首地址**存放到 `a` 中。
- 生成的中间代码函数必须要有 `RETURN` 语句，即使函数返回值为空，也需要写成 `RETURN`，不能省略。
- 全局变量需要在函数定义前先进行定义。
- `PARAM` 语句在每个函数开头使用，对于函数中形参的数目和名称进行声明。例如，若一个函数 `func` 有两个形参 `a`、`b`，则该函数的函数体内前两条语句为：`PARAM a`、`PARAM b`。在调用一个函数之前，我们先使用 `ARG` 语句传入所有实参，随后使用 `CALL` 语句调用该函数并存储返回值。
- 解释器内置了两个函数来完成输入输出，即 `read` 和 `write`。`read()` 从控制台读取整数，例如 `x = CALL read`；`write(x)` 将整数 `x` 输出到控制台，例如 `ARG x` 和 `CALL write`。
- 支持 C-style 的注释，即 `//` 与 `/* ... */`。 -->

1. **语法规则**：
    - **大小写敏感**，关键字必须全大写（如 `GOTO` 和 `goto` 是不同的）。
    - **变量命名**：`x`、`y`、`z` 等表示临时变量（标识符）；`#k` 表示整型常量（如 `#40`）。
2. **函数内变量声明**：
    - **临时变量**：直接赋值定义后使用即可。非数组局部变量可直接用一个临时变量表示。
    - **栈上空间分配 `DEC`**：`DEC` 用于在一个函数的栈上声明一段连续的内存空间，以**字节**为单位。特别适用于数组等需要连续内存空间的变量。例如，若需声明一个长度为 10 的 int 类型数组 `a`，可以写成 `DEC a #40`，即为 `a` 分配 40 字节内存，并将该空间的**首地址**存入 `a` 中。
    - **变量作用域**：一个函数即为一个作用域，不同函数之间的变量相互独立。
3. **函数规范**：
    - **函数返回**：每个函数必须包含 `RETURN` 语句，即使函数返回值为空，也需要写成 `RETURN`，不能省略。
    - **参数声明**：使用 `PARAM` 语句在函数开头声明形参。
    - **函数调用**：使用 `ARG` 传递所有实参，并用 `CALL` 调用函数并接受返回值；`CALL` 调用后会将所有 `ARG` 清空。
4. **全局变量**：
    - **在函数定义前声明**：需要在所有函数相关的中间代码前完成对全局变量的声明与定义。
    - **自动赋零**：未给定初值的全局变量会自动赋值为全 0 数组。
5. **内置 I/O 函数**：
    - **`read()`**：从控制台读取整数并返回。例如 `x = CALL read`。
    - **`write(x)`**：将整数 `x` 输出到控制台。例如 `ARG x` `CALL write`。
6. **注释**：
    - **单行注释**：`//`。
    - **多行注释**：`/* ... */`。

!!! example "一些帮助你快速理解的例子"
    === "变量赋值"
        ```c
        x = 5
        y = x * 2 + 3
        ```
        对应的中间代码可能是：
        ```c
        // x = 5
        x = #5
        // y = x * 2 + 3
        t1 = x * #2
        y = t1 + #3
        ```
    === "数组操作"
        ```c
        int a[5];
        a[2] = 1;
        a[i] = 2;
        ```
        对应的中间代码可能是：
        ```c
        // int a[5];
        DEC a #20
        // a[2] = 1
        t0 = #1
        *(a + #8) = t0
        // a[i] = 2
        t1 = i * #4
        t2 = a + t1
        t3 = #2
        *t2 = t3
        ```
    === "函数传参与调用"
        ```c
        int func(int a, int b) {
            return a + b;
        }
        int main() {
            // ...
            int z = func(x, y);
            // ...
        }
        ```
        对应的中间代码可能是：
        ```c
        FUNCTION func:
            PARAM a
            PARAM b
            t1 = a + b
            RETURN t1
        
        FUNCTION main:
            // ...
            ARG x
            ARG y
            z = CALL func
            // ...
        ```
    === "条件判断"
        ```c
        if (a > b) {
            b = 1;
        } else {
            b = 2;
        }
        ```
        对应的中间代码可能是：
        ```c
            IF a > b GOTO Label1
            GOTO Label2
        LABEL Label1:
            b = #1
            GOTO Label3
        LABEL Label2:
            b = #2
        LABEL Label3:
        ```
    === "全局变量"
        ```c
        int a = 1;
        int b[5];
        int main() {
            a = 2;
            // ...
        }
        ```
        对应的中间代码可能是：
        ```c
        GLOBAL a: #4 = #1
        GLOBAL b: #20
        FUNCTION main:
            t1 = &a
            t2 = #2
            *t1 = t2
            // ...
        ```

!!! note "在实验 3 中你不需要特殊处理 `main` 函数，即在 `main` 函数最后你同样只需要写 `RETURN` 语句即可。"

??? note "模板代码中的 IR 结构定义"

    与 AST 类似，我们在 `ir/ir.hpp` 中定义了 IR 节点的基类 `IR::Node`：

    ```cpp title="src/ir/ir.hpp"
    class Node;
    using NodePtr = std::shared_ptr<Node>;
    class Node {
    public:
    virtual std::string to_string() const = 0;
    virtual ~Node() = default;  // make the class polymorphic
    };
    ```

    其中，`to_string` 函数用于将 IR 节点转换为对应的 IR 代码字符串。

    我们已经帮你定义好了一些基本的 IR 节点，例如 `IR::Assign`、`IR::Call` 等等。你只需要参考已经实现的节点，添加其余的 IR 节点即可。


## 语法制导的代码生成

讨论完了中间代码的定义，下一步我们就要把经过语义检查和推断的语法树转换成中间代码。
从语法树生成中间代码并没有你想象的那么困难，和打印语法树一样，我们要做的就是遍历语法树的节点，然后根据节点的类型生成对应的中间代码。
其核心和语义分析类似，我们要实现一个 `translate_X` 函数，`X` 可以对应表达式，语句等等。

### 表达式代码生成

让我们从最简单的常量表达式开始，例如 `1`，我们可以生成如下的中间代码：

```asm
t1 = #1
```

这看起来非常简单，但是有一个显然的问题是这个临时变量 `t1` 是从哪里来的呢？表达式本身并不会给出这一信息。这启示我们在设计 `translate_exp` 函数时，需要传入一个参数，用于指定表达式的结果的存放位置。例如，对于`1`这个表达式，我们可以传入一个临时变量`t1`，然后生成上面的中间代码。所以，你可以这么定义`translate_exp`函数：

```python
translate_exp(exp, symbol_table, place)
```

其中，`exp` 是表达式节点，`symbol_table` 是符号表，`place` 是表达式的结果的存放位置。

为什么要符号表呢？答案是显然的，因为对于形如 `x` 的变量表达式，我们需要查阅符号表来获取该变量在中间表示下的名字。我们的中间表示并没有作用域的概念，因此对于那些在语法树中重复命名的变量，如在一个 `Block` 里定义的变量和外层的变量重名时，你需要自行处理。

而对形如 `exp1 + exp2` 这样的二元运算表达式，我们生成新的临时变量，递归调用二元运算两个子节点的 `translate_exp` 函数，最后生成代码将它们加起来，即:

```python
code1 = translate_exp(exp1, symbol_table, t1)
code2 = translate_exp(exp2, symbol_table, t2)
place = t1 + t2
```

同理，这个存储结果的位置 `t3` 也是由 `translate_exp` 函数的参数传入的。而`t1`和`t2`则是由内部生成的。

最后是函数调用表达式，我们只需要递归调用实参的 `translate_exp` 函数，然后生成代码将它们作为参数传递给函数，最后将函数的返回值存放到 `place` 即可。

我们可以将表达式的翻译规则总结如下：

![表达式生成](../../assets/image/zero-exp.svg#only-light)
![表达式生成](../../assets/image/zero-exp.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

!!! quote "该图来自[南京大学编译原理实验指导](https://cs.nju.edu.cn/changxu/2_compiler/projects/Project_3.pdf)，根据本实验我们略有修改，下同。"

!!! tip "显然存在的优化：合并指令"

    在学习虎书第 9 章中关于指令选择的内容时，大家接触到了一种名为 **Maximal Munch** 的策略。这是一种贪心策略，通过使用尽可能少的 tile 覆盖树形 IR 上尽可能多的节点。类似的策略同样适用于将 AST 转换为 IR。例如，对于表达式 `Exp1 + INT`，可以按以下规则生成代码：

    ```c
    t1 = new_temp()
    code = translate_exp(Exp1, symbol_table, t1)
    value = get_value(Exp2)
    return code + [place = t1 + #value]
    ```

    我们这里列出部分可能可以合并的形式，你可以尝试在实现中进行优化：

    - `Exp1 + INT`, `INT + Exp2`，可以合并为 `place = Exp1 + #k` 或者 `place = Exp2 + #k`
    - `Exp1 - INT`，可以合并为 `place = Exp1 - #(k)`
    - `ID[Exp + INT]`, `ID[INT]`，以及推广到多维数组 `ID[Exp1 + INT][Exp2 + INT]` 等，可以合并为一个 `*(x + #k) = y` 或者 `x = *(y + #k)` 指令

    当某个表达式的所有子表达式均为常量时，可以直接计算结果并生成相应的常量节点。这是一种静态常量表达式求值的优化。例如，对于 `a = (1 + 2) * 3`，可以直接生成 `a = #9`。具体实现时，若在 AST 中发现某个表达式节点，其所有子节点均为常量，则可计算其值并替换为常量节点。

    实际上，我们这里提到的这些优化都算局部常量传播。你可以参考 Bonus 3 的 [局部常量传播/折叠](../../bonus/optimization.md#_19) 部分，如果你完成了这个优化，可以在 Lab 4 完成后提交至 Bonus 3 中获得加分。

    一种更通用的方式是，按照表格中的通用方法先生成一个欠优化中间代码，再通过全局常量传播、常量折叠等优化手段，将其优化为更简洁的形式。这样你在翻译中间代码时就不需要考虑这些细节了。具体可以参见 [Bonus 3: 编译优化](../../bonus/optimization.md) 以及 [静态单赋值](../../appendix/ssa.md)。

### 语句代码生成

表达式生成的核心是 `translate_exp` 函数，而其中的参数 `place` 是表达式的结果的存放位置。那这个参数由谁来生成传递呢？
那就是语句的生成函数 `translate_stmt(stmt, symbol_table)`。

最简单的就是表达式语句，只需要直接翻译里面的表达式即可。

赋值语句的生成也很简单，分别生成左右两边的表达式，然后将右边的结果存放到左边的位置即可。

条件语句的生成则要复杂些，我们所定义的 `LABEL` 指令有了用武之地。直觉上来说，对于下面这种 `If` 语句：

```c
if (exp) {
    stmt1;
} else {
    stmt2;
}
```

应该生成如下的中间代码：

```c
    translate_cond(exp)
LABEL L1:
    translate_stmt(stmt1)
    GOTO L3
LABEL L2:
    translate_stmt(stmt2)
LABEL L3:
```

而理想中 `translate_cond` 应该在里面的条件为真时跳转到 `L1`，条件为假时跳转到 `L2`。而这也就是 `If` 语句的翻译流程：

- 生成新的标签 `L1`，`L2` 和 `L3`，分别用于条件为真时的跳转，条件为假时的跳转，以及条件语句结束后的跳转。
- 调用 `translate_cond` 生成条件表达式的中间代码，传入 `L1` 和 `L2` 作为条件为真时和条件为假时的跳转位置。
- 而对于具体的语句，只需要递归调用 `translate_stmt` 即可。
- 在上图一样在合适的位置中加上 `LABEL` 和 `GOTO` 语句，完成控制流的跳转。

其余类型的语句本质上是一样的，我们不再一一赘述。我们总结语句翻译的规则如下：

![语句生成](../../assets/image/zero-stmt.svg#only-light)
![语句生成](../../assets/image/zero-stmt.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

### 条件判断语句代码生成

你可能已经注意到了，在翻译语句的时候，我们专门为条件判断语句定义了一个函数 `translate_cond`，这是因为条件判断语句的翻译和普通语句有所不同，我们多传入了两个参数，`true_label` 和 `false_label`，分别表示条件为真时和条件为假时的跳转位置。

你可能觉得这和二元表达式没什么区别，只需要递归调用两边的表达式的 `translate_exp` 函数，然后生成代码将它们进行比较，最后根据结果跳转到对应的位置即可。对于一般的条件判断例如大于等于或者小于等于确实如此。

增加该函数复杂性的一大原因是 SysY 语言和 C 语言相同，是支持短路的。也就是说在翻译 `Exp1 && Exp2` 时，如果第一个表达式为假，那么第二个表达式就不需要计算了，直接跳转到 `false_label` 即可。而对于 `Exp1 || Exp2` 也是类似的。那么我们该如何实现这一点呢？答案还是控制流跳转。在这种情况，我们需要额外生成一个标签以供跳转，以 `Exp1 && Exp2` 为例，我们可以生成如下的中间代码：

```c
// 如果 Exp1 为假直接跳转到 false_label， 不进行 Exp2 的计算
translate_cond(Exp1, L1, false_label) 
LABEL L1:
    translate_cond(Exp2, true_label, false_label)
```

而条件判断如果不是关系运算，例如简单的 `if (x)`，只需要当成 `if (x != 0)` 处理。

我们总结条件判断翻译的规则如下：

![条件判断生成](../../assets/image/zero-cond.svg#only-light)
![条件判断生成](../../assets/image/zero-cond.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

!!! note "Undefined Behavior in SysY"
    在当前的 SysY 语言定义中，并没有说明 `<` `<=` 等关系运算符作为 `Exp` 表达式的计算结果，因此，可以暂不考虑形如 `a < b < c` 的关系运算，实验的测试用例也不包含这类情况。

    你可以自行完善你的编译器，像 C 语言一样来处理这种情况，即关系运算作为表达式节点时，若运算结果为真，则表达式计算结果为 `1`，否则为 `0`。

    如果你完善了你的编译器，以支持这一语法特性，可以提交至 [Bonus 2: 支持更多语法特性](../../bonus/extra.md)。

### 函数代码生成

相比于表达式和语句的生成，函数的代码生成更加结构化且相对简单。

在函数定义中，建议紧跟在 `FUNCTION` 关键字后声明参数。可以使用 `PARAM` 指令来依次定义所有函数参数。

在调用函数时，你需要使用 `ARG` 语句将所有实参传入，然后使用 `CALL` 语句调用该函数。建议你将 `ARG` 语句和 `CALL` 语句连续地放在一起。

对于函数体内的语句，我们按照前文中的规则循环生成即可。

### 全局变量和数组代码生成

上述代码生成中我们忽略了全局变量和数组相关的表达式求值和定义翻译。**这两种类型的变量实质上都是地址/指针**，它们的使用和赋值是相似的。

#### 定义

- 对于全局变量，我们需要使用 `GLOBAL` 指令，用于声明全局变量的大小和初始化。你可以假设全局变量的初始化一定是一个`INT_CONST`。例如，全局变量 `int a[4] = {1, 2, 3, 4}; int c;` 的中间代码可以翻译如下：

    ```c
    GLOBAL a: #16 = #1, #2, #3, #4
    GLOBAL c: #4    // or GLOBAL c: #4 = #0
    ```

    你会发现，对于全局变量 `c`，我们在实际使用时可以当作一个全局数组 `int c[1];` 来处理。

- 对于局部变量中的数组，我们需要使用 `DEC` 指令来声明数组的大小，其初始化过程可以视作对于数组元素的赋值。例如，局部数组 `int a[4] = {1, 2, 3, 4};` 的中间代码可以翻译如下：

    ```c
    DEC a #16
    T0 = #1
    *(a + #0) = T0
    T1 = #2
    *(a + #4) = T1
    T2 = #3
    *(a + #8) = T2
    T3 = #4
    *(a + #12) = T3
    ```

#### 使用与赋值

对于全局变量，我们首先需要使用 `x = &label` 来获取全局变量的地址。
在获得地址后，全局数组和局部变量中的数组使用是一样的，我们根据下标计算对应的偏移量，使用 `*(x + #k) = y` 和 `x = *(y + #k)` 来完成对应元素的赋值和取值。比如，同样是 `place = a[1]`，如果 `a` 是全局数组，我们可以这样翻译：

```c
t1 = &a
place = *(t1 + #4)
```

而对于局部数组，因为 `a` 已经是数组的起始地址，则可以省去 `t1 = &a` 这一步，直接使用：

```c
place = *(a + #4)
```

它们的具体生成规则就留给你作为练习啦 :-)

!!! info "根据 SysY 语言的标准，未显式初始化的全局变量，其值应当初始化为 0。"

??? note "模板代码中的中间代码生成"

    与 `TypeChecker` 类似，我们在 `ir/ir_translator.hpp` 中定义了 `IRTranslator` 类，用于遍历 AST 并生成 IR。在 `main` 函数中，在完成类型检查之后，调用 `IRTranslator` 的 `translate` 函数即可生成 IR。

    `IRTranslator` 类的定义如下：

    ```cpp title="src/ir/ir_translator.hpp"
    class IRTranslator {
    public:
    IR::Code translate(AST::NodePtr node);
    IR::Code translateExp(AST::NodePtr node, const std::string &place = "");

    private:
    IR::Code translateCompUnit(AST::CompUnitPtr node);
    // ... 对每种 AST 节点类型定义一个 translate 函数 ... //
    IR::Code translateLVal(AST::LValPtr node,
                            const std::string &place = "");
    // ... 对每种 AST 的表达式节点类型定义一个 translate 函数 ... //
    std::string new_temp();
    };
    ```

    其中：

    - `translate` 函数会根据 AST 节点的类型调用对应的 `translate` 函数，生成 IR。该函数的实现与 `TypeChecker::check` 相近，这里不再重复介绍。
    - `translateExp` 函数用于生成表达式的 IR。
    - `new_temp` 函数用于生成新的临时变量。

    为了支持条件表达式的翻译，你可以参考文档中的 `translate_cond` 部分，设计一个 `translateCond` 函数来根据节点类型调用对应的 `translate` 函数生成条件表达式的 IR。

    我们只需要实现每个 AST 节点类型对应的 `translate` 函数，即可完成 IR 的生成。例如，对于表达式生成中遇到的 `IntConst` 节点，我们可以这样生成 IR：

    ```cpp title="src/ir/ir_translator.cpp"
    IR::Code IRTranslator::translateIntConst(AST::IntConstPtr node,
                                            const std::string &place) {
    IR::Code ir;
    // 添加赋值常量指令
    if (!place.empty()) {
        ir.push_back(IR::LoadImm::create(place, node->value));
    }
    return ir;
    }
    ```

    !!! note "通常对于 IR 的代码，我们不需要对其随机访问，但是在优化过程中往往需要频繁地插入和删除节点。因此，我们使用双向链表  `std::list` 来存储 IR 代码，这样可以在 O(1) 的时间复杂度内进行插入和删除操作。"

## 基本块划分

在完成上述的中间代码生成后，我们会得到一系列的中间代码。为了便于后续的优化和目标代码生成，我们需要将这些中间代码转换为一种更加结构化的形式。可以将程序按以下结构进行划分：

1. **Basic Block（基本块）**：一个基本块是一个连续的中间代码序列，其中没有分支指令。基本块的第一个指令为 `LABEL`（对于函数的入口基本块，第一个指令为 `FUNCTION`），表示基本块的入口；最后一个指令通常是 `GOTO` 或 `IF`，表示基本块的出口。在基本块内部，不允许出现其他的 `LABEL`、`GOTO` 或 `IF` 指令，因此基本块内的指令序列是线性执行的。通过分析每个基本块最后的 `GOTO` 或 `IF` 指令，我们可以将这些基本块连接起来，形成**控制流图（Control Flow Graph，CFG）**。
2. **Function（函数）**：一个函数由若干个基本块构成，包含函数的信息以及表示函数体的基本块序列。实际上，函数就是由多个基本块组成的一个控制流图。
3. **Module（模块）**：一个模块表示整个编译单元，包含多个函数以及全局变量等。

关于控制流和基本块的详细内容及例子，请参考 Bonus 3 文档控制流分析中的 [基本块划分](../../bonus/optimization.md#_2) 部分。

在函数的返回时，需要考虑到目标代码生成中的栈清理、恢复 Callee-saved 寄存器等操作。因此，我们在这一步可以对控制流进行一些简单的优化，使得一个函数内只有一个出口基本块，即 `RETURN` 语句所在的基本块，这样可以简化目标代码生成的过程。具体来说，我们可以给函数添加一个新的出口基本块 `<func_name>.ret`，并且：

- 对于没有返回值的函数，我们可以将所有 `RETURN` 替换为 `GOTO <func_name>.ret`，并在出口基本块中生成 `RETURN` 语句。
- 对于有返回值的函数，我们可以定义一个统一的临时变量（例如 `A0`）来存储返回值，将 `RETURN T1` 替换为 `A0 = T1` `GOTO <func_name>.ret`，并在出口基块中生成 `RETURN A0` 语句。

!!! tip "如果你不想实现基本块划分以利于后续优化，可以只划分到函数级别。你也可以把这部分内容留到 Lab 4 再完成。"

??? note "模板代码中的基本块划分"

    在 `analysis/control_flow.hpp` 中，我们定义了 `Module`、`Function` 和 `BasicBlock` 类，用于表示模块、函数和基本块。为了支持全局变量，你需要在 `Module` 类中添加一个成员变量来存储全局变量的信息。

    我们使用 `CFGBuilder` 类来将线性存储的 IR 代码划分为模块、函数、基本块层级的形式。`CFGBuilder` 类定义在 `analysis/cfg_builder.hpp` 中：

    ```cpp title="src/analysis/cfg_builder.hpp"
    class CFGBuilder {
    public:
    Module build(IR::Code code);

    private:
    Function build_single_func(IR::Code code);
    std::string new_label();
    };
    ```

    你需要修改 `build` 函数以支持全局变量。而在 `build_single_func` 中，框架仅完成了合并 `RETURN` 语句的功能，且还未对函数进行基本块划分。你可以在 `build_single_func` 中实现基本块划分。

    如果你不想实现基本块划分以利于后续优化，可以仅做合并 `RETURN` 语句的功能，这样仅需要修改 `build` 函数以支持全局变量即可。此时，程序的结构实际上被划分为 Module -> Function -> (FuncBody, ExitBlock)。
    
    当然，你也可以把这部分内容留到 Lab 4 再完成，只需要修改 `main` 函数，将输出 IR 的步骤提前至进行基本块划分前即可。

## Zero IR Tools

### Interpreter

为了检测生成的中间代码生成正确性和评测，我们为大家提供了一个该中间表示的解释器，位于测试仓库的 `ir` 目录下。

!!! warning "尽管我们对于解释器的代码有过一些测试，但面对大家使用的实际情况，很难保证没有新的问题。如果你发现生成的中间代码不能被**正确**的执行，请及时联系助教。"

#### 使用方法

```console
$ python3 zero.py -h
usage: zero.py [-h] [-d] file

Interpret file generated by your compiler.

positional arguments:
  file         The IR file to interpret.

options:
  -h, --help   show this help message and exit
  -d, --debug  Whether to print debug info.
```

#### 常见报错与可能原因

- lark 报错，形如：

    ```text
    lark.exceptions.UnexpectedCharacters: No terminal matches 'a' in the current parser context, at line 1 col 5

    int a;
        ^
    Expected one of: 
            * EQUAL
    ```

    原因：产生的中间代码不符合我们要求的中间代码语法。大概率是输出的格式不对，比如缺少了 `:` 或者 `#` 等。

- `Variable {key} is not defined.`

    原因：正如报错所说，使用了未定义的变量。特别要注意的是，全局变量本质上是 label, 而不是变量，需要通过 `&` 来访问它的地址。

- `ValueError(f"address {address} not found in load")`

    原因：访问了未定义的地址，也就是我们在所有的通过 DEC 定义的合法数组地址范围内找不到 `address`，极大可能是你在使用 `*address = x` 的时候 `address` 并不是**地址**，而是**指向的值**。

#### 代码导读

虽然原则上你不需要理解我们提供的代码，但考虑到大家实际使用中各种调试需求，暂时修改虚拟机代码来帮助你调试是一个不错的选择。

你可以在源码中找到中间代码的形式语法，如下所示：

```text
start: instruction*

?instruction: "LABEL" NAME ":" -> label
    | "GOTO" NAME -> goto
    | "IF" NAME relop NAME "GOTO" NAME -> if
    | NAME "=" NAME -> assign
    | NAME "=" unop NAME -> unary
    | "*" "(" NAME "+" "#" SIGNED_INT ")" "=" NAME -> store
    | "*" NAME "=" NAME -> store
    | NAME "=" "*" "(" NAME "+" "#" SIGNED_INT ")" -> deref
    | NAME "=" "*" NAME -> deref
    | NAME "=" NAME binop NAME -> binary
    | NAME "=" NAME binop "#" SIGNED_INT -> binaryi
    | NAME "=" "#" SIGNED_INT -> li
    | "PARAM" NAME -> param
    | "ARG" NAME -> arg
    | "RETURN" NAME -> return
    | "RETURN" -> return
    | NAME "=" "CALL" NAME -> call
    | "CALL" NAME -> call
    | "FUNCTION" NAME ":" -> function
    | "DEC" NAME "#" SIGNED_INT -> dec
    | NAME "=" "&" NAME -> la
    | "GLOBAL" NAME ":" "#" SIGNED_INT ("=" "#" SIGNED_INT ("," "#" SIGNED_INT)*)? -> global
    | NAME "=" "PHI" "[" NAME "," NAME "]" ("," "[" NAME "," NAME "]")* -> phi

?relop : "<" -> lt
    | ">" -> gt
    | "<=" -> le
    | ">=" -> ge
    | "==" -> eq
    | "!=" -> ne
    
?binop: "+" -> add
    | "-" -> sub
    | "*" -> mul
    | "/" -> div
    | "%" -> rem
    | "<<" -> sll
    | ">>" -> sra
    | "&" -> and_
    | "|" -> or_
    | "^" -> xor

?unop : "-" -> neg
    | "+" -> pos

NAME: /[a-zA-Z\.\-_][a-zA-Z0-9\.\-_]*/ 
```

上述语法基本上与我们所介绍的中间代码形式基本一致，如果你想完全理解上述写法的全部细节，可以参考 [Lark](https://lark-parser.readthedocs.io/en/stable/) 相关文档。

在 `ir.py` 中，我们首先读取中间代码文件，parse 成一个一个 IR 语句，然后组装成一个个函数，最后执行 `main` 函数。

而一个函数的执行过程就是解释一条条语句的过程，如下所示：

```python
    def run(params: list[int] = []):
        # current pc
        pc = 0
        # current stack
        args = []
        while pc < len(codes):
            ir = codes[pc] # get the current ir
            pc += 1
            match ir:
                case Binary(dst, left, op, right):
                  ...
                case Li(label, value):
                    env[label] = value
                case Goto(label):
                    pc = labels[label]
                case Assign(dst, src):
                    env[dst] = env[src]
                case Return(src):
                    return env[src]
                case Arg(src):
                    args.append(env[src])
                case Param(dst):  # get the param from the stack
                    env[dst] = params.pop(0)
                case Call(dst, name):
                    env[dst] = env[name].new().run(args)
                    args = []  # reset args
                case If(left, op, right, label):
                    if op2func[op](env[left], env[right]) == 1:
                      pc = labels[label]
                case Dec(dst, size):
                    env[dst] = Array.new(size)
                case Store(dst, src):  # * x = y
                    env.store(dst, src)
                case Deref(dst, src):  # x = * y
                    value = env.load(src)
                    env[dst] = value
```

解释执行的过程是自明的，我们维护了一个 `env` 来保存当前的变量值，`pc` 来保存当前执行的指令位置，`args` 来保存当前函数的参数。而在调用函数时，我们会新建一个函数实例，然后将参数传递给它，最后执行它的 `run` 方法。

#### Debug 建议

在调试的过程中，你可以在 `zero.py` 中加入一些 `print` 语句来帮助你理解中间代码的执行过程。
但是由于我们的 `test.py` 会直接调用 `zero.py` 获得标准输出进行测试，所以请确保你的输出是干净的，不要有多余的输出。
所以你可以利用我们提供的 `-d` 参数来控制是否输出 debug 信息，因为 `test.py` 不会传递这一参数。

### Debugger

我们还提供了一个 VSCode 插件 [Zero IR Debugger](https://git.zju.edu.cn/compiler/zero-ir-debugger)，可以帮助你更方便地调试中间代码。

!!! warning "该插件仅供参考，我们最终评测仍然以上述解释器为准。如果你遇到任何问题，如输出的结果与解释器不一致，请及时联系助教。"

#### 使用方法

首先，从[这里](https://git.zju.edu.cn/-/project/3020/uploads/4c2ae29f75b0b5f478c09d11e5e24351/zero-ir-debugger-0.0.1.vsix)下载插件，在 VSCode 中选择扩展，点击扩展面板右上角的三个点，选择 **从 VSIX 安装**，选择下载的 VSIX 文件，安装插件。

然后，在 VSCode 工作区的 `.vscode` 文件夹下创建一个 `launch.json` 文件，内容如下：

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "type": "zeroir",
            "request": "launch",
            "name": "Zir: Debug Current File",
            "program": "${file}",
            "stopOnEntry": true,
        }
    ]
}
```

你也可以在 VSCode 打开 `launch.json` 文件时，点击右下角的 **添加配置**，Zero IR Debugger 提供了以下几种预设配置：

- `Zero IR Debug: Launch`：调试时选择文件
- `Zero IR Debug: Launch Current File`：调试当前打开的文件
- `Zero IR Debug: Launch Current File with specific input`：调试当前打开的文件，并在调试开始时选择输入文件
- `Zero IR Debug: Launch Specific File`：调试指定文件

你也可以自行配置设置，每个配置项的含义如下：

- `type`：必须为 `zeroir`
- `request`：必须为 `launch`
- `name`：配置名称
- `program`：要调试的文件路径
- `stopOnEntry`：是否在程序开始时停止
- `inputFile`：输入文件路径（为空时则从控制台输入）

最后，假如我们选择了调试当前文件的预设，只需要将生成的 Zero IR 保存到 `.zir` 后缀的文件中，并在 VSCode 中打开，然后按下 `F5` 开始调试，即可进入调试模式，像你熟悉的那样使用 VSCode 的调试功能（例如打断点、单步调试、查看变量等）。

除此之外，你也可以在打开一个中间代码 `.zir` 文件时，使用快捷键 ++ctrl+shift+p++（ macOS 为 ++cmd+shift+p++ ）打开命令面板，选择 `Zero IR Debug: Debug File` 来调试当前中间代码文件，或选择 `Zero IR Debug: Run File` 来执行当前中间代码文件。