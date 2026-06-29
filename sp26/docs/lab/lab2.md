# Lab 2: 语义分析

!!! info "开始之前"
    请务必在 **2026-5-10 23:59** 前完成实验并推送到你自己对应的远程仓库的 `lab2` 分支中，否则将会按照评分标准扣分。

    建议你在开始前，先使用 `git checkout -b lab2 lab1` 从 `lab1` 分支创建一个新的 `lab2` 分支，并使用 `git push -u origin lab2` 在远程仓库中创建该分支，这样你就可以在实验过程中随时使用 `git push origin lab2` 提交代码，而不会误交到其他分支。

!!! abstract "[Checklist: 测试 Bug 排查](./lab0.md#bug-checklist)"

??? note "模版代码使用指南"

    Lab 2 部分的模板代码位于仓库中的 `template/lab2` 分支下，你可以将其合并到自己的 `lab2` 分支中：
    
    ```console
    $ git merge upstream/template/lab2
    ```

    如果出现冲突，Git 会提示你解决冲突，手动编辑冲突的文件并在解决后执行：

    ```console
    $ git add <conflict-file>
    $ git commit
    ```

    合并完成后，你可以先把模板代码推送到远程仓库：

    ```console
    $ git push origin lab2
    ```

    然后开始修改模板代码，完成你的编译器。
    
    Lab 2 框架的文件结构如下，高亮部分为新增和改动的文件：

    ``` hl_lines="4 5 8 11-18"
    src
    ├── ast
    │   ├── tree.cpp
    │   └── tree.hpp
    ├── common.hpp              // 新增错误定义
    ├── lexer
    │   └── lexer.l
    ├── main.cpp
    ├── parser
    │   └── parser.y
    └── semantic
        ├── symbol_table.cpp    // 符号表实现
        ├── symbol_table.hpp
        ├── type.cpp            // 类型定义
        ├── type.hpp
        ├── type_checker.cpp    // 类型检查器
        └── type_checker.hpp
    ```

    ??? note "OCaml 模板"

        OCaml 模板位于 `template/ocaml/lab2` 分支下，合并方式与 C++ 相同：

        ```console
        $ git merge upstream/template/ocaml/lab2
        ```

        合并后你可能需要在 `check.ml` 的 `check_stmt` 中为你在 Lab 1 新增的语句类型（如 `If`、`While` 等）补充对应的 match 分支，否则编译器会因非穷尽匹配而报错。模板已给出 `Assign`、`Return`、`Expr`、`Block` 的骨架作为示例。

        新增的文件结构：

        ```text
        semantic
        ├── env.ml          # 作用域符号表
        ├── types.ml        # 语义类型定义
        └── check.ml        # 类型检查
        ```

        构建和测试方式与 Lab 1 相同：

        ```console
        $ dune build
        $ python3 sp26-tests/test.py lab2 .
        ```

在实验 1 中我们进行了词法分析与语法分析，对于符合语法的程序，我们可以从中构建出一个语法树。

然而并不是所有合语法的程序都是合法的程序，下面的语句完全符合 SysY 语法（可以被上下文无关文法识别），但仍然不是合法的程序， 无法通过编译：

```c
int a[5] = {1, 2, 3, 4, 5};
int b = a;
```

这是因为它不符合 SysY 的语义约束，初始化的左右两边类型不匹配。在本次实验中，我们将进行语义分析，即检查程序是否符合语义规则。

合法的 SysY 程序需要满足诸如赋值语句类型匹配，函数调用类型匹配，变量定义不重复等语义约束。为了检查这些语义约束，我们需要遍历语法树，同时构建符号表以查找需要的类型信息，检查每个节点是否符合语义规则。

## 符号表

我们举一个例子来说明符号表的作用：

```c linenums="1"
int x = 1; // x1
int y = 0; // y1
{
    int x = 3; // x2
    y += x; // x2
}
y += x; // x1

```

对于上述代码，容易发现 `y += x` 加的两个 `x` 来自于两个不同的定义，那么编译器在生成代码的时候，仅仅看 `y += x` 是判断不出来 `x` `y` 的类型的。这就是符号表所需要解决的问题，为这些使用的变量提供额外的语义信息。

在符号表中，对于一个变量，我们需要记录下它的名字以及类型等信息。

对于函数，我们也需要在符号表中记录，方便在调用函数时查询符号，以及检查参数列表是否一致、返回值是否一致等。我们需要在符号表中记录下函数的名字、参数列表（包含多个变量符号）和返回值类型。在 SysY 中，所有函数都是全局的，并且不能重载。

