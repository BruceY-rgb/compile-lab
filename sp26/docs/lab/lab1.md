# Lab 1: 词法与语法分析

!!! info "开始之前"
    请务必在 **2026-4-19 23:59** 前完成实验并推送到你自己对应的远程仓库的 `lab1` 分支中，否则将会按照评分标准扣分。

    建议你在开始前，先使用 `git checkout -b lab1 lab0` 从 `lab0` 分支创建一个新的 `lab1` 分支，并使用 `git push -u origin lab1` 在远程仓库中创建该分支，这样你就可以在实验过程中随时使用 `git push origin lab1` 提交代码，而不会误交到其他分支。

!!! abstract "[Checklist: 测试 Bug 排查](./lab0.md#bug-checklist)"

??? note "模版代码使用指南"

    为了帮助大家高效地搭建编译器框架，我们提供了一个模板代码。该模板基于 C++17 编写，结合了 Flex 和 Bison 作为语法和词法分析工具，并利用 C++ RTTI（Run-Time Type Information）和 `std::shared_ptr` 智能指针来实现核心功能。模板中尽可能将各个编译阶段解耦，模块化地实现整个编译器。理论上，如果你填完模板代码中的所有空，应该能够编译 A + B Problem。但要通过所有测试点，你还需要扩展和修改模板代码，例如添加更多的 AST 节点类型、增加语法规则等。

    !!! tip "如果你不喜欢这套实现，可以参考模板代码提供的编译器框架，选择其他编程语言或实现方法来完成实验。"

    ??? note "OCaml 模板"

        除了 C++ 模板外，我们还提供了基于 OCaml 的模板代码。OCaml 模板使用 OCamllex + Menhir 作为词法/语法分析工具，Dune 作为构建系统。如果你更熟悉函数式编程，可以选择使用 OCaml 模板。关于环境，我们使用 OCaml 编译器、Dune (≥ 3.17)、Menhir，你可以通过 `opam install dune menhir` 安装所需依赖。你也可以直接使用从[这个 Dockerfile](https://github.com/ZJU-CP/tools/blob/main/Dockerfile.ocaml) 构建的 Docker 镜像：

        ```console
        $ docker run -it ghcr.io/zju-cp/compiler-tester-ocaml:latest /bin/bash
        ```

        OCaml 模板位于 `template/ocaml/lab1` 分支下。你理应可以通过如下命令编译运行：

        ```console
        $ dune build
        $ ./compiler input.sy
        ```

        测试方式与 C++ 相同，你的任务也理论上与 C++ 模板完全一致。我们会在后续讲解 C++ 模板时也插入对 OCaml 的介绍。

        Lab 1 OCaml 框架中的代码结构如下：

        ```text
        .
        ├── ast
        │   ├── ast.ml      // AST 节点类型定义与打印
        │   └── ast.mli     // AST 模块接口
        ├── bin
        │   └── main.ml     // 编译器入口
        ├── lib
        │   ├── loc.ml      // 源代码位置信息
        │   └── tree.ml     // 通用树形打印工具
        └── parsing
            ├── lexer.mll   // 词法分析器 (OCamllex)
            └── parser.mly  // 语法分析器 (Menhir)
        ```

    Lab 1 部分的模板代码位于 [sp26-starter](https://git.zju.edu.cn/compiler/sp26-starter) 仓库中的 `template/lab1` 分支下，你可以将该仓库设为上游仓库，并合并上游仓库的 `template/lab1` 分支到你的 `lab1` 分支中：

    ```console
    $ git remote add upstream https://git.zju.edu.cn/compiler/sp26-starter.git
    $ git fetch upstream
    $ git merge upstream/template/lab1
    ```

    如果出现冲突，Git 会提示你解决冲突，手动编辑冲突的文件并在解决后执行：

    ```console
    $ git add <conflict-file>
    $ git commit
    ```

    合并完成后，你可以先把模板代码推送到远程仓库：

    ```console
    $ git push origin lab1
    ```

    然后开始修改模板代码，完成你的编译器。

    你需要选择 `template/lab1` 分支。

    Lab 1 框架中的代码结构如下：

    ```text
    src
    ├── ast
    │   ├── tree.cpp    // AST 节点的实现
    │   └── tree.hpp
    ├── common.hpp
    ├── lexer
    │   └── lexer.l     // 词法分析器
    ├── main.cpp
    └── parser
        └── parser.y    // 语法分析器
    ```

    在实验指导的理论讲解的对应部分，会给出模板代码中的参考实现导读。

编译器的第一步是将源代码转换为**抽象语法树（AST）**，以便后续处理。首先通过**词法分析**将输入字符串切分为一个个 token，然后通过**语法分析**，根据 SysY 语言的文法规则，将这些 token 组织成抽象语法树。

??? note "模板代码中的 Compiler 是如何跑起来的？"

    在了解具体的词法和语法分析机制之前，我们可以先对整个 compiler 的生命周期有一个直观的认识。

    1. **构建（Build）阶段**：首先，项目根目录的 `Makefile` 会调用 Flex 将你编写的词法规则文件（`lexer.l`）编译成一个 C++ 源文件（`lexer.yy.cc`）。紧接着，调用 Bison 将语法规则文件（`parser.y`）编译成语法分析的 C++ 源文件（`parser.tab.cc`）以及其接口头文件。随后，`g++` 将这些由工具自动生成的代码，连同 `main.cpp` 与 AST 节点实现代码，一起编译并链接成最终的可执行程序 `compiler`。
    
    2. **运行（Runtime）阶段**：当执行 `./compiler input.sy` 时，`main` 函数会打开用户输入的源代码文件，将其文件指针赋给全局接口变量 `yyin`（词法分析器的预设输入源），然后调用 Bison 提供的核心接口 `yyparse()`，正式把接力棒递给语法分析器。
        * **`parser` 模块**：进入 `yyparse()` 后，Parser 尝试按照你写的层级语法公式进行解析。它并不会自己去从头读取源文件，而是每碰到缺少后续单词的情况，就会调用 Flex 暴露的接口 `yylex()`，向 Lexer 索要下一个 Token。
        * **`lexer` 模块**: 每当 `yylex()` 被 Parser 调用，Lexer 就会从由 Main 传入的 `yyin` 缓冲区中读取字符流，利用自身生成的自动状态机判断出它属于哪个 Token 类型（例如关键字、变量名、数字常量等）。它将该单词蕴含的可能字面量数值赋给全局通信通道 `yylval` 交给 Parser，并向其 `return` 该 Token 的种类 ID。
        * **语法树构造**: Parser 拿到 Token 后在内部进行移进/规约（Shift/Reduce）。当一组 Tokens 组成的语法规则匹配成功时，它会执行我们在 `parser.y` 文件中手写的 C++ 代码片段（Actions），在堆上动态实例化 `AST::Node` 派生子节点，把它们挂载到语法树上。
    
    !!! tip "补充：`lexer.yy.cc` 与 `parser.tab.cc` 这对母体"
        - **`yylex()` 诞生于 `lexer.yy.cc`**：`yylex()` 是词法分析的核心驱动函数。执行 `flex` 命令时，它会把 `lexer.l` 里的正则规则转换为一个长长的状态机 C++ 源码并保存在 `lexer.yy.cc` 当中，`yylex()` 的函数体就被硬编码在这。
        - **`yyparse()` 诞生于 `parser.tab.cc`**：同理，`yyparse()` 的核心移进规约算法是在执行 `bison` 命令后被自动生硬编码写进 `parser.tab.cc` 里的。在 Parser 解析代码的环节中，也是 `parser.tab.cc` 跨文件在不断地调用跑在 `lexer.yy.cc` 里的取词机器 `yylex()`。

    OCaml 模板的构建和运行流程与 C++ 模板有所不同，但整体思路一致。

    3. **构建（Build）阶段**：执行 `dune build` 时，Dune 会自动调用 OCamllex 将词法规则文件（`parsing/lexer.mll`）编译成 OCaml 源文件（`lexer.ml`），调用 Menhir 将语法规则文件（`parsing/parser.mly`）编译成语法分析的 OCaml 源文件（`parser.ml`）及其接口文件。随后，OCaml 编译器将这些生成的代码连同 `bin/main.ml`、AST 定义等一起编译链接成最终的可执行程序 `compiler`。

    4. **运行（Runtime）阶段**：当执行 `./compiler input.sy` 时，`main.ml` 中的 `parse` 函数会读取整个源文件内容，创建一个 `Lexing.lexbuf`（词法分析缓冲区），然后调用 `Parser.comp_unit Lexer.read lexbuf`，将 Lexer 和 Parser 连接起来开始解析。
        * **`Lexer` 模块**：`Lexer.read` 函数由 OCamllex 从 `lexer.mll` 自动生成。每次被 Parser 调用时，它从 `lexbuf` 中读取字符，根据正则规则匹配出一个 token 并返回。
        * **`Parser` 模块**：`Parser.comp_unit` 函数由 Menhir 从 `parser.mly` 自动生成。它反复调用 `Lexer.read` 获取 token，根据语法规则进行移进/规约，并在规约时执行 `parser.mly` 中编写的 OCaml 动作代码，直接构造 AST 值。
        * **语法树构造**：与 C++ 模板不同，OCaml 使用代数数据类型（ADT）表示 AST，Menhir 的动作直接返回 OCaml 值，无需裸指针和类型转换。例如，一条规则的动作 `{ mk ~loc:(loc_of_pos $startpos) (Return (Some e)) }` 会直接构造一个 `stmt node` 值。
        * **错误处理**：当 Parser 遇到语法错误时会抛出 `Parser.Error` 异常，当 Lexer 遇到非法字符时会抛出 `Lexer.Lexer_error` 异常，`main.ml` 捕获这些异常并以返回值 1 退出。

在这一阶段中，我们只关注语法结构的正确性，而不处理语义问题（如数组越界、类型不匹配等）。我们需要构建出正确的抽象语法树，为后续编译阶段做好准备。

## 词法分析

词法分析的目的是将源代码（一个个字符）解析成一个个 token。Token 就是词法分析的最小单元，表示一个程序有意义的最小分割。下表为常见的 token 类型和样例：

| 类型   | 样例                           |
| ------ | ------------------------------ |
| 标识符 | `x`、`sum`、`i`                |
| 关键字 | `if`、`while`、`return`        |
| 分隔符 | `}`、`(`、`)`、`;`             |
| 运算符 | `+`、`<` 、`=`                 |
| 字面量 | `true`、`666`、`"hello world"` |
| 注释   | `// this is a comment.`        |

```c
x = a + b * 2;
```

上面的简单代码会被解析为：
`[(identifier, x), (operator, =), (identifier, a), (operator, +), (identifier, b), (operator, *), (literal, 2), (separator, ;)]`.

词法分析并不复杂，本质上就是一个有限状态机（[正则文法](https://en.wikipedia.org/wiki/Regular_expression)）的匹配。只需要简单的遍历源代码中的字符，根据字符的值分别判断即可。

??? note "模板代码中的词法分析"
    在 C++ 框架中，我们使用 Flex 进行词法分析。词法分析器的定义在 `lexer/lexer.l` 中，你需要根据 SysY 语言的词法规则，定义更多的 token 类型，具体可参考附录中的 [Flex 与词法分析](../appendix/lex_yacc.md#flex) 部分。

    ??? note "OCaml 中的词法分析"
        OCaml 模板使用 **OCamllex** 进行词法分析，规则文件为 `parsing/lexer.mll`。语法与 Flex 类似——每条规则由一个正则表达式和对应的 OCaml 动作组成。模板已给出部分 token（如 `+`、`-`、`=`、关键字 `int`、`return` 等）作为示例，你需要补充其余运算符和关键字的规则。

## 语法分析

由词法分析得到的一个个 token 在语法分析阶段进行进一步的解析。具体来说，需要：

1. 对于一串合法的 tokens，生成语法树。
2. 对于的一串**不合法**的 token，检测到可能的错误并报告给用户。

如果说词法分析由[正则文法](https://en.wikipedia.org/wiki/Regular_expression)为基础，语法分析则以[上下文无关文法](https://en.wikipedia.org/wiki/Context-free_grammar)作为基石。

!!! tip "正则文法和上下文无关文法的学习可以参考上课内容和鹤翔万里的[计算理论笔记](https://note.tonycrane.cc/cs/tcs/toc/)。"

??? note "抽象语法树"
    抽象语法树（Abstract Syntax Tree，简称 AST）是 Parse Tree 的一个保留核心细节的化简版本, 被输入进语义分析部分。

    ![WHY_AST](../assets/image/why_AST.svg#only-light)
    ![WHY_AST](../assets/image/why_AST.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

    Parse Tree 严格遵循 Grammar，它的目的是证明一段代码“合乎语法”。因此，它保留了每一个字符细节，有许多冗余信息。而 AST 只保留变量名、操作符、函数调用等核心逻辑，以便后续进行语义分析。


为了让同学们更直观地理解抽象语法树，下图提供了一个 if-else 条件语句解析成抽象语法树的案例。

```cpp title="if-else statement"
if (a < b) {
    c = 2;
    return c;
} else {
    c = 3;
}
```
对应的抽象语法树：

![AST](../assets/image/AST.svg#only-light)
![AST](../assets/image/AST.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }


??? note "模板代码中的语法分析与抽象语法树"

    在 `ast/tree.hpp` 中定义了 AST 的节点类型，其中，`AST::Node` 是所有节点的基类，它的定义如下：

    ```cpp title="src/ast/tree.hpp"
    class Node;
    using NodePtr = std::shared_ptr<Node>;
    class Node {
    public:
    int lineno;

    virtual std::vector<NodePtr> get_children() { return std::vector<NodePtr>(); }
    void print_tree(std::string prefix = "", std::string info_prefix = "");
    virtual std::string to_string() = 0;

    Node() : lineno(yylineno) {}
    virtual ~Node() = default;
    };
    ```

    你需要根据 SysY 语言的语法规则，定义更多的 AST 节点类型，例如 `AST::WhileStmt`、`AST::IfStmt` 等。为了方便打印语法树，我们还给每个节点添加了 `to_string` 和 `get_children` 方法。具体的实现可以参考已经给出的类的定义。

    ??? tip "使用其它语言的 AST 表示与打印"
        AST 是编译器的核心数据结构之一，在尝试使用不同编程语言时会有不同的技术方案，除了模板中所使用的 C++ 外，我们以 `Stmt` 为例，介绍在 C、Rust 中表示语法树这类数据结构的一种方法。

        === "C"

            C 语言常用的技巧是 enum + union：
            ``` c
            enum StmtKind {
                STMT_EXPR,
                STMT_IF,
                STMT_WHILE,
                STMT_RETURN,
            };

            struct Stmt {
                enum StmtKind kind;
                union {
                    struct Expr *expr;
                    struct IfStmt *if_stmt;
                    struct WhileStmt *while_stmt;
                    struct ReturnStmt *return_stmt;
                };
            }; 
            ```

            我们使用一个枚举类型来表示语法树的节点类型，并且使用 union 来存储不同类型的节点。
            而在打印语法树时，只需要 switch 递归遍历即可：

            ``` c
            void print_stmt(struct Stmt *stmt) {
                switch (stmt->kind) {
                    case STMT_EXPR:
                        print_expr(stmt->expr);
                        break;
                    case STMT_IF:
                        print_if_stmt(stmt->if_stmt);
                        break;
                    case STMT_WHILE:
                        print_while_stmt(stmt->while_stmt);
                        break;
                    case STMT_RETURN:
                        print_return_stmt(stmt->return_stmt);
                        break;
                }
            }
            ```

        === "Rust"
            熟悉 Rust 的同学一定会使用 enum，即枚举类型来表示 AST：

            ```rust
            enum Stmt {
                ExprStmt(Expr),
                IfStmt(IfStmt),
                WhileStmt(WhileStmt),
                ReturnStmt(ReturnStmt),
            }  

            struct ExprStmt {
                expr: Expr,
            }

            struct IfStmt {
                ...
            }
            ```

            在打印语法树时，我们可以使用 match 语法来实现：

            ```rust
            fn print_stmt(stmt: &Stmt) {
                match stmt {
                    Stmt::ExprStmt(expr_stmt) => print_expr(&expr_stmt.expr),
                    Stmt::IfStmt(if_stmt) => print_if_stmt(if_stmt),
                    Stmt::WhileStmt(while_stmt) => print_while_stmt(while_stmt),
                    Stmt::ReturnStmt(return_stmt) => print_return_stmt(return_stmt),
                }
            }
            ```

    ??? note "OCaml 中的 AST 与语法分析"
        OCaml 模板使用代数数据类型（ADT）定义 AST，使用 **Menhir** 进行语法分析。

        AST 定义在 `ast/ast.ml` 中，使用 `type` 定义各种节点类型。例如：

        ```ocaml
        type binop = Add (* TODO: 添加更多运算符 *)
        type stmt =
          | Assign of lval node * expr node
          | Return of expr node option
          | Expr of expr node
          | Block of block_item list
          (* TODO: 添加 if/else, while 等语句类型 *)
        ```

        语法规则在 `parsing/parser.mly` 中定义，语法与 Bison 类似但更简洁——Menhir 的动作直接返回 OCaml 值，无需裸指针和类型转换。模板已给出加法表达式和基本语句的规则作为示例，你需要补充完整的表达式优先级链和其余语句类型。

    在 C++ 框架中，我们使用 Bison 进行语法分析，其定义在 `parser/parser.y` 中，同样地，你需要根据 SysY 语言的语法规则，定义更多的语法规则。你可以阅读附录中的 [Bison 与语法分析](../appendix/lex_yacc.md#bison) 部分来学习如何使用 Bison 构建语法树。

    需要注意的是，由于我们使用了智能指针，但 Bison 中的 yylval 是一个 union 类型，不能直接使用智能指针等含有构造函数、析构函数、虚函数的类型。因此，在 union 中，我们传递的是 `AST::Node *` 裸指针：

    ```bison title="src/parser/parser.y"
    // yylval 的定义, 我们把它定义成了一个联合体 (union)
    // 因为 token 的值有的是字符串指针, 有的是整数
    // 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
    // 这里的 union 是一种特殊的数据结构，所有成员共享相同的内存地址
    // 因此只能存储一个成员的值，且占用的内存大小等于其最大成员的大小
    // union 中的类型必须是 trivially copyable 的类型
    // 也就是说不能包含有自定义的构造函数、析构函数、虚函数等
    // 符合这个条件的类型有基本数据类型、指针、C 结构体、枚举等
    // 因此我们不能使用 std::shared_ptr，而只能使用普通指针
    %union{
        int int_val;
        char *str_val;
        BinaryOp op;
        AST::Node *node;
    }
    ```

    在解析过程中，创建对应的 AST 节点时，我们需要传入智能指针作为参数。此时我们将裸指针转换为智能指针：

    ```bison title="src/parser/parser.y"
    // 同样的，由于 union 中的类型不能是 std::shared_ptr
    // 而 FuncDef 初始化时需要传入一个 BlockPtr (std::shared_ptr<Block>)
    // 所以我们需要通过 static_cast 来转换类型
    // 再用 std::shared_ptr 包装一下
    // 才能传入 FuncDef 的构造函数
    FuncDef : "int" IDENT "(" ")" Block { $$ = new FuncDef(BasicType::Int, $2, BlockPtr(static_cast<Block *>($5))); }
        ;
    ```

    而在使用每个类的方法时，我们需要将 `AST::Node *` 静态转换为对应类型的指针，才能访问对应的方法：

    ```bison title="src/parser/parser.y"
    // 同样由于 union 中的类型不能是 std::shared_ptr
    // 只是普通的指针，所以我们需要通过 static_cast 来转换类型
    // 才能访问到对应的成员和函数
    VarDecl : "int" VarDefs ";" { static_cast<VarDecl *>($2)->btype = BasicType::Int; $$ = $2; }
    ```

    语法规则的编写与 AST 节点定义是相辅相成的，你需要仔细考虑如何设计语法规则以及 AST 节点。

!!! note "设计自己的 Lexer 和 Parser"
    当然，你完全可以不使用 Flex 和 Bison，而是自己写一个词法分析器和基于递归下降的语法分析器，这件事并不像你想象的那样困难。如果你自己手搓了 Lexer 和 Parser，可以等 Lab 4 完成后提交至 [Bonus 1: Lexer & Parser, DIY!](../bonus/rdparser.md)。

??? tip "如果你不确定如何设计抽象语法树 AST，可以参考 Clang 对 AST 的设计。"

    以这个输入文件为例：

    ```c title="example.c"
    int main() {
        int a = 1, b = -1;
        return a + b;
    }
    ```

    你可以使用以下命令输出词法分析后的所有 tokens：

    ```console
    $ clang -Xclang -dump-tokens -fsyntax-only example.c
    int 'int'        [StartOfLine]  Loc=<test.c:1:1>
    identifier 'main'        [LeadingSpace] Loc=<test.c:1:5>
    l_paren '('             Loc=<test.c:1:9>
    r_paren ')'             Loc=<test.c:1:10>
    l_brace '{'      [LeadingSpace] Loc=<test.c:1:12>
    int 'int'        [StartOfLine] [LeadingSpace]   Loc=<test.c:2:5>
    identifier 'a'   [LeadingSpace] Loc=<test.c:2:9>
    equal '='        [LeadingSpace] Loc=<test.c:2:11>
    numeric_constant '1'     [LeadingSpace] Loc=<test.c:2:13>
    comma ','               Loc=<test.c:2:14>
    identifier 'b'   [LeadingSpace] Loc=<test.c:2:16>
    equal '='        [LeadingSpace] Loc=<test.c:2:18>
    minus '-'        [LeadingSpace] Loc=<test.c:2:20>
    numeric_constant '1'            Loc=<test.c:2:21>
    semi ';'                Loc=<test.c:2:22>
    return 'return'  [StartOfLine] [LeadingSpace]   Loc=<test.c:3:5>
    identifier 'a'   [LeadingSpace] Loc=<test.c:3:12>
    plus '+'         [LeadingSpace] Loc=<test.c:3:14>
    identifier 'b'   [LeadingSpace] Loc=<test.c:3:16>
    semi ';'                Loc=<test.c:3:17>
    r_brace '}'      [StartOfLine]  Loc=<test.c:4:1>
    eof ''          Loc=<test.c:4:2>
    ```

    使用以下命令查看 Clang 语法分析后构建的语法树：

    ```console
    $ clang -Xclang -ast-dump -fsyntax-only example.c
    TranslationUnitDecl 0x13080b008 <<invalid sloc>> <invalid sloc>
    |-TypedefDecl 0x13080beb0 <<invalid sloc>> <invalid sloc> implicit __int128_t '__int128'
    | `-BuiltinType 0x13080b5d0 '__int128'
    # ... omit more built in typedef ...
    |-TypedefDecl 0x11780c268 <<invalid sloc>> <invalid sloc> implicit __builtin_va_list 'char *'
    | `-PointerType 0x11780c1b0 'char *'
    |   `-BuiltinType 0x13080b0b0 'char'
    `-FunctionDecl 0x11780c310 <example.c:1:1, line:4:1> line:1:5 main 'int ()'
      `-CompoundStmt 0x11780c628 <col:12, line:4:1>
        |-DeclStmt 0x11780c570 <line:2:5, col:22>
        | |-VarDecl 0x11780c418 <col:5, col:13> col:9 used a 'int' cinit
        | | `-IntegerLiteral 0x11780c480 <col:13> 'int' 1
        | `-VarDecl 0x11780c4b8 <col:5, col:21> col:16 used b 'int' cinit
        |   `-UnaryOperator 0x11780c540 <col:20, col:21> 'int' prefix '-'
        |     `-IntegerLiteral 0x11780c520 <col:21> 'int' 1
        `-ReturnStmt 0x11780c618 <line:3:5, col:16>
          `-BinaryOperator 0x11780c5f8 <col:12, col:16> 'int' '+'
            |-ImplicitCastExpr 0x11780c5c8 <col:12> 'int' <LValueToRValue>
            | `-DeclRefExpr 0x11780c588 <col:12> 'int' lvalue Var 0x11780c418 'a' 'int'
            `-ImplicitCastExpr 0x11780c5e0 <col:16> 'int' <LValueToRValue>
              `-DeclRefExpr 0x11780c5a8 <col:16> 'int' lvalue Var 0x11780c4b8 'b' 'int'
    ```

    如果你本地没有安装 Clang，可以使用 [Compiler Explorer](https://godbolt.org) 来运行 Clang。在该平台上，你可以选择相应版本的 Clang 编译器，添加所需参数并查看输出。

## 你的任务

首先不要忘了更新测试库、切换到 `lab1` 分支：

```console
$ git submodule update --recursive --remote --merge
$ git checkout -b lab1
$ git push -u origin lab1
```

现在到你了，你需要完成编译器的词法分析和语法分析部分，能够解析出符合词法与语法的 SysY 语言源代码，并且我们**强烈建议**你实现一个打印语法树的函数，以便于后续调试。为了减轻你的负担，我们只要求你实现**简化版**的 SysY 语言，见[附录：SysY 语言规范](../appendix/sysy.md)。

   * **词法分析**：你需要根据 SysY 语言的词法规则，补充词法规则文件，定义更多的 token 类型。
       * C++ 模板：补充 `src/lexer/lexer.l`
       * OCaml 模板：补充 `parsing/lexer.mll`
   * **语法分析**：首先，你需要根据 SysY 语言的语法规则，定义更多的 AST 节点类型；接着，补充语法规则文件，定义更多的语法规则。
       * C++ 模板：在 `src/ast/tree.hpp` 中定义 AST 节点，在 `src/parser/parser.y` 中定义语法规则
       * OCaml 模板：在 `ast/ast.ml`（及 `ast/ast.mli`）中定义 AST 类型，在 `parsing/parser.mly` 中定义语法规则

以 [Lab 0](lab0.md#sysy) 中的阶乘函数为例，你打印出的语法树可以是这样的：

```text
 CompUnit (line 11)
 ├─ FuncDef <return_btype: int, name: factorial> (line 5)
 │  ├─ Param <btype: int, ident: n> (line 1)
 │  └─ Block (line 5)
 │     ├─ IfStmt (line 2)
 │     │  ├─ BinaryCond <op: ==> (line 2)
 │     │  │  ├─ LVal <ident: n> (line 2)
 │     │  │  └─ IntConst <value: 1> (line 2)
 │     │  └─ ReturnStmt (line 3)
 │     │     └─ IntConst <value: 1> (line 3)
 │     └─ ReturnStmt (line 4)
 │        └─ BinaryExp <op: *> (line 4)
 │           ├─ LVal <ident: n> (line 4)
 │           └─ FuncCall <name: factorial> (line 4)
 │              └─ BinaryExp <op: -> (line 4)
 │                 ├─ LVal <ident: n> (line 4)
 │                 └─ IntConst <value: 1> (line 4)
 └─ FuncDef <return_btype: int, name: main> (line 11)
    └─ Block (line 11)
       ├─ VarDecl <btype: int> (line 7)
       │  └─ VarDef <ident: n> (line 7)
       │     └─ FuncCall <name: read> (line 7)
       ├─ VarDecl <btype: int> (line 8)
       │  └─ VarDef <ident: result> (line 8)
       │     └─ FuncCall <name: factorial> (line 8)
       │        └─ LVal <ident: n> (line 8)
       ├─ FuncCall <name: write> (line 9)
       │  └─ LVal <ident: result> (line 9)
       └─ ReturnStmt (line 10)
          └─ IntConst <value: 0> (line 10)
```

你的编译器必须支持一个命令行参数的情形，程序名必须为 `compiler`，且在仓库根目录下。你的编译器应该能够按照以下方式运行：

```console
$ ./compiler <input_file>
```

该程序必须接受一个输入的源代码文件名作为参数，我们只会使用这种方式来测试你的编译器。

<!-- 我们并不会对你打印的语法树格式进行测试，在 `tests/lab1` 目录下有我们参考编译器生成的语法树，你可以参考它们的格式调试你的编译器。 -->

## 测试

在仓库根目录下，运行以下命令来测试你的编译器：

```console
$ python3 sp26-tests/test.py lab1 .
```

在实验 1 中，我们提供了以下两种测试样例：

- 如 `tests/lab1/01_main.sy` 所示，源文件中的语法是正确的，你的编译器应该能够正确的解析出语法树并以返回值 0 退出。

    ```c
    int main(){
        return 0;
    }
    ```

- 如 `tests/lab1/error2.sy` 所示，源文件中的语法是错误的，你的编译器应该能够检测出错误并报错后以返回值 1 退出。

    ```c
    // Return 1 - Syntax Error Line 8: identifier "3c"

    int foo(int x){
    return x + 1;
    }

    int main(){
    int 3c = foo(1);
    }
    ```

我们的测试完全采用输入输出形式，即对于符合语法的源代码，你的程序正常运行后正常退出（返回 `0`），对于不符合语法的源代码，你的程序应该能够检测出错误报错后返回 `1`，我们对报错的格式没有要求。测试文件中的注释供你参考。

## 实验提交

你需要将作为 Lab 1 评分的代码放到 `lab1` 分支中，并提交一个实验报告，**不得超过 3 页**，内容包括:

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
        - 上述 AI 工具使用的完整记录可存放于仓库的 `reports/appends/lab1` 文件夹中，并在实验报告中简要说明。

!!! danger "实验报告存在以下情况的，将由助教现场验收，并相应扣分"
    - 若实验报告中**缺乏实现思路**或**未真实描述 AI 工具使用情况**，将由助教现场验收，并相应扣分。
    - 实验报告**禁止使用 AI**，包括但不仅限于直接生成报告内容、润色等。提交的报告将会使用 AI 检测工具进行检测。若 AI 生成的内容占比过高，或者检测出明显的 AI 生成痕迹，将由助教现场验收，并相应扣分。

我们只接受 PDF 格式的实验报告。你需要将报告放在仓库的 `reports/lab1.pdf` 中。