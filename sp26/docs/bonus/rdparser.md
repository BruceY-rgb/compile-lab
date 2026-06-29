# Bonus 1: Lexer & Parser, DIY!

!!! info "开始之前"
    请务必在 **2026-6-24 23:59** 前完成实验并推送到你自己对应的远程仓库的 `bonus1` 分支中，否则将会按照评分标准扣分。

    建议你在开始前，先使用 `git checkout -b bonus1 lab4` 从 `lab4` 分支创建一个新的 `bonus1` 分支，并使用 `git push -u origin bonus1` 在远程仓库中创建该分支，这样你就可以在实验过程中随时使用 `git push origin bonus1` 提交代码，而不会误交到其他分支。

!!! info "Bonus 1 最高可获得 2 分加分"

在基础实验中，我们使用了 Flex 和 Bison 来生成词法分析器和语法分析器。在这个 Bonus 中，你需要自己实现一个词法分析器和语法分析器。

我们在课程中讲解了词法分析和语法分析的基本原理，在词法分析时，使用 NFA 表示一个正则表达式，并将其转化为 DFA，最后使用 DFA 来识别输入的字符串。在语法分析时，使用 CFG 表示一个文法，使用 LL(1)、LR(0)、LALR(1) 等方法来构建语法分析器。你可以尝试着将上课所学的方法转换为代码，在 SysY 上写一个语法分析器。

和首页中推荐的自学资料相同，这里推荐以下三个资源：