!!! tip "实现函数重载以及嵌套函数"
    SysY 不支持函数重载和嵌套函数，但如果你对实现感兴趣，可以尝试实现这两个特性：

    - **函数重载：**允许多个函数拥有相同的函数名，但参数列表不同。需要注意的是，不能有两个函数拥有相同的参数列表但返回值不同。
    - **嵌套函数：**允许在函数内部定义函数，这些函数只能在外部函数内部调用。在内部函数中，可能会引用外部函数的变量，这就需要用到我们上课时提到的静态链（static link），具体实现可能会比较复杂。

    具体内容参见 Bonus 2 的 [函数](../bonus/extra.md#_9) 部分。

### 符号表实现

符号表的实现有两种风格：函数式风格和命令式风格。这里我们对两种风格进行一个简要的介绍。

#### 函数式风格

函数式风格的方法在每次进入新的作用域时都需要申请一个新的符号表，而当离开该作用域时，就可以将该符号表销毁，相当于维护了一个符号表的**栈**。例如刚才的例子，在运行到第 2 行时符号表是这样的：

```c
x -> x1
y -> y1
```

在运行到第 3 行时，我们新建一个符号表，并连接到之前的符号表上，然后根据第 4 行的定义更新当前的符号表：

```c
  表2    ->          表1           
x -> x2           x -> x1
                  y -> y1  
```

在寻找某个符号时，我们首先在当前的符号表中查找，如果找不到，我们就去上一个符号表中查找。例如在第 5 行时，我们会在表 2 中查找 `x`，而对于 `y`，表 2 中我们并没有重复定义，所以我们会在表 1 中找到。

而在第 6 行也就是离开该作用域时，我们只需要销毁当前的符号表，用它的上一个符号表替代即可。

??? note "这里的实现细节与虎书上的不完全相同，具体实现只要能满足符号表功能即可。"
    在虎书的函数式符号表中，遇到一个新的作用域时，会先复制上一层的符号表，并在复制后的新的符号表上进行修改。这样查找符号时，只需要查找最顶层的符号表。
    
    本实验指导中的函数式符号表则是在进入一个新的作用域时，直接创建一个新的空符号表。在查找符号时，若当前作用域未找到，则需要逐层向上递归查找父作用域。

#### 命令式风格

命令式风格的方法则不需要申请多个符号表，自始至终在单个符号表上进行动态维护。其中每一项对应的不是一个变量的定义，而是变量定义的**栈**。每次进入新的作用域时，将一个新的符号表压入栈中，离开作用域时弹出。还是刚才的例子，在运行到第 2 行时符号表是这样的：

```c
x -> [x1]
y -> [y1]
```

而运行到第 4 行时，符号表变成了：

```c
x -> [x2, x1]
y -> [y1]
```

在每次我们想要查看某个变量的定义时，我们只需要查看栈顶的元素即可。而当我们离开某个作用域时，我们只需要将在该语句块定义的元素弹出即可。所以在第 6 行时，符号表又变回了：

```c
x -> [x1]
y -> [y1]
```

那么我们怎么知道哪些元素是在该语句块下定义的呢？你可以在每次进入一个块时，将一个特殊的标记符压入栈中，而当你离开该块时，你只需要将栈顶的元素弹出直到遇到这个特殊的元素即可。

!!! tip "无论你使用的是什么语言，你都可以使用函数式风格或者命令式风格的符号表实现。"

??? note "模板代码中的类型定义与符号表"

    为了方便在符号表和类型检查时使用储存和传递类型信息，我们先对用到的类型进行定义，这些定义在 `semantic/type.hpp` 中，包括：

    - `Type` 基类：所有类型的基类，定义了 `compatible` 和 `to_string` 方法。
    - `PrimitiveType`：基本数据类型（基础实验中只有 `int` 和 `void`）。
    - `FunctionType`：函数类型，包含参数和返回值类型。

    在基类 `Type` 中定义的比较类型相等的 `compatible` 方法和转换为字符串的 `to_string` 方法如下：

    ```cpp title="src/semantic/type.hpp"
    class Type;
    using TypePtr = std::shared_ptr<Type>;
    class Type {
    public:
    virtual bool compatible(const TypePtr& other) const = 0;
    virtual std::string to_string() const = 0;
    virtual ~Type() = default;  // make the class polymorphic
    };
    ```

    在 `semantic/type.cpp` 中，我们已经给出了 `PrimitiveType` 和 `FunctionType` 的 `compatible` 函数的实现。你需要实现 `ArrayType` 类以及相关的 `compatible` 方法，比较数组类型的维度和元素类型是否相同。

    ??? note "C++ RTTI 与 `std::dynamic_pointer_cast`"

        在实现 `compatible` 时，我们使用到了 C++ RTTI 的特性。例如，`PrimitiveType::compatible` 函数如下：

        ```cpp
        bool PrimitiveType::compatible(const TypePtr& other) const {
        auto other_type = std::dynamic_pointer_cast<PrimitiveType>(other);
        return other_type && basic_type == other_type->basic_type;
        }
        ```

        从中，我们可以看出 `compatible` 函数的实现方式：首先尝试将 `other` 转换为对应的类型，然后再进行比较。这里使用了 `std::dynamic_pointer_cast` 函数，用于在运行时检查类型，尝试将 `shared_ptr` 转换为指定的类型。如果转换成功，返回转换后的指针，否则返回空指针。

    符号表用于存储变量信息，包括符号的名称、唯一名称以及类型。在 `semantic/symbol_table.hpp` 中，我们定义了 `Symbol` 类和 `SymbolTable` 类。

    对于 `Symbol` 类，它用于表示一个符号，包含了 name（符号名）、unique_name（符号的唯一标识符）和 type（符号类型）：

    ```cpp title="src/semantic/symbol_table.hpp"
    class Symbol;
    using SymbolPtr = std::shared_ptr<Symbol>;
    class Symbol {
    public:
    /// @brief The name of the symbol
    std::string name;
    /// @brief The unique name of the symbol
    std::string unique_name;
    /// @brief The type of the symbol
    TypePtr type;
    #warning Symbol: need to add a member variable to store the symbol's scope
    Symbol(std::string name, TypePtr type) : name(name), type(type) {}
    static SymbolPtr create(std::string name, TypePtr type) {
        return std::make_shared<Symbol>(name, type);
    }
    };
    ```

    这里的 `unique_name` 用于保存符号的唯一标识符，在实现中需要保证每个符号的 `unique_name` 字段都是不同的。具体而言，在你将一个新的符号插入符号表时，需要修改 `Symbol` 类的 `unique_name` 字段：对于局部变量和数组，为其生成一个唯一的 `unique_name`；对于全局变量和全局函数，直接使用其名称作为唯一标识符即可。为了方便后续 IR 生成时可能需要变量所在的 Scope 语义信息，你也可以在语义分析时将每个变量的 Scope 信息存在 Symbol 类中。

    ??? example "一个重名符号的例子"

        例如，对于以下代码：

        ```c linenums="1"
        int x = 1;
        {
            int x = 2;
        }
        ```

        在第 1 行和第 3 行中的 `x` 是不同的符号，因此它们对应的 `Symbol` 对象应该是不同的，`Symbol.unique_name` 也应该是不一样的。这一步其实是为了重命名变量，这样在后续的编译流程中就不需要考虑作用域的问题了。

    对于 `SymbolTable` 类，主要功能是管理符号的添加、查找以及作用域的管理。`SymbolTable` 类提供以下几个函数：

    - `SymbolPtr add_symbol(std::string name, TypePtr type);`：向符号表中添加一个符号，并返回新创建的符号。
    - `SymbolPtr find_symbol(std::string name, bool in_current_scope = false);`：在符号表中查找名为 `name` 的符号，如果找到则返回该符号，否则返回空指针。如果 `is_current_scope` 为 `true`，则只在当前 Scope 中查找。
    - `void enter_scope();` 和 `void exit_scope();`：进入和退出一个新的作用域。

    你需要实现这些函数，以确保符号的唯一性和作用域管理。更多细节请参考 `semantic/symbol_table.hpp` 和 `semantic/symbol_table.cpp` 中的注释。

    ??? note "OCaml 中的符号表"

        OCaml 模板在 `semantic/env.ml` 中提供了一个函数式风格的作用域符号表框架。其类型定义为 `Map` 的列表（栈），每一层 `Map` 对应一个作用域：

        ```ocaml
        module SMap = Map.Make (String)
        type 'a t = 'a SMap.t list
        let empty : 'a t = [ SMap.empty ]
        ```

        你需要实现以下 5 个函数（每个函数的实现通常只需要 1–3 行）：

        - `enter_scope`：进入新作用域（压入一个空 Map）
        - `exit_scope`：退出当前作用域（弹出栈顶 Map）
        - `add`：在当前作用域中添加绑定
        - `find`：从内到外逐层查找符号
        - `find_current`：仅在当前作用域中查找（用于检测重定义）

        与 C++ 模板中 `SymbolTable` 类需要自行设计内部数据结构不同，OCaml 模板已确定了 `Map` 列表的设计，你只需要基于这个数据结构实现上述操作即可。

        在 `check.ml` 中，符号表存储的值类型为：

        ```ocaml
        type sym_info = { ty : Types.ty; is_const : bool }
        type env = sym_info Env.t
        ```

        与 C++ 模板中通过 AST 节点的 `symbol` 成员保存符号不同，OCaml 模板采用纯函数式风格——`env` 作为参数在各检查函数之间传递，无需修改 AST 节点。

## 类型检查

所谓类型，就是一组值构成的集合，对于这组集合我们可以进行其专属的操作，例如整型的加减乘除，数组的访问等。
当我们在某组值上尝试去执行其不支持的操作时，类型错误就产生了。而类型检查，就是检查我们对这些类型的操作有没有错误。

```c
void f() {
    return;
}
int a;
int c = a / f();
```

例如上述代码， 你需要发现得到 `f()` 返回值是 `void`， 是无法进行除法的，也就产生了类型错误。这时你会发现变量绑定是十分重要的，如果碰到了使用某一变量如 `a = b + c` 时我们怎么知道 `b` 的类型呢？答案是使用我们上面介绍的符号表，通过查找符号表，我们就可以知道 `b` 的类型了。除此之外，函数调用的参数类型，个数和返回类型等也都是你应该检查的.

!!! question
    很多静态类型语言允许开发者不需要显式的指定变量的类型而交由编译器来推断，例如 C++ 中的 `auto a = 1;`、Rust 中的 `let a = 1;` 等等。感兴趣的同学可以想想该如何实现呢？

    如果你额外实现了这个功能，可以提交至 [Bonus 2: 支持更多语言特性](../bonus/extra.md)。

### 类型检查实现

与打印语法树和符号表的建立一样，类型检查也是遍历语法树的过程。对于有类型的语法树节点（如 `Exp` 节点），我们可以自然地将该节点的类型作为返回值，继续给父节点使用。例如，对于赋值语句你可以这么写：

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

其中的 `type_check` 函数负责的就是检查某个表达式的类型是否正确，并返回其类型。`table` 是我们的符号表，显然符号表的建立和类型检查可以在同一个遍历过程中完成。

??? note "模板代码中的类型检查"

    为了模块化，我们设计一个 `TypeChecker` 类，用于遍历语法树并进行类型检查。它的核心是根据 AST 节点类型调用对应的 `check` 函数进行检查。在 `main.cpp` 中，我们只需要在 `main` 函数中创建一个 `TypeChecker` 对象，然后调用 `check` 函数即可。

    在 `semantic/type_checker.hpp` 中，我们定义了 `TypeChecker` 类：

    ```cpp title="src/semantic/type_checker.hpp"
    class TypeChecker {
    public:
    TypeChecker();

    TypePtr check(AST::NodePtr node);

    private:
    /// @brief The symbol table
    SymbolTable symbol_table;

    TypePtr checkIntConst(AST::IntConstPtr node);
    TypePtr checkLVal(AST::LValPtr node);
    // ... 对每种 AST 节点类型定义一个 check 函数 ... //
    };
    ```

    `TypeChecker` 需要初始化符号表并在构造函数中添加内置函数。类型检查的核心函数是 `check`，它会根据 AST 节点的类型调用对应的 `check` 函数。这里同样使用了 C++ RTTI 来判断 AST 节点的类型，并且为了方便，我们在 `check` 函数中使用了一个宏来简化代码：

    ```cpp title="src/semantic/type_checker.hpp"
    TypePtr TypeChecker::check(AST::NodePtr node) {
    #define CHECK_NODE(type)                                     \
    if (auto n = std::dynamic_pointer_cast<AST::type>(node)) { \
        return check##type(n);                                   \
    }

    CHECK_NODE(CompUnit)
    CHECK_NODE(FuncDef)
    // ... 添加其他 AST 节点类型 ... //

    #warning Add more AST node types if needed

    #undef CHECK_NODE

    ASSERT(false, "Unknown AST node type " + node->to_string() +
                        " in type checking at line " +
                        std::to_string(node->lineno));
    }
    ```

    为了存储后续步骤所需要的语义信息信息，我们在 AST 的 `VarDef` `LVal` `FuncDef` `FuncCall` 节点中添加一个 `SymbolPtr symbol` 成员，用于保存符号表中的对应符号。例如，对于 `VarDef` 节点，我们添加了一个 `symbol` 成员：

    ```cpp hl_lines="4" title="src/ast/tree.hpp"
    class VarDef : public Node {
    public:
    std::string ident;
    SymbolPtr symbol;
    VarDef(char const *ident) : ident(ident) {}
    std::string to_string() override { return "VarDef <ident: " + ident + ">"; }
    };
    ```

    ??? tip "更进一步的解耦方式"
        在设计编译器时，为了尽可能将整个项目模块化，每一个阶段都应该不修改上一阶段的产物，而是生成下一阶段所需的新的产物。比如，在语义分析阶段，我们最好使用语法分析后的语法树进行符号表的建立和类型检查，并且生成一棵信息更丰富的语法树，也就是 Typed AST。但是，由于我们只需要构建一个简单的编译器，我们这里简化了这个过程，选择直接在 AST Node 上储存对应的 Type 信息。

    在 `TypeChecker` 的对应节点的 `check` 函数中，你需要根据 AST 节点的类型进行类型检查，并将对应的符号保存在 `node->symbol` 中。例如，对于 `LVal` 节点，你需要将 `node->symbol` 设置为在符号表中查询到的对应符号：

    ```cpp title="src/semantic/type_checker.cpp"
    TypePtr TypeChecker::checkLVal(AST::LValPtr node) {
    // 你需要在这里查找符号表，判断变量是否被定义过
    // 根据符号表中的信息设置 LVal 的类型
    // 若变量未定义，你需要报错
    // 否则，将符号表中的 symbol 挂到 LVal 节点上
    // 如果 LVal 是数组，你还需要根据下标索引来设置 LVal 的类型

    #warning Not implemented: TypeChecker::checkLVal
    // 你需要返回 LVal 的类型
    return nullptr;
    }
    ```

    需要特别注意的是，由于我们都使用了 `std::shared_ptr<Type>` 来储存类型信息，所以在比较两个类型是否相等时，你需要使用上面提到的 `compatible` 函数，而不是直接使用 `==` 或 `!=` 运算符。

    你需要自行实现其他 `check` 函数，并针对你在完成 Lab 1 时新增的 AST 节点实现对应的 `check` 函数，完成类型检查的功能。更多细节请参考 `semantic/type_checker.hpp` 和 `semantic/type_checker.cpp` 中的注释。

    ??? note "OCaml 中的类型检查"

        **类型定义**

        OCaml 模板的类型系统定义在 `semantic/types.ml` 中，使用 ADT 表示类型（对应 C++ 模板中的 `Type` 类层次结构）：

        ```ocaml
        type ty =
          | TInt | TVoid
          | TArray of ty * int    (* 元素类型, 大小 *)
          | TFunc of ty * ty list (* 返回类型, 参数类型列表 *)
          | TPtr of ty            (* 指针, 用于数组参数 *)
        ```

        模板已提供以下辅助函数：

        - `equal`：结构类型相等判断（对应 C++ 中的 `compatible` 方法，已实现）
        - `is_int`、`is_array`、`is_ptr`、`is_subscriptable`、`is_func`：类型谓词
        - `elem_type`：获取数组/指针的元素类型
        - `to_string`：类型的字符串表示

        你需要扩展 `TArray` 的使用以支持数组语义检查。

        **类型检查骨架**

        类型检查在 `semantic/check.ml` 中。与 C++ 模板通过 `dynamic_cast` 宏分派到各个 `check*` 函数不同，OCaml 利用模式匹配直接对 AST 节点进行分派。模板已提供遍历骨架——各检查函数能走遍整棵 AST 但不进行任何实际验证，你需要在标注了 `TODO` 的位置补充语义检查逻辑。

        入口函数 `check_comp_unit` 遍历编译单元中的所有项：

        ```ocaml
        let check_comp_unit (cu : comp_unit node) : unit =
          let env = Env.empty in
          (* TODO: register built-in functions read and write *)
          ignore (List.fold_left (fun env item ->
            match item with
            | FuncDef f -> check_func_def env f
            | GlobalDecl _d -> env (* TODO: check global variable declaration *)
          ) env cu.desc.items)
        ```

        表达式检查函数 `check_expr` 对每种表达式类型都有对应的 match 分支，当前统一返回 `TInt` 作为占位：

        ```ocaml
        let rec check_expr (_env : env) (_ret_ty : Types.ty) (e : expr node) : Types.ty =
          match e.desc with
          | IntConst _ -> Types.TInt
          | LVal _lv ->
            (* TODO: look up lval in symbol table, check subscripts if array *)
            Types.TInt
          | BinaryExp (_, lhs, rhs) ->
            let _lty = check_expr _env _ret_ty lhs in
            let _rty = check_expr _env _ret_ty rhs in
            (* TODO: validate both operand types are int *)
            Types.TInt
          (* ... 其余分支类似 ... *)
        ```

        你需要实现的主要工作包括：

        1. 在 `check_comp_unit` 中注册内置函数 `read` 和 `write`，处理全局变量声明
        2. 实现 `check_func_def`：检查重定义、注册函数类型、创建新作用域、添加参数、检查函数体
        3. 在 `check_expr` 中实现符号查找（`LVal`）、函数调用检查（`FuncCall`）、操作数类型验证
        4. 在 `check_stmt` 中实现作用域管理（`Block`）、类型兼容性检查（`Assign`）、返回类型检查（`Return`）、变量声明处理（`Decl`）
        5. 实现 `eval_const_expr`（常量表达式求值）、`build_array_type`、`build_param_type` 等辅助函数

        错误码与 C++ 模板完全一致（定义在 `check.ml` 顶部），使用 `raise (Semantic_error code)` 报错，例如：

        ```ocaml
        let error code = raise (Semantic_error code)
        (* 使用方式：error err_undeclared *)
        ```

### 数组初始化检查

除了上面两种经典的检查外，我们还要求检查数组初始化时的元素个数是否超出数组大小。例如：

```c
int a[2][3][4] = {1, 2, 3, 4, {5}, {9}, 13, 14, 15, 16, {17}, {21}, 25};
// [0][0][0] = 1,  [0][0][1] = 2,  [0][0][2] = 3,  [0][0][3] = 4
// [0][1][0] = 5,  [0][1][1] = 0,  [0][1][2] = 0,  [0][1][3] = 0
// [0][2][0] = 9,  [0][2][1] = 0,  [0][2][2] = 0,  [0][2][3] = 0
// [1][0][0] = 13, [1][0][1] = 14, [1][0][2] = 15, [1][0][3] = 16
// [1][1][0] = 17, [1][1][1] = 0,  [1][1][2] = 0,  [1][1][3] = 0
// [1][2][0] = 21, [1][2][1] = 0,  [1][2][2] = 0,  [1][2][3] = 0
/* 25: extra element in array initializer */
```

在数组 `a` 的初始化中，我们发现了超出数组大小的元素 25，这时你应该报错。

!!! note "这一检查实际上要求你能分析出填充完成后的初值列表，为后续代码生成做准备。"

附录：SysY 语言规范中的 [变量定义](../appendix/sysy.md#_6) 部分提供了数组初始化的规范。需要注意的是，对于多维数组的初始化，我们采用 C 语言的初始化列表规范：

1. 在初值列表中，可以直接使用标量进行横跨任意维度的初始化：
    ```c
    int a[2][2] = {1, 2, 3, 4};
    ```
2. 在抵达初值列表结尾时，如果**对应**数组仍未被填满，则填 0 直到**对应**数组被填满：
    ```c
    int a[2][2] = {1}; // {1, 0, 0, 0}. {1} for int[2][2].
    ```
3. 在初值列表内，可以用嵌套的初值列表对子数组进行初始化：
    ```c
    int a[3][2][2] = {1, 2, {3, 4}, {1, 2, 3, 4}， {1, 2, 3, 4}}; 
    //                      int[2],   int[2][2],     int[2][2] 
    //                  int[2][2],    int[2][2],     int[2][2]
    ```
4. 遇到嵌套的初值列表时，根据已经填充的元素数决定该初值列表对应的**子**数组（不包括自身），并尽量选择更大的子数组：

    ```c
    int a[3][2][2] = {{1        }, 5, 6, {7  }, {9        }};
    //               { 1, 0, 0, 0, 5, 6,  7, 0,  9, 0, 0, 0}. 
    // {1} for int[2][2]. {7} for int[2], {9} for int[2][2].
    ```
    这里遇到 `{1}` 时，已经填充了 0 个元素，所以对应初始化 `int[2][2]`。 遇到 `{7}` 时，已经填充了 6 个元素，所以对应初始化 `int[2]`。 而遇到 `{9}` 时，已经填充了 8 个元素，所以对应初始化 `int[2][2]`。准确来说，已填充的元素数要能被将要填充的子数组的总元素数整除，在此基础上选择可以填充的最大子数组。

    如果遇到嵌套的初值列表时，无法匹配上任何子数组，即已填充元素数无法被最高维度的长度整除，那么说明此处只能用标量初始化，不能在这里使用嵌套列表进行初始化，这时应该报错：
    ```c
    int a[2][2] = {1, {2, 3}, 4}; 
    //                    ^ Error：excess elements in scalar initializer
    ```
    这里遇到 `{2, 3}` 时，只填充了一个元素，我们还在对第一个 `int[2]` 的初始化中，因为此时也没有遇到任意初值列表的结尾，也不能填 0，所以应该报错。

5. 数组初始化不应该超出数组大小：
    ```c
    int a[2][2] = {1, 2, {3}, 4}; 
    //                        ^ Error: excess elements in array initializer
    ```

??? info "与 C 的初始化的区别"
    虽然大体上参考了 C 的初始化规范，但是实验中的初始化规范与 C 的初始化规范并不完全一致，具体表现为：

    - C 允许使用 `{}` / `{ expression }` 对标量进行初始化，而我们不允许。
    - 由于上一条差异，在嵌套列表中，我们默认花括号只能对应初始化子数组，而不能对应初始化标量。这在 C 中是允许的，尽管可能会有 warning， 如 `{1}` 会警告多余的花括号，`{1, 2}` 对应标量的时候会警告多余的初值并忽略 `2` 。在实验中，我们都视作错误。
    - 类似的，其实初始化超过数组大小在 C 中也往往体现为 warning，而我们在实验中视作错误。

??? example "更多例子"
    ```c
    int a[2][3][4] = {1, 2, 3, 4, {5}, {9, 10}, {13}, 25};
    // [0][0][0] = 1,  [0][0][1] = 2,  [0][0][2] = 3,  [0][0][3] = 4
    /* when {5} is read, 4 elements have been filled for array[2][3][4]
       as 4 % 4 == 0 but 4 % (3*4) != 0, use {5} to fill next array[4] */
    // [0][1][0] = 5,  [0][1][1] = 0,  [0][1][2] = 0,  [0][1][3] = 0
    /* when {9, 10} is read, 8 elements have been filled for array[2][3][4]
       as 8 % 4 == 0 but 8 % (3*4) != 0, use {9, 10} to fill next array[4] */
    // [0][2][0] = 9,  [0][2][1] = 10, [0][2][2] = 0,  [0][2][3] = 0
    /* when {13} is read, 12 elements have been filled for array[2][3][4]
       as 12 % (3*4) == 0, use {13} to fill next array[3][4]  */
    // [1][0][0] = 13, [1][0][1] = 0,  [1][0][2] = 0,  [1][0][3] = 0
    // [1][1][0] = 0,  [1][1][1] = 0,  [1][1][2] = 0,  [1][1][3] = 0
    // [1][2][0] = 0,  [1][2][1] = 0,  [1][2][2] = 0,  [1][2][3] = 0
    /* 25: extra element in array initializer */
    ```
    
    ```c
    int a[2][3][4] = {{{1, 2}, {5}}, {{13}, {17}}, 25};
    /* when {{1, 2}, {5}} is read, no element has been filled for array[2][3][4]
       though 0 % (2*3*4) == 0, only use {{1, 2}, {5}} to fill next array[3][4] */
    /* when {1, 2} is read, no element has been filled for array[3][4]
       though 0 % (3*4) == 0, only use {1, 2} to fill next array[4] */
    // [0][0][0] = 1,  [0][0][1] = 2,  [0][0][2] = 0,  [0][0][3] = 0
    /* when {5} is read, 4 elements have been filled for array[3][4]
       as 4 % 4 == 0 but 4 % (3*4) != 0, use {5} to fill next array[4] */
    // [0][1][0] = 5,  [0][1][1] = 0,  [0][1][2] = 0,  [0][1][3] = 0
    /* as {1, 2} and {5} cannot fill the whole array[3][4],
       fill the rest with 0 */
    // [0][2][0] = 0,  [0][2][1] = 0,  [0][2][2] = 0,  [0][2][3] = 0
    /* when {{13}, {17}} is read, 12 elements have been filled for array[2][3][4]
       as 12 % (3*4) == 0, use {{13}, {17}} to fill next array[3][4]  */
    /* when {13} is read, no element has been filled for array[3][4]
       fill the rest with 0 */
    // [1][2][0] = 0,  [1][2][1] = 0,  [1][2][2] = 0,  [1][2][3] = 0
    /* 25: extra element in array initializer */
    ```

<!-- 如果你对上述表述仍存在疑惑，或者对 gcc 初始化列表的其他细节感兴趣，可以参考[北京大学编译原理](https://pku-minic.github.io/online-doc/#/lv9-array/nd-array)中对应章节，或者自行查找 gcc 的相关实现。 -->

如果你仍有疑惑，可以在 [Compiler Explorer](https://godbolt.org/) 上多尝试一些例子，看看编译器是如何实现数组初始化的。当然也可以自行查找 gcc 的相关实现。

## 你的任务

你的任务就是在实验 1 所构建的语法树基础上完成上述语义分析，对语法树进行遍历以进行符号表的相关操作以及类型的构造与检查。对于违反语义的程序，你的编译器需要检测出错误，打印相关报错信息（格式不限），并以特定的错误码（Exit Code）退出程序。具体的错误码定义请参考[测试](#_9)一节。以下是程序需要满足的语义约束：

1. 符号表相关：
    - 禁止使用未定义的符号。符号的作用域从其声明点开始，持续到词法作用域的结尾（块结尾/文件结尾）。
    - 同一作用域内禁止出现重名符号，但允许内部作用域遮蔽外部同名符号
2. 类型检查相关：
    - 在赋值/初始化/参数传递中，源表达式与目标对象的类型需要严格一致
    - 所有运算符的操作数都必须是整形表达式
    - 函数返回值的类型应与函数声明中的返回类型保持严格一致
    - 函数调用中的被调用对象必须是函数，且参数个数、类型应与函数声明一致
    - 只能在数组上使用整形表达式进行索引，并且索引维度不应超过数组维度
    - 赋值操作必须作用于合法的左值表达式
    - 控制流语句的条件表达式必须是整形表达式
3. 初始化相关：
    - 数组初始化时的初值列表必须匹配
    - 整形变量初始化时的初值只能是整形标量

!!! tip "别忘了 `read` 和 `write` 这两个编译器的内置函数，用户可以直接调用，你也需要进行对应的类型检查。"

同实验 1，你的编译器必须支持一个命令行参数的情形，程序名必须为 `compiler`，且在仓库根目录下。你的编译器应该能够按照以下方式运行：

```console
$ ./compiler <input_file>
```

该程序必须接受一个输入的源代码文件名作为参数，我们只会使用这种方式来测试你的编译器。

## 测试

运行以下命令来测试你的编译器：

```console
$ python3 sp26-tests/test.py lab2 .
```

在实验 2 中，我们提供了以下两类测试样例：

- 如 `tests/lab2/main.sy` 所示，源文件中的语义没有错误，你的编译器应该能够正常退出。

    ```c
    int main(){
        return 0;
    }
    ```

- 如 `tests/lab2/unmatch_call_num.sy` 所示，源文件中的函数形参与实参不匹配，你的编译器应该能够检测出错误并报错后以返回值 `13` 退出。

    ```c
    // Return 13 ArgNumMismatch - Semantic Error at Line 8: Function arguments not matched

    int f (int i) {
        return i;
    }

    int main () {
        int a = f(1,2);
        return 0;
    }
    ```

我们的测试完全采用输入输出形式，即对于符合语义的源代码，你的程序正常运行后正常退出（返回 `0`），对于存在语义错误的源代码，你的程序应该能够检测出错误，并返回对应的错误码。我们对标准输出/标准错误输出中的报错文本格式没有严格要求，但强烈建议你打印有意义的错误信息以便调试。测试文件中的报错注释供你参考。我们约定了以下错误码：


| Code | Identifier             | 判定规则                                                                 | Message |
|------|------------------------|-------------------------------------------------------------------------|---------|
| 0    | `OK`                   | 程序无错误，编译成功退出。                                               | `pass` |
| 1    | `SyntaxError`          | 词法/语法阶段无法构造合法 AST。                                          | `syntax error` |
| 2    | `NotAssignable`        | **LHS 是数组对象**，且出现在赋值语句/整体初始化的赋值目标位置。          | `array type is not assignable` |
| 3    | `ExcessInitializers`   | initializer list 元素数量超出目标数组/标量可容纳的数量。                 | `excess elements in array/scalar initializer [-Wexcess-initializers]` |
| 4    | `IncompatibleConversion` | 除了返回值为 `2` 的情况外，所有赋值/初始化/参数传递的类型不兼容。               | `incompatible conversion/assignment` |
| 5    | `InvalidOperands`      | 运算符与操作数类型不匹配（如 `{1} + 1`、指针 + 浮点）。                      | `invalid operands to unary/binary expression` |
| 6    | `NotSubscriptable`     | 下标操作中，被下标对象不是数组或指针。                                   | `subscripted value is not an array or pointer` |
| 7    | `ReturnMismatch`       | `return expr` 与函数返回类型不兼容，或非 `void` 函数使用 `return;`。    | `return type mismatch` |
| 8    | `NotIntegerSubscript`  | 下标表达式中，下标不是整数类型（整型提升后仍不满足）。                   | `array subscript is not an integer` |
| 9    | `NotCallable`          | 调用表达式中，被调用对象不是函数或函数指针。                             | `called object is not a function or function pointer` |
| 10   | `Undeclared`           | 使用了未声明的标识符。                                                   | `use of undeclared identifier` |
| 11   | `Redefinition`         | 同一作用域内对同一标识符进行不兼容的重复定义。                           | `redefinition of identifier` |
| 12   | `InvalidInitializer`   | 目标是数组/聚合类型，但 RHS 不是 initializer list（如 `int a[2] = 0;`）。 | `array initializer must be an initializer list` |
| 13   | `ArityMismatch`        | 函数调用时，实参与形参数量不一致。                                       | `function arguments number not matched` |

???+ tip "特别说明：`2` vs `4`"
    - **2 NotAssignable**：只要 **LHS 是数组对象且被赋值** → 一律返回 `2`。  
    - **4 IncompatibleConversion**：所有其他类型不兼容的情况（不涉及“数组整体赋值”）都返回 `4`。  

???+ abstract "错误判定优先级（防冲突指南）"
    当一段代码同时触发多个语义错误时，请按照以下优先级（由高到低）进行判定并返回第一个遇到的错误码：

    1. **Parse 阶段:** 优先报 `1 SyntaxError`。
    2. **Name / Scope 阶段:** 优先检查 `10 Undeclared` 和 `11 Redefinition`。
    3. **函数调用检查:** 依次检查 `9 NotCallable`，然后是 `13 ArityMismatch`，最后逐个参数检查 `4 IncompatibleConversion`。
    4. **下标操作检查:** 依次检查 `6 NotSubscriptable`，然后是 `8 NotIntegerSubscript`。
    5. **运算表达式检查:** 检查 `5 InvalidOperands`。
    6. **赋值与初始化检查:** - 首先检查 `2 NotAssignable`（数组整体赋值禁令）。
        * 若目标是数组，依次检查 `12 InvalidInitializer`（形式错误）和 `3 ExcessInitializers`（数量超额）。
        * 若是其他类型不兼容，检查 `4 IncompatibleConversion`。


    7. **Return 语句检查:** 检查 `7 ReturnMismatch`。

## 实验提交

你需要将作为 Lab 2 评分的代码放到 `lab2` 分支中，并提交一个实验报告，**不得超过 3 页**，内容包括:

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
        - 上述 AI 工具使用的完整记录可存放于仓库的 `reports/appends/lab2` 文件夹中，并在实验报告中简要说明。

!!! danger "实验报告存在以下情况的，将由助教现场验收，并相应扣分"
    - 若实验报告中**缺乏实现思路**或**未真实描述 AI 工具使用情况**，将由助教现场验收，并相应扣分。
    - 实验报告**禁止使用 AI**，包括但不仅限于直接生成报告内容、润色等。提交的报告将会使用 AI 检测工具进行检测。若 AI 生成的内容占比过高，或者检测出明显的 AI 生成痕迹，将由助教现场验收，并相应扣分。

我们只接受 PDF 格式的实验报告。你需要将报告放在仓库的 `reports/lab2.pdf` 中。