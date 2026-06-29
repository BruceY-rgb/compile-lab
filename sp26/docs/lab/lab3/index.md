# Lab 3: 中间代码生成

!!! info "开始之前"
    请务必在 **2026-5-31 23:59** 前完成实验并推送到你自己对应的远程仓库的 `lab3` 分支中，否则将会按照评分标准扣分。

    建议你在开始前，先使用 `git checkout -b lab3 lab2` 从 `lab2` 分支创建一个新的 `lab3` 分支，并使用 `git push -u origin lab3` 在远程仓库中创建该分支，这样你就可以在实验过程中随时使用 `git push origin lab3` 提交代码，而不会误交到其他分支。

!!! abstract "[Checklist: 测试 Bug 排查](../lab0.md#bug-checklist)"

除了语法树，编译器里另一个核心数据结构就是中间代码（Intermediate Representation，IR）。
中间代码是编译器从源语言到目标语言之间采用的一种过渡性质的代码形式，往往介于语法树和汇编代码之间，其表示独立于机器，易于分析和翻译成汇编代码。
你可能会有疑问：为什么编译器不能把输入程序变成语法树，然后直接翻译成目标代码，而要额外引入中间代码生成呢?

答案当然是可以的。但中间代码的引入有两个好处。
一方面，中间代码将编译器自然地分为前端和后端两个部分。当我们需要改变编译器的源语言或目标语言时，如果采用了中间代码，我们只需要替换原编译器的前端或后端，而不需要重写整个编译器。
著名的 [LLVM](https://llvm.org/) 框架就是一个后端平台的例子，我们可以将 C++、Rust 或者自定义的其他语言统一转为 LLVM 中间代码，再由它负责优化和目标代码生成，这极大的简化了新的编程语言开发难度。
另一方面，中间代码既不像语法树那样抽象，也不像汇编代码那样底层，因此在某些分析和优化任务会更加方便。

在本实验中，我们提供了 Zero IR 和 Accipit IR 两种中间代码的定义，你可以自由选择其中一种来实现，将 Lab 2 中实现的语法树转换成对应的中间代码。这里我们对两种中间代码进行一个简要介绍：

- [Zero IR](zero.md) 是一个几乎 **Zero Gap to RISC-V** 的中间代码，它的设计简单直接，与 RISC-V 汇编代码的对应关系非常直观。缺点在于没有显式的类型信息，如果你想在 Bonus 2 中扩展更多类型（如 float），可能需要自己修改中间代码设计。
- [Accipit IR](accipit.md) 是一个类似 LLVM IR 的中间代码，采用静态单赋值（SSA）形式。它强大灵活，可以轻松地扩展更多类型，更贴近实际的现代编译器设计。缺点在于实现上稍微复杂点，看起来没那么直观。此外，由于所有变量默认存储在栈上，这一设计在编译优化上带来了利弊并存的影响。
 
!!! note "关于 Accipit IR 与编译优化"
    在 Lab 3 中，为了实现一个简化版的 SSA 而不依赖 `PHI` 函数，Accipit IR 选择将所有局部变量存储在栈上。然而，在未进行 mem2reg（将栈上的变量提升至寄存器）优化时，这种设计会影响某些优化的效果，例如寄存器分配、全局常量传播和死代码消除。毕竟，保存在栈上的局部变量无法直接参与这些优化。
    
    尽管如此，由于 Accipit IR 仍符合 SSA 形式，这些全局优化仍然适用，不过它们的效果仅限于中间代码中的临时变量，而无法作用于源代码中的原始局部变量。源代码中的原始变量才是优化的主要目标，因此，未进行 mem2reg 优化时，这些全局优化的效果相当于在局部范围内“弱化”。

    当然，你也可以在 Lab 3 中直接生成带有 `PHI` 函数的 Accipit IR，这相当于提前完成 mem2reg 优化。这样可以更好地支持后续的编译优化，但实现难度也会相应增加。具体实现方法可参考 [附录：静态单赋值 SSA](../../appendix/ssa.md)。如果你选择这种方式，可以在 Lab 4 完成后提交至 Bonus 3。

以下面这个阶乘函数为例：

```c linenums="1"
int factorial(int n) {
    if (n == 1) {
        return 1;
    } else {
        int ans = n * factorial(n - 1);
        return ans;
    }
}
```

两种 IR 的表示如下：

=== "Zero IR"

    ```c linenums="1"
    FUNCTION factorial:
        PARAM n                     // parameter n
        t1 = n
        t2 = #1
        IF t1 == t2 GOTO Label1     // if n == 1
        GOTO Label2
    LABEL Label1:                   // return 1
        A0 = #1
        GOTO factorial_ret
    LABEL Label2:
        t5 = n
        t7 = t5 - #1                // n - 1
        ARG t7
        t6 = CALL factorial         // factorial(n - 1)
        A0 = t5 * t6                // n * factorial(n - 1)
        GOTO factorial_ret
    LABEL factorial_ret:
        RETURN A0
    ```

=== "Accipit IR"

    ```c linenums="1"
    fn @factorial(#n: i32) -> i32 {
    %Lentry:
        // create a stack slot of i32 type as the space of the return value.
        // if n equals 1, store `1` to this address, i.e. `return 1`,
        // otherwise, do recursive call, i.e. return n * factorial(n - 1).
        let %ret.addr = alloca i32, 1
        // store function parameter on the stack.
        let %n.addr = alloca i32, 1
        let %4 = store #n, %n.addr
        // create a slot for local variable ans, uninitialized.
        let %ans.addr = alloca i32, 1
        // when we need #n, you just read it from %n.addr.
        let %6 = load %n.addr
        // comparison produce an `i8` value.
        let %cmp = eq %6, 0
        br %cmp, label %Ltrue, label %Lfalse
    %Ltrue:
        // retuen value = 1.
        let %10 = store 1, %ret.addr
        jmp label %Lret
    %Lfalse:
        // n - 1
        let %13 = load %n.addr
        let %14 = sub %13, 1
        // factorial(n - 1)
        let %res = call @factorial, %14
        // n
        let %16 = load %n.addr
        // n * factorial(n - 1)
        let %17 = mul %16, %res
        // write local variable `ans`
        let %18 = store %17, %ans.addr
        // now we meets `return ans`, which means we
        // should first read value from `%ans.addr` and then
        // write it to `%ret.addr`.
        let %19 = load %ans.addr
        let %20 = store %19, %ret.addr
        jmp label %Lret
    %Lret:
        // load return value from %ret.addr
        let %ret.val = load %ret.addr
        ret %ret.val
    }
    ```

具体的中间代码定义、生成规则、模板导读以及相关工具可以参考 [Zero IR](zero.md) 和 [Accipit IR](accipit.md)。

## 你的任务

你的任务就是在语法树和语义分析的基础上，将语法树转为中间代码。你可以选择使用 Zero IR 或 Accipit IR。

!!! tip "我们介绍的只是中间代码生成的一种方式，你可以不必完全按照实验文档中的来实现，只需要保证最后生成的代码能够被我们的解释器正确执行即可。"

不同于前两次实验，你的编译器 compiler 必须支持**三个**命令行参数的情形，即：

```console
$ ./compiler <input_file> <output_file> --ir
```

该程序必须接受一个输入的源代码文件名作为参数，并且将输出的中间代码写入到指定的 `output_file` 文件中，我们只会使用这种方式来测试你的编译器。

`--ir` 参数是用来告诉你的编译器需要生成中间代码的。如果你已经完成了整个编译器，这个参数可以帮你分辨你的编译器是生成目标代码还是中间代码。

## 测试

我们的测试完全基于你的编译器将源代码处理后生成的中间代码进行。我们保证在本实验的测试样例都是语法语义正确的。

运行以下命令来测试你的编译器：

```console
$ python3 sp26-tests/test.py lab3 .
```

在实验 3 中，我们的测试完全采用输入输出形式。我们会首先运行你的编译器来编译目标文件，将输出写入到 `output_file` 文件中，然后将 `output_file` 文件作为输入，运行我们的解释器并比较输出结果是否一致。

例如在 `tests/lab3/factorial.sy` 中，我们会以 5 作为程序的输入，并检测输出是否为 120。

```c
// Input: 5
// Output: 120

int factorial(int n) {
    if (n == 0) return 1;
    return n * factorial(n - 1);
}

int main() {
    int n = read();
    int result = factorial(n);
    write(result);
    return 0;
}
```

如果你选择使用 Accipit IR，还需要在你仓库下的 `config.toml` 中添加如下配置：

```toml
use_accipit = true
```

这样我们的测试脚本会自动使用 Accipit IR 的解释器来运行你的中间代码。否则，当 `use_accipit = false` 时，我们会使用 Zero IR 的解释器来运行你的中间代码。

## 实验提交

你需要将作为 Lab 3 评分的代码放到 `lab3` 分支中，并提交一个实验报告，**不得超过 3 页**，内容包括:

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
        - 上述 AI 工具使用的完整记录可存放于仓库的 `reports/appends/lab3` 文件夹中，并在实验报告中简要说明。

!!! danger "实验报告存在以下情况的，将由助教现场验收，并相应扣分"
    - 若实验报告中**缺乏实现思路**或**未真实描述 AI 工具使用情况**，将由助教现场验收，并相应扣分。
    - 实验报告**禁止使用 AI**，包括但不仅限于直接生成报告内容、润色等。提交的报告将会使用 AI 检测工具进行检测。若 AI 生成的内容占比过高，或者检测出明显的 AI 生成痕迹，将由助教现场验收，并相应扣分。

我们只接受 PDF 格式的实验报告。你需要将报告放在仓库的 `reports/lab3.pdf` 中。

<!-- ## 实现建议

1. 你可以先不考虑函数调用和定义，全局变量与数组，先把只有一个主函数的程序编译通过后，再逐渐添加这些功能。
2. 对于变量的的定义和使用，有全局/局部及普通变量/数组**四种**情况。它们的实现方式有所不同，但也有很多代码可以共用（如全局变量和数组的使用与赋值都是通过地址来实现的）。
3. 我们建议在具体实现中 `translate_X` 的函数返回的是 IR Node 的结构体或者类，而不是直接返回字符串。然后和语法树的实现类似，通过形如 `dump` 函数来将 IR Node 转为字符串并输出到文件中。
这样做在 Lab 4 中会更加方便，因为你可以直接将 IR Node 作为输入传递给后端进行目标代码生成，而不需要再次解析字符串。
4. 实验 3 需要在实验 2 的基础上完成。你可以在实验 2 的语义分析部分添加中间代码生成的内容，同时进行语义检查和中间代码生成；也可以将中间代码生成需要的信息写到一个单独的数据机构中，等到语义检查全部完成并通过之后再生成中间代码。前者会让你写起来简单一点，后者会让你的编译器模块性更好一些。

举例而言，在实验 2 中，你可能会有如下的代码：

```c
Type type_check(Assign assign, Table table) {
    Type left = type_check(assign->left, table);
    Type right = type_check(assign->right, table);
    if (equal(left, right)) 
        ...
}

Type type_check(Ident ident, Table table) {
    return table.lookup(ident);
}
```

而在实验 3 中，你可能会修改为：

```c
// translate_exp 返回的是一个二元组，第一个元素是表达式的类型，第二个元素是生成的中间代码
(Type, Code) translate_exp(Ident ident, Table table, Temp place) {
    return (table.lookup(ident), Assign(place, ident));
}

// translate_stmt 不需要类型信息，只返回中间代码
Code translate_stmt(Assign assign, Table table) {
    (Type left, Code code1) = translate_exp(assign->left, table);
    (Type right, Code code2) = translate_exp(assign->right, table);
    if (equal(left, right)) // Lab 2 的语义检查
        ...
    return code1 + code2;
}
``` -->