- [Compiling to Assembly from Scratch](https://keleshev.com/compiling-to-assembly-from-scratch/)：它讲了如何用 TypeScript 写一个把 JavaScript 子集编译成 ARM32 汇编的简易编译器。书中 Part I 中的 4、5、6 三个章节向你介绍了一种用 Typescript 手搓 Parser 的方法。
- [Crafting Interpreters](https://craftinginterpreters.com/)：这本书前半部分用 Java 写一个基于树的解释器，后半部分用 C 写一个为名叫 Lox 的语言服务的字节码虚拟机。在其中 Part II 的 4、5、6 三个章节是与 Parser 相关的介绍。如果你对 Java 没什么兴趣，更喜欢 Rust，也可以参考这个用 Rust 实现的 Lox：[lox-rs](https://github.com/jeschkies/lox-rs)。
- [ChibiCC](https://github.com/rui314/chibicc)：这个项目是 Mold（新 LLVM 链接器）的作者 Rui Ueyama 写的一个 C 编译器项目。其中的 Tokenize 和 Parse 两个阶段的任务与本实验一致。你可以参考它的代码，但**请不要抄袭**。

<!-- !!! note "Lexer 和 Parser 的融合"
    在我们课程学习时，Lexer 和 Parser 通常被明确区分开来，Lexer 负责将源代码转换为 token 序列，而 Parser 则根据这些 token 构建语法树。然而，随着编程语言的复杂化和一些特殊需求，现代编译器往往开始模糊 Lexer 和 Parser 的边界，甚至在某些情况下，直接由 Parser 处理词法分析的部分工作。例如，C++ 中的嵌套模板语法是一个典型案例：

    在旧版 C++ 标准中，写 `<<` 和 `>>` 会被 Lexer 识别为两个连续的位移操作符，而非模板边界。例如：

    ```cpp
    std::vector<std::vector<int>> matrix;
    ```

    其中的 `>>` 原本被 Lexer 视为位移运算符。为了处理这种情况，Lexer 必须在分析过程中加入复杂的上下文推理，或者依赖 Parser 来判断它是否为模板嵌套的结束符。C++ 后来通过引入语言标准的修改，允许模板嵌套中连续的 `>>` 被正确识别为两个模板边界，而非位移操作符，这实际需要 Parser 参与到词法层面的决策中，成为了 Lexer 和 Parser 职能模糊化的一个例子。 -->

我们更推荐感兴趣的同学直接阅读现代编译器的源码。以更直观地理解词法分析（Lexer）和语法分析（Parser）的实现方式。当前主流编译器，如 GCC、Clang、Rustc 等，大多采用手写的递归下降解析器，并手动处理左递归，而非使用自动生成工具（如 Flex & Bison）。选择手写递归下降解析器主要有这些考量：

- 性能上不会比递归上升的方法差很多，但是实现起来比递归上升简单许多；
- 像 Flex & Bison 这种表驱动的语法解析起几乎无法维护或者微调，而且因为现代语言众多的语法糖，使用这种 Parser Generator 反而可能有奇怪的地方和 Generator “抗争”一下，更加麻烦；
- 手写 Parser 对于错误恢复和丰富报错信息输出来说也更加方便。

感兴趣的同学可以先从 Clang 的源码入手，Clang 的源码可读性较高，比较直观。

??? note "Clang 源代码导读：Lexer & Parser"
    Clang 的源代码在 [llvm-project](https://github.com/llvm/llvm-project) 仓库中的 `clang` 目录下。

    - Clang 词法分析（Lexer）：采用递归下降的方式解析字符流，并支持预处理指令的处理。我们这里不需要关注预处理指令的处理，重点关注如何将字符流转化为 Token 序列。
        - 源码位置：头文件位于 `clang/include/clang/Lex`，实现位于 `clang/lib/Lex`。
        - 建议关注这些内容：`clang/lib/Lex/Lexer.cpp` 的 `Lexer::Lex` 函数是 Lexer 的入口函数，`Lexer::LexTokenInternal` 函数是 Lexer 的核心函数，根据当前字符流的状态，构造一个 Token。
    - Clang 语法分析（Parser）：采用递归下降的方式解析 Token 序列，并构建抽象语法树。Clang 的 Parser 将不同的语法结构（如表达式、语句、函数）分解到独立的函数中，便于维护和阅读。
        - 源码位置：头文件位于 `clang/include/clang/Parse`，实现位于 `clang/lib/Parse`。
        - 建议关注这些内容：`clang/lib/Parse/ParseExpr.cpp` 负责表达式的解析，涵盖了从基本字面量到复杂函数调用的所有表达式类型，`clang/lib/Parse/ParseStmt.cpp` 处理语句，包括条件语句、循环语句、返回语句等。

## 你的任务

你需要实现一个词法分析器和语法分析器，将 SysY 语言的源代码转化为抽象语法树。本实验需要你纯手搓词法和语法分析器，不允许使用任何生成器，例如 Flex & Bison 或 Rust 的 `pest` 库等等，我们会检查你的代码是否符合这个要求。


本次 Bonus 实验是基于 Lab 4 进行的，同样你可以选择使用 RV32 工具链 或者 Venus：

=== "针对 RV32 工具链的编译器"

    你的编译器 compiler 必须支持**两个**命令行参数的情形，即：

    ```console
    $ ./compiler <input_file> <output_file>
    ```

    该程序必须接受一个输入的源代码文件名作为参数，并且将输出的汇编代码写入到指定的 `output_file` 文件中，我们只会使用这种方式来测试你的编译器。

    在提交的时候，你需要将你的编译器放到 `bonus1` 分支中，并且在 `config.toml` 中修改以下选项：

    ```toml
    use_qemu = true
    ```

    这样，我们的 CI 测试脚本在测试 Bonus 1 时会使用 RISC-V 32 工具链进行编译、运行，而不是直接使用 Venus 模拟器来运行。你可以运行以下命令来测试你的编译器：

    ```console
    $ python3 sp26-tests/test.py bonus1 .
    ```
    
=== "针对 Venus 模拟汇编的编译器"

    你的编译器 compiler 必须支持**三个**命令行参数的情形，即：

    ```console
    $ ./compiler <input_file> <output_file> --venus
    ```

    该程序必须接受一个输入的源代码文件名作为参数，并且将输出的汇编代码写入到指定的 `output_file` 文件中，第三个参数 `--venus` 是提示程序需要使用 Venus 测试。我们只会使用这种方式来测试你的编译器。

    运行以下命令来测试你的编译器：

    ```console
    $ python3 sp26-tests/test.py bonus1 .
    ```

## 实验提交

你需要将作为 Bonus 1 评分的代码放到 `bonus1` 分支中，并提交一个实验报告，**页数不限**，内容包括：

- **实现思路**：描述你完成实验的思路和方法，重点描述你是如何实现的。请不要照搬实验指导。
- **难点与亮点**：你在实现过程中遇到的难点？说说你是如何解决这些难点的；你实现中的亮点？重点描述你的实现中的亮点，你认为最个性化、最具独创性的内容，避免大段地向报告里贴代码。
- **优化效果分析**：你的优化技术对编译器性能的影响？定量分析你的优化技术对编译器性能的影响，例如编译时间、生成代码大小、代码运行速度等，并最好能够尝试与 GCC 的不同优化等级进行对比。
- **心得体会（20%）**：做完这次实验后，有什么心得体会？说说你完成实验后的真实感悟、避坑指南或吐槽。
- **附加内容（不计入页数）**：
    - **建议（可选）**：你对本次实验有什么建议和想法？你可以提出对实验设计的建议，指出实验指导里描述含糊的地方，或者是对实验的任何想法。
    - **借鉴声明**：如果你复用借鉴了参考代码或其他资源，请明确写出你借鉴了哪些内容。如果你没有上述情况，也请在报告中明确声明。**即使你声明了代码借鉴，你也需要自己独立认真完成实验。**
    - **AI 工具使用记录**：如果你使用了 AI 工具辅助实验，请在报告中明确指出使用了什么工具、如何使用。例如：
        - 问答类（ChatGPT 等）：附上你与 AI 工具的对话截图或文字记录。
        - Tab Completion 类（如 Cursor Tab 等）：明确指出使用工具名称。
        - Agent 类（Claude Code 等）：请让 Agent 生成代码时一同生成 update notes，并在提交代码库保留所有 Agent 生成的辅助文档（例如 `AGENT.md`），在报告中附上相关截图或文字记录。
        - 上述 AI 工具使用的完整记录可存放于仓库的 `reports/appends/bonus1` 文件夹中，并在实验报告中简要说明。

!!! danger "实验报告存在以下情况的，将由助教现场验收，并相应扣分"
    - 若实验报告中**缺乏实现思路**或**未真实描述 AI 工具使用情况**，将由助教现场验收，并相应扣分。
    - 实验报告**禁止使用 AI**，包括但不仅限于直接生成报告内容、润色等。提交的报告将会使用 AI 检测工具进行检测。若 AI 生成的内容占比过高，或者检测出明显的 AI 生成痕迹，将由助教现场验收，并相应扣分。

我们只接受 PDF 格式的实验报告。你需要将报告放在仓库的 `reports/bonus1.pdf` 中。