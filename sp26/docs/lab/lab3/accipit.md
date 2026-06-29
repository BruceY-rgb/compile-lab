# Accipit IR

??? note "模板代码使用指南"

    如果你选用 Accipit IR 作为中间代码，Lab 3 部分的模板代码位于仓库中的 `template/lab3-accipit` 分支下，你可以将其合并到自己的 `lab3` 分支中：
    
    ```console
    $ git merge upstream/template/lab3-accipit
    ```

    如果出现冲突，Git 会提示你解决冲突，手动编辑冲突的文件并在解决后执行：

    ```console
    $ git add <conflict-file>
    $ git commit
    ```

    合并完成后，你可以先把模板代码推送到远程仓库：

    ```console
    $ git push origin lab3-accipit
    ```

    然后开始修改模板代码，完成你的编译器。
    
    Lab 3 框架的文件结构如下，高亮部分为新增和改动的文件：

    ```text hl_lines="6-10 13"
    src
    ├── ast
    │   ├── tree.cpp
    │   └── tree.hpp
    ├── common.hpp
    ├── ir
    │   ├── ir.def // 辅助宏
    │   ├── ir.hpp // IR 结构定义
    │   ├── ir_translator.cpp // IR 翻译器
    │   └── ir_translator.hpp
    ├── lexer
    │   └── lexer.l
    ├── main.cpp
    ├── parser
    │   └── parser.y
    └── semantic
        ├── symbol_table.cpp
        ├── symbol_table.hpp
        ├── type.cpp
        ├── type.hpp
        ├── type_checker.cpp
        └── type_checker.hpp
    ```

    ??? note "OCaml 中的 Accipit IR"

        OCaml 模板的 Accipit IR 定义在 `ir/accipit_ir.ml` 中，使用 ADT 表示 SSA 指令：

        ```ocaml
        type rhs = BinExpr of binop * operand * operand
                 | Alloca of int | Load of operand | Store of operand * operand
                 | Call of string * operand list
                 (* TODO: 添加 Offset 等 *)
        type terminator = Jmp of string | Ret of operand
                 (* TODO: 添加 Br 条件分支 *)
        type binop = Add | Sub
                 (* TODO: 添加更多运算 *)
        ```

        模板已给出所有已定义变体的打印函数。你新增变体时需要补充对应的打印 case。

        翻译函数在 `ir/translate_accipit.ml` 中。模板使用 `bb_builder` 可变记录管理基本块的构建，并给出了 `IntConst` 和 `BinaryExp(Add)` 的翻译示例。其余翻译函数标注为 `TODO`。

!!! warning "使用最新的模板代码"
    学期初版本的模板代码存在一些 bug，因此强烈建议同学们确认自己基于的模板代码是最新的。实验模板在实验正式开始前都可能有更新，在正式开始后一般不再更新，如果有更新会钉钉通知大家。

## 中间代码的定义

我们先看一个 SysY 对应的 Accipit 代码，直观感受一下 Accipit 长什么样子。

源码：
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
参考中间代码：
```rust linenums="1"
fn @factorial(#n: i32) -> i32 {
%Lentry:
    // create a slot of i32 type as the space of the return value.
    let %ret.addr = alloca i32, 1
    // create a slot for function parameter n.
    let %n.addr = alloca i32, 1
    // store function parameter on the stack, %4 is void(unit).
    let %4 = store #n, %n.addr
    // create a slot for local variable ans, uninitialized.
    let %ans.addr = alloca i32, 1
    // [ if (n == 1) ]
    // when we need #n, you just read it from %n.addr.
    let %6 = load %n.addr
    let %cmp = eq %6, 1
    br %cmp, label %Ltrue, label %Lfalse
%Ltrue:
    // [ return 1; ]
    let %10 = store 1, %ret.addr
    jmp label %Lret
%Lfalse:
    // [ n - 1 ]
    let %13 = load %n.addr
    let %14 = sub %13, 1
    // [ factorial(n - 1) ]
    let %res = call @factorial, %14
    // [ n ]
    let %16 = load %n.addr
    // [ n * factorial(n - 1) ]
    let %17 = mul %16, %res
    // [ int ans = n * factorial(n - 1); ]
    let %18 = store %17, %ans.addr
    // [ return ans; ]
    // we should first read value from `%ans.addr` and then
    // store it to `%ret.addr`.
    let %19 = load %ans.addr
    let %20 = store %19, %ret.addr
    jmp label %Lret
%Lret:
    // load return value from %ret.addr
    let %ret.val = load %ret.addr
    ret %ret.val
}
```

结合代码注释基本可以理解这段中间代码的含义。通过观察我们不难发现：

- Accipit IR 是有类型的。
- Accipit IR 的符号被分为 `@`（全局）`#`（参数）`%`（局部）三类，前者的作用与是全局的，而后二者的作用域仅限于所在的函数，同一作用域内不应存在同名的符号。
- 变量可以是一个值（`%4`, `%6`, `%ret.val`），也可以是一个指针（`%ret.addr`, `%ans.addr`）。但变量的命名可以是任意的，清楚即可。
- Accipit IR 使用了基本块来组织代码，每个基本块都有一个入口标签和一个出口跳转语句，基本块内的指令顺序执行。基本块在后续的编译优化中起到重要作用。
- 每个变量都只在一处被赋值（当然此处代码可以执行多次），这是静态单赋值（Single Static Assignment, SSA）的重要表现形式。事实上，Accipit IR 是类似 LLVM IR 形式的 Partial SSA IR。SSA 对后续许多编译优化同样起着不可或缺的作用，有关编译优化的更多内容请参考[Bonus3](../../bonus/optimization.md)。

为实现 SSA， `alloca` `load` `store` 三条指令起着重要的作用。举例来说，`factorial` 中有两处 `return`，如果没有这三条指令，我们只能定义一个变量 `%ret`，并在两处分别为其赋值，而借助这三条指令，我们只需要在两处分别“使用”其地址，从而隐性的对其完成赋值，而在程序中出现的任何一个变量都没有被赋值两次。这两处赋值在最后一个基本块中的 `load` 语句被汇合起来，并被用作了最后的返回值。

我们通过下表简要地给出 Accipit IR 指令的定义供你参考，更具体的规范请参阅 [Accipit IR 规范](../../appendix/accipit-spec.md)。

| 类型 | 格式 | 说明 |
|---------------|-----------------------------------------------------------|----------------------------------|
| valuebinding  | `let %x = binop %y, %z`                                      | 将 `%y` 和 `%z` 的双目运算结果存放到 `%x` 中                    |
|               | `let %x = call @fun1, %arg1 ...`                                | 使用 `%arg1`... 等参数调用函数 `@fun1`，结果存入 `%x` 中                  |
|               | `let %addr = offset type, %array, [%a < 3], [%b < 4], [%c < 5]`   | 从 `%array[3][4][5]` 中取指向 `%array[%a][%b][%c]` 的指针， 结果存入 `%addr` 中        |
|               | `let %addr = alloca type, n`                                   | 在栈上开辟长度为 `n` 的 `type` 数组，并将数组的首地址存入 `%addr` 中 |
|               | `let %x = load %addr`                                       | 从 `%addr` 地址加载数据存入 `%x` 中 |
|               | `let %x = store value, %addr`                               | 把 `value` 存入 `%addr` 中，返回值 `%x` 可以忽略|
| terminator    | `br value, label %L1, label %L2`                            | 根据 `i32` 类型的 `value` 的值条件跳转，若为 `true` 则跳转到 `%L1`，否则跳转到 `%L2` |
|               | `jmp label %L1`                                            | 无条件跳转到 `%L1` 处                       |
|               | `ret value`                                               | 函数返回，返回值为 `value` |
| function      | `fn @f(#p1: t1, #p2: t2...) -> t {basic block list}`          |定义函数 `@f`，参数为 `t1` 类型的 `#p1`，`t2` 类型的 `#p2`... 返回类型为 `t`, 函数体是一个 `basic block` 列表 |
|               | `%L1:`                                                     | 定义标签 `%L1`                          |
| global        | `@x: region t1, n`                                       | 定义全局变量 `@x`，`@x` 指向长度为 `n` 的 `type` 数组，进行零初始化。 `i32` 类型全局变量用长度为 1 的数组表示|
|               | `@x: region t1, n = [v1, v2 ... vn]`                            | 定义全局变量 `@x`，`@x` 指向长度为 `n` 的 `type` 数组，并指定初始化值 |

我们支持的 `binop` 双目运算如下表所示：

| 算数运算符 | 说明 | 关系运算符 | 说明   | 
|-------|----|-------|------|
| `add`     | 加法 | gt     | 大于   |
| sub     | 减法 | ge    | 大于等于 |
| mul     | 乘法 | lt     | 小于   |
| div     | 除法 | le    | 小于等于 |
| rem     | 模  | ne    | 不等于  | 
|      |   | eq    | 等于   |

Accipit IR 并不直接支持单目运算符，但你一定可以方便的把它们用双目运算符表示出来。

你应该也注意到了 `and/or` 等逻辑运算符 Accipit IR 同样也不支持，这是因为[SysY语言规范](../../appendix/sysy.md)已经限定了逻辑运算符只在条件语句中出现，而对于条件语句的翻译我们采用短路的处理，`and/or` 等运算符最终会被翻译为不同基本块间的跳转关系而非具体的运算语句，详情请看后文的[短路](#_7)。

对于 IO 操作，你可以像调用其他函数一样调用 `@write` 来输出一个 `"%d\n"` 到 `stdout`，使用 `@read` 来从 `stdin` 读一个 `"%d"`。这些函数是可以不用声明直接使用的，你可以在编译器实现中将他们默认加入符号表中。

??? example "更多 Accipit IR 程序样例"
    === "add.acc"
        ```rust linenums="1"
        fn @add(#1: i32, #2: i32) -> i32 {
        %3:
            let %4: i32 = add #1, #2
            ret %4
        }

        fn @add_plus_one(#1: i32, #2: i32) -> i32 {
        %3:
            let %4: i32 = add #1, #2
            let %5: i32 = add #4, 1
            ret %5
        }

        fn @add_ret_unit(#1: i32, #2: i32) -> () {
        %3:
            let %4: i32 = add #1, #2
            let %5: i32 = add #4, 1
            ret ()
        }

        fn @add_but_unused_bb(#a: i32, #b: i32) -> i32 {
        %3:
            let %4 = add #a, #b
            ret %4
        %dead_bb:
            let %5: i32 = add #4, 1
            ret %5
        }

        fn @add_but_direct_link_bb(#a: i32, #b: i32) -> i32 {
        %3:
            let %4 = add #a, #b
            jmp label %6
        %dead_bb:
            let %5: i32 = add #4, 1
            ret %5
        %6:
            ret %4
        }

        fn @add_with_load_store_alloca(#1: i32, #2: i32) -> i32 {
        %entry:
            let %arg.1.addr = alloca i32, 1
            let %arg.2.addr = alloca i32, 1
            let %3 = store #1, %arg.1.addr
            let %4 = store #2, %arg.2.addr
            jmp label %6
        %6:
            let %7 = load %arg.1.addr
            let %8 = load %arg.2.addr
            let %9: i32 = add %7, %8
            ret %9
        }

        fn @load_store_alloca_offset(#1: i32, #2: i32) -> i32 {
        %entry:
            let %arg.array = alloca i32, 6
            let %arg.1.addr = offset i32, %arg.array, [0 < 2], [1 < 3]
            let %arg.2.addr = offset i32, %arg.array, [1 < 2], [2 < 3]
            let %3 = store #1, %arg.1.addr
            let %4 = store #2, %arg.2.addr
            jmp label %6
        %6:
            let %8 = load %arg.2.addr
            ret %8
        }
        ```
    === "array_parameter.acc"
        ```rust  linenums="1"
        // void print_array(int a[], int len)
        // This is an optimized manually written version.
        // NOTE: The output heavily relies on the assumption that
        // multi-dimension array has a continuous memory, which is not specified in
        // our SysY standard, but rather target dependent.

        fn @print_array(#a: i32*, #len: i32) -> () {
        %entry:
            let %i.addr = alloca i32, 1
            let %0 = store 0, %i.addr
            jmp label %while_entry
        %while_entry:
            let %1 = load %i.addr
            let %4 = lt %1, #len
            br %4, label %while_body, label %while_exit
        %while_body:
            let %i.load = load %i.addr
            let %5 = offset i32, #a, [%i.load < none]
            let %6 = load %5
            let %7 = call @write, %6
            // i = i + 1
            let %9 = load %i.addr
            let %10 = add %9, 1
            let %11 = store %10, %i.addr
            jmp label %while_entry
        %while_exit:
            jmp label %return
        %return:
            ret ()
        }

        fn @main() -> i32 {
        %entry:
            // int a[4][2]
            let %a.addr = alloca i32, 8
            let %0 = offset i32, %a.addr, [0 < 4], [0 < 2]   
            let %1 = store 1, %0
            let %2 = offset i32, %a.addr, [0 < 4], [1 < 2]   
            let %3 = store 2, %2
            let %4 = offset i32, %a.addr, [1 < 4], [0 < 2]   
            let %5 = store 3, %4
            let %6 = offset i32, %a.addr, [1 < 4], [1 < 2]   
            let %7 = store 4, %6
            let %8 = offset i32, %a.addr, [2 < 4], [0 < 2]   
            let %9 = store 5, %8
            let %10 = offset i32, %a.addr, [2 < 4], [1 < 2]   
            let %11 = store 6, %10
            let %12 = offset i32, %a.addr, [3 < 4], [0 < 2]   
            let %13 = store 7, %12
            let %14 = offset i32, %a.addr, [3 < 4], [1 < 2]   
            let %15 = store 8, %14
            // [2 < 4], [0 < 2] can also be [4 < 8] if you like
            // assuming the target properties mentioned above.
            let %16 = offset i32, %a.addr, [2 < 4], [0 < 2]
            let %17 = call @print_array, %16, 2
            let %18 = offset i32, %a.addr, [1 < 4], [0 < 2]
            let %19 = call @print_array, %18, 2
            let %20 = offset i32, %a.addr, [0 < 4], [0 < 2]
            let %21 = call @print_array, %20, 2
            let %22 = offset i32, %a.addr, [3 < 4], [0 < 2]
            let %23 = call @print_array, %22, 2
            ret 0
        }
        ```
    === "global_array.acc"
        ```rust  linenums="1"
        // a is a global array with 105 i32 elements.
        // Suppose in SysY it is a multi-dimension array `int a[5][3][7]`
        @a : region i32, 105


        fn @write_global_var(#value: i32) -> () {
        %entry:
            // offset: 3 * 3 * 7 + 2 * 7 + 4 = 81
            let %0 = offset i32, @a, [3 < 5], [2 < 3], [4 < 7]
            let %1 = store #value, %0
            // offset based on previous offsets.
            // offset: 81 + 1 * 3 * 7 + 2 = 104
            let %2 = offset i32, %0, [1 < 5], [0 < 3], [2 < 7]
            let %3 = add #value, 1
            let %4 = store %3, %2
            ret ()
        }

        fn @read_global_var(#dim1: i32, #dim2: i32, #dim3: i32) -> i32 {
        %entry:
            let %0 = offset i32, @a, [#dim1 < 5], [#dim2 < 3], [#dim3 < 7]
            let %1 = load %0
            ret %1
        }

        fn @main() -> i32 {
        %entry:
            let %0 = call @write_global_var, 10
            let %1 = call @read_global_var, 3, 2, 4
            let %2 = call @read_global_var, 4, 2, 6
            let %3 = call @write, %1
            let %5 = call @write, %2
            ret 0
        }
        ```
    === "if.acc"
        ```rust  linenums="1"
        /*
        int if_ifElse_() {
          int a;
          a = 5;
          int b;
          b = 10;
          if(a == 5){
            if (b == 10) 
              a = 25;
            else 
              a = a + 15;
          }
          return (a);
        }
        */

        fn @if_ifElse_() -> i32 {
        %entry:
            let %ret.addr = alloca i32, 1
            // int a;
            let %0 = alloca i32, 1
            let %1 = store 5, %0
            // int b;
            let %2 = alloca i32, 1
            let %3 = store 10, %2
            // a == 5
            let %4 = load %0
            let %5 = eq %4, 5
            br %5, label %outer_if_true, label %outer_if_exit
        %outer_if_true:
            // b == 10
            let %6 = load %2
            let %7 = eq %6, 10
            br %7, label %inner_if_true, label %inner_if_false
        %inner_if_true:
            let %8 = store 25, %0
            jmp label %inner_if_exit
        %inner_if_false:
            let %9 = load %0
            let %10 = add %9, 15
            let %11 = store %10, %0
            jmp label %inner_if_exit
        %inner_if_exit:
            jmp label %outer_if_exit
        %outer_if_exit:
            let %12 = load %0
            let %13 = store %12, %ret.addr
            jmp label %ret_bb
        %ret_bb:
            let %14 = load %ret.addr
            ret %14
        }

        /*
        int if_if_Else() {
          int a;
          a = 5;
          int b;
          b = 10;
          if(a == 5){
            if (b == 10) 
              a = 25;
          }
          else 
              a = a + 15;
          return (a);
        }
        */

        fn @if_if_Else() -> i32 {
        %entry:
            let %ret.addr = alloca i32, 1
            // int a;
            let %0 = alloca i32, 1
            let %1 = store 5, %0
            // int b;
            let %2 = alloca i32, 1
            let %3 = store 10, %2
            // a == 5
            let %4 = load %0
            let %5 = eq %4, 5
            br %5, label %outer_if_true, label %outer_if_false
        %outer_if_true:
            // b == 10
            let %6 = load %2
            let %7 = eq %6, 10
            br %7, label %inner_if_true, label %inner_if_exit
        %inner_if_true:
            let %8 = store 25, %0
            jmp label %inner_if_exit
        %inner_if_exit:
            jmp label %outer_if_exit
        %outer_if_false:
            let %9 = load %0
            let %10 = add %9, 15
            let %11 = store %10, %0
            jmp label %outer_if_exit
        %outer_if_exit:
            let %12 = load %0
            let %13 = store %12, %ret.addr
            jmp label %ret_bb
        %ret_bb:
            let %14 = load %ret.addr
            ret %14
        }

        fn @main() -> i32 {
        %entry:
            let %0 = call @if_ifElse_
            let %1 = call @write, %0
            let %3 = call @if_if_Else
            let %4 = call @write, %3
            ret 0
        }
        ```

??? example "不同 SysY 结构对应的 Accipit IR 结构"
    === "变量赋值"
        ```c
        x = 5
        y = x * 2 + 3
        ```
        对应的中间代码可能是：
        ```c
        // Entry Block
        let %x.addr = alloca i32, 1
        let %y.addr = alloca i32, 1
        // x = 5
        let %0 = store 5, %x.addr
        // y = x * 2 + 3
        let %1 = load %x.addr
        let %2 = mul %1, 2
        let %3 = add %2, 3
        let %4 = store %3, %y.addr
        ```
    === "数组操作"
        ```c
        int a[5];
        a[2] = 1;
        a[i] = 2;
        ```
        对应的中间代码可能是：
        ```c
        // Entry Block
        let %a.addr = alloca i32, 5
        let %i.addr = alloca i32, 1
        // a[2] = 1
        let %0 = offset i32, %a.addr, [2 < 5]
        let %1 = store 1, %0
        // a[i] = 2
        let %2 = load %i.addr
        let %3 = offset i32, %a.addr, [%2 < 5]
        let %4 = store 2, %3
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
        fn @func(#a: i32, #b: i32) -> i32 {
        %entry:
            let ret.addr = alloca i32, 1
            let %a.addr = alloca i32, 1
            let %0 = store #a, %a.addr
            let %b.addr = alloca i32, 1
            let %1 = store #b, %b.addr
            let %2 = load %a.addr
            let %3 = load %b.addr
            let %4 = add %2, %3
            let %5 = store %4, %ret.addr
            jmp label %exit:
        %exit:
            let %6 = load %ret.addr
            ret %6
        }
        
        fn @main() -> i32 {
            // ...
            let %0 = load x.addr
            let %1 = load y.addr
            let %2 = call @func, %0, %1
            // ...
        }
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
            let %0 = load a.addr
            let %1 = load b.addr
            let %2 = gt %0, %1
            br %2, label %Label1, label %Label2
        %Label1:
            let %3 = store 1, b.addr
            jmp label %Label3
        %Label2:
            let %4 = store 2, b.addr
            jmp label %Label3
        %Label3:
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
        @a: region i32, 1
        @b: region i32, 5
        fn @main() -> i32 {
            // ...
            let %0 = store 2, @a
            // ...
        }
        ```

### Accipit 中的基本块

基本块是划分控制流的边界，基本块内指令有序地线性执行，控制流跳转只存在于基本块之间，这种关系使得基本块之间连成一个有向图，一般称这个有向图为控制流图 (Contorl Flow Graph，简称 CFG)。
例如：`if` 的两个分支分别翻译到两个基本块 `LTrue` 与 `LFalse`。

![CFG](../../assets/image/factorial.svg#only-light)
![CFG](../../assets/image/factorial.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

以上图的 CFG 为例,
在 `if` 分支入口前，有一个基本块作为入口，计算 `if` 条件的真假，即 `%cmp`； 
在 `if` 的两个分支结束后，控制流进行了“合并”，处理下一个语句块，进行一个无条件跳转 `br label %ret`，来到了出口基本块 `%ret`。
这是结构化控制流通常的处理思路，你可以将其类推到 `while` 循环。


### Accipit 中的局部变量

Accipit 中的变量可以是一个值，也可以是一个指针。

在从 SysY 翻译到 Accipit 的过程中，我们只会在两种情况下创建变量：

- 对于那些一经创建就不会再经历修改的变量，常见的如运算产生的结果和中间结果，我们只需要给其分配一个临时变量即可，如 `1 + 2 + 3`，就可以被翻译为：
    ```rust
    let %0 = 1 + 2
    let %1 = %0 + 3
    ```
- 对于那些会被重新赋值的局部变量，我们就不能简单的分配一个临时变量，否则我们就要在多处对其赋值，破坏SSA。此时我们就需要为其开辟一个栈上的地址，再将这个不变的地址存入变量中（如 `%a.addr`），这样后续的读写都不会导致对变量的重新赋值了，因为我们是在使用这个地址，而不是把它赋值为新的地址。
```rust
let %a.addr = alloc i32, 1
let %0 = store 1, %a.addr
let %1 = load %a.addr
let %2 = store 2, %a.addr
let %3 = load %a.addr
```

简单起见，你可以不用区分哪些是不会被重新赋值的局部变量，而直接为所有局部变量都开辟一块栈上的空间(因为他们都可能被重新赋值)，并在使用前后用 `load` `store` 进行读写。

一般来说，你需要为下图中的这些变量开辟栈上的空间：

![frame](../../assets/image/frame.svg#only-light)
![frame](../../assets/image/frame.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

而对于运算产生的结果和中间结果，你往往只需要分配一个临时变量（%1，%2）来存储他们即可，因为他们不可被重新赋值。

对于整形的函数参数，因为可能涉及多次赋值，你需要为其开辟栈上的空间。但是对于指针类型的参数，它不是可赋值的左值，所以你其实可以不为其开辟栈上的空间，而是直接使用它。

??? note "模板代码中的 IR 结构定义"

    与 AST 类似，我们在 `ir/ir.hpp` 中定义了 IR 节点的基类 `IR::Node`：

    ```cpp title="src/ir/ir.hpp"
    class Node {
    public:
        virtual std::string to_string() const = 0;
        virtual ~Node() = default; // make the class polymorphic
    };
    using NodePtr = std::shared_ptr<Node>;
    ```

    其中，`to_string` 函数用于将 IR 节点转换为对应的 IR 代码字符串。

    我们已经帮你定义好了一些基本的 IR 节点，例如 `IR::Alloc`、`IR::Call` 等等。你只需要参考已经实现的节点，添加其余的 IR 节点即可。


## 语法制导代码生成

对 Accipit 有个大致的了解后，我们可以着手把经过语义检查语法树转换成 Accipit 中间代码。
首先，简要回顾 Accipit IR 的结构，详细请看 [Accipit IR 规范](../../appendix/accipit-spec.md)：

- **Type（类型）**：包括基本类型 `i32` `()` `none` 以及指针类型、函数类型。
- **Instruction（指令）**：指令分为 value binding 和 terminator 两类。 前者主要进行数据操作，由 `let` 开头，定义一个新变量；后者主要进行控制流操作，如无条件跳转和条件跳转等。
- **Value（值）**：值包含符号 `symbol`（或者标识符 `Id` ） 和常量 `const` 两类。
- **BasicBlock（基本块）**：基本块包含开头的标签和若干线性排列的指令序列，其中最后一条指令必须是 terminator。 基本块内部的指令序列线性排列，线性执行（线性结构）；基本块之间的跳转构成图结构，表示控制流的跳转（图结构）。
- **Function（函数）**：函数的名称，类型等。
- **Module（模块）**：表示整个编译单元，包含函数和全局变量等。

翻译的基本思路是遍历语法树的节点，然后根据节点的类型生成对应的中间代码。
整个翻译的最大矛盾在于前端树结构的语法树和后端线性的汇编之间的差异，本实验的核心哲学便在于中间代码如何连接这两种迥异的代码表示形式：

- **数据依赖（Data Dependency）**：语法树以树状形式来表示各种运算，以各种可重复的变量来表征数据，而汇编的只能操作有限的物理寄存器。 中间代码需要理清表达式所使用的变量的数据来源，从而能够最终映射到寄存器操作上。 Accipit IR 规范中的 value 一定程度上表征了“数据”，即符号或常量。
- **控制流（Control Flow）**：语法树语句块是结构化的、嵌套的树形结构，并没有显式的控制流跳转；汇编是线型的，需要给不同的子语句块标记 label，并加上合适的跳转指令。 中间代码需要理清不同语句块之间的控制流跳转关系，也就是各个基本块结接成的图。

我们需要实现 translate_X 函数，X 对应表达式，语句等等。

- `translate_expr` 将表达式翻译到中端 IR 的 value。 起到跟踪数据依赖关系，完成表达式翻译到线性的指令的任务。
- `translate_stmt` 将语句块翻译到中端 IR 的 basicblock。
你需要跟踪控制流，将语句块之间的关系翻译到控制流跳转任务，插入合适的 terminator 指令。

### 表达式生成

`translate_expr` 的任务是把一个语法树上的 `expr` 节点翻译为 Accipit IR 的 `value`。我们以 `1 + 2 * f(5)` 为例：

<center>
![ExpAst](../../assets/image/exp-ast.svg#only-light)
![ExpAst](../../assets/image/exp-ast.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }
</center>

不难想象，这个表达式最后翻译出来应该长这样：

```rust
let %0 = call @f, 5 // 由 translate_expr(" f(5) ") 插入， 返回 %0 供父节点使用
let %1 = mul 2, %0 // 由 translate_expr(" 2 * f(5) ") 插入， 返回 %1 供父节点使用
let %2 = add 1, %1 // 由 translate_expr(" 1 + 2 * f(5) ") 插入， 返回 %2 供父节点使用
```

而在遍历语法树时，我们显然是先访问根节点，也就是 `+` 节点。我们在处理 `+` 节点时，应该先递归的调用 `translate_expr` 处理其下面的所有子 `expr`。在这个例子中，也就是 `1` 和 `2 * f(5)`，而每个调用要么返回一个常量（如左节点返回 `1`），要么返回一个变量存储其运算结果（如右节点返回 `%1`），最后 `+` 只需要利用这两个返回的结果构造自己的运算指令推入基本块中，并且返回储存其结果的变量（`%2`），以供 `+` 的父节点使用即可。

每个 `expr` 语句翻译完都返回一个常量或者变量供其父节点使用，这个返回值可以被继续用于父表达式的运算（如上例的 `%0` `%1`），也可以被父语句使用，如 `if` 使用其结果进行跳转，或 `ret` 用其结果作返回值等。

回顾 Accipit IR，我们定义的值（`value`）正好包括变量和常数。所以 `translate_expr` 的返回值就是一个 `value`。一个可供参考的 `translate_expr` 签名如下：


```rust
translate_expr(expr, symbol_table, current_bb) -> value
```

其中 `symbol_table` 是符号表，维护一个 `string -> symbol` 的映射，我们在遍历语法树时遇到某个SysY变量（`a`, `sum`），就需要用符号表来查找它的地址（`%a.addr`, `%b.addr`），通过插入 `load` 语句来加载其值。因为一个函数内可以存在多个同名变量（嵌套层级不同），因此变量到地址的映射不能简单地通过加一个后缀（如 `.addr`）来完成，你仍然需要维护一个符号表以保证每个变量都能找到其当前正确的地址。

??? tip "你是否需要符号表"
    当然，如果你在之前的阶段中就通过重命名实现了局部变量名的唯一性，那么你可以直接添加后缀来获取地址（`a1 -> a1.addr`），自然也就不需要符号表了。

其中 `current_bb` 用于指示当前控制流是所在的基本块，由于 `translate_expr` 只是翻译表达式，没有任何控制流转移，因此只会生成线性的指令流，翻译得到的指令序列应当插入 `current_bb` 指示的基本块。

而 `expr` 就表征了我们当前正在翻译的语法节点。举例来说，如果此节点代表某个二元运算，包含两个子 `expr` 节点，那么我们就应该如此翻译此节点：

```rust
lhs_value = translate_expr(expr1, sym_table, current_bb)
rhs_value = translate_expr(expr2, sym_table, current_bb)
result_value = create_binary(addop, lhs_value, rhs_value, current_bb)
return result_value
```
 
??? tip "模板中的实现"
    具体的实现上，你可以不用把 `symbol_table` `current_bb` 传来传去，而是保存在一个环境中。
    
    如果你使用我们提供的 C++ 模板，这二者都被保存在 `IRTranslator` 中，在翻译过程中可以直接读取和修改。


对于各种可能的 `expr`，我们可以将其翻译规则总结如下：

![ACCIPIT-EXP](../../assets/image/accipit-exp.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }
![ACCIPIT-EXP](../../assets/image/accipit-exp.svg#only-light)

其中 `create_load` `create_binary` `create_function_call` 等是生成指令的接口，它们的最后一个参数是基本块 `current_block`，表示指令在基本块 `current_block` 中插入，由于基本块中指令是线性的，你可以在基本块中维护一个 `vector`，在末端不断加入指令即可，类似于：

```cpp
void insert_instruction(Instruction *inst, BasicBlock *block) {
    std::vector<Instruction *> &instrs = block.getInstrs();
    instrs.push_back(inst);
}
```

??? tip "Value实现建议"
    === "C"
        C 通常使用 enum + union：

        ```c
        enum value_kind {
            kind_constant_int32,
            kind_constant_unit,
            kind_local_variable,
            // ...
        };

        struct value {
            enum value_kind kind,
            struct type *ty;
            union {
                struct { int number; } constant_int32;
                struct { } constant_unit;
                struct { char* name } kind_local_variable;
            };
        };
        ```

        使用一个枚举类型 enum 来标记 Value 的类型，union 来存储不同 Value 的 field，这种方式我们一般称为 "tagged union"。
        因此，在遍历 Value 时，需要根据 Value 的类型来做不同的操作：

        ```c
        void handle_value(struct value *val) {
            switch (val->kind) {
                case kind_constant_int32:
                    handle_constant_int32(val);
                    break;
                case kind_constant_unit:
                    handle_constant_unit(val);
                    break;
                // ...
                default:
                    raise_error_and_dump();
            }
        }
        ```

    === "C++"
        C++ 可以使用 `std::variant` 来代替 C 的 enum + union 来实现 “类型安全” 的 tagged union，具体可以看 [cpp reference](https://en.cppreference.com/w/cpp/utility/variant)。
        简单来说，当一个 Value 实际上是 `kind_constant_int32` 类型，但是你错误地使用了处理 `kind_binary_expression` 类型的函数处理它，就会错误地把这个 Value 当作二元表达式来处理，因此得出错误的结果，然而不论是编译时还是运行时都不会对这样的误用产生任何的报错。
        而 `std::variant` 会在上述情况发生时扔出异常，方便你 debug。

        除了 C 风格，C++ 可以使用面对对象实现：

        ```cpp
        class Value {
            /*...*/ 
        };

        class ConstantInt : public Value {
            int number;
            /*...*/ 
        };

        class ConstantUnit: public Value {
            /*...*/
        };
        ```

        在这种实现模式下，在遍历 Value 时如何判断当前 Value 是哪种类型，以便我们用正确的函数处理不同类型的 Value 呢？
        有三大类方法：

        * 使用虚函数 (上届的助教不推荐)

            例如，我一个打印 IR 的函数叫做 `print_value`，给 Value 声明一个虚成员函数 `virtual void print_value()`，然后每个 Value 的子类继承时重载这个函数，这样我们就可以对所有 Value 类型的变量直接调用 `value->print_value()` 即可。
            但是缺点是，你永远不可能知道你到底需要多少这样的处理 Value 的函数，新增一个 `Value::foo()`，你就要回过头去修改所有 Value 以及子类的声明；再新增一个 `Value::bar()`，你还要重复上面的事情。
            这样很麻烦，而且也不优雅。s所以虚函数并不适合后期可能会增加的方法， 只适合类似 print 必要的和功能固定的操作。

            在提供的 C++ 模板中，我们用虚函数实现了 `to_string` 方法， 在继承基类时，你也应该实现它。

        * 使用 RTTI（可行，~~自己看着用.jpg~~）

            C++ 标准有一个特性 “运行时类型识别” (Runtime Type Identification, RTTI)。
            使用运算符 `typeid` 会返回一个运行时的、对用户不透明的 `std::type_info` 类型，具体可参考 [cpp reference](https://en.cppreference.com/w/cpp/types/type_info)。
            你可以获取 `type_info` 的内部名字以及通过 `==` 比较两个 `type_info` 是否相等，这里我们为大家编写了一个 [godbolt 在线样例](https://godbolt.org/z/o4W7To3Ya)。

            在使用智能指针 `std::shared_ptr` 时，可以使用 `dynamic_pointer_cast` 来进行 RTTI，这样你就可以在运行时判断一个指针指向的对象的类型，然后进行相应的操作。
            ```c++
            std::shared_ptr<Base> b = std::make_shared<Derived>();
            if (auto d = std::dynamic_pointer_cast<Derived>(b)) {
                // `b` is `Derived`
            } else {
                // `b` is NOT `Derived`
            }
            ```
            如果转换成功了，`d` 就是 `b` 指向的对象的 `Derived` 类型，否则 `d` 为 `nullptr`。

            在提供的 C++ 模板中，我们正是使用了此种方法来进行 RTTI，你可以参考。

    === "OCaml"
        ML 系语言以及 Rust 都支持代数数据类型 (Algebraic Data Type)，可以很方便地定义：

        ```ocaml
        (** Identifier types *)
        type id =
        | Global of string  (** [Global s] *)
        | Local of string  (** [Local s] *)
        | Param of string  (** [Param s] *)

        (** Constant values *)
        type const =
        | Int of int  (** [Int i] *)
        | Unit  (** [Unit] *)
        | None  (** [None] *)

        (** Value representation *)
        type value = Id of id  (** [Id id] *) | Const of const  (** [Const c] *)
        ```

        在 ADT 上， 用 pattern match 可以很方便地处理不同的 Value 。

??? tip "数据结构对 IR 的影响"
    使用类似数组的数据结构存放指令序列，能够提高 cache 的命中率，这样遍历指令就会很快。
    这种实现足够简单，也足够你完成本课程的实验的基础部分了。

    但是，如果进行中端的目标无关代码优化，那么需要频繁地删除某些指令，在中间插入某些指令，或者将几条指令替换成更高效的指令，而双端链表相比数组更容易实现上面这些操作。 因此 LLVM 中使用双端链表来存放指令序列——甚至是基本块序列。

    即便如此，双端链表的访问效率仍然是个问题，这在 JIT 编译器中是一个减分项。 为此， WebKit B3 JIT compiler 就将后端模块中原来的 LLVM IR 换成了新的 B3 IR，B3 IR 就使用数组存储，为了满足在 B3 IR 层级上进行代码优化的需要，编译器引入了一个 `InsertionSet` 数据结构。
    它记录优化 Pass 中所有的变化，并在最后进行统一插入更新，以提高效率。
    如果你对此感兴趣，可以阅读 [WebKit Blog](https://webkit.org/blog/5852/introducing-the-b3-jit-compiler/)
    
??? tip "为 RISC-V 32 做特殊处理"
    一般而言，IR 的设计与实现是完全与后端平台解耦合的过程，但鉴于我们已经知道了使用的是 RISC-V 32 作为目标平台，我们可以提前做一定的准备。例如，我们已经知道了没有 RISC-V 32 指令可以接受两个立即数，那么在 `BinExpr` 中，就提前将两个运算数都是常量的表达式进行计算（`Add 1, 2 => Add 0 3` 后者可以用 `zero` 和立即数 3 相加的指令实现，前者没有直接对应的指令），你也可以事先把其中一个立即数给放到临时变量中。
    当然，你可以都把这些问题留给 Lab 4，或者等到进行局部常量折叠等编译优化的时候再考虑把 IR 转化为方便翻译的形式。不过并非所有优化都最适合在中间代码生成后再进行，语法树的树状结构其实很适合语句内的常量折叠，对于 `(2 + 3)` 这样的子表达式，你完全可以不用插入 `%0 = 2 + 3` 再返回 `%0`，而是直接返回 5 以供父节点使用，这同样是一个 `value`。你也可以在这一过程顺便完成跨语句的局部常量折叠，这不仅便于你在后端的翻译，也可以直接获得 Bonus 加分，更详细的介绍请参考 Bonus 3 文档中的[局部常量折叠](../../bonus/optimization.md#_19)部分。

    为了保持 Accipit IR 的独立与语法的简洁，我们不将这些与后端相关的要求写入语法规范中。

### 语句生成

有的语句不引起控制流的跳转，如赋值语句，他们的翻译非常简单，考虑 `a = 1 + 2 * f(5)`，它的翻译结果大概看起来像：

```rust
...
let %2 = add 1, %1
let %3 = store %2, %a.addr
```

此次翻译只是插入了一条指令，很显然我们只要先翻译子表达式拿到其返回值，然后把生成的语句插入基本块即可。

但也有语句会引起控制流的变化，他们的处理要复杂得多，考虑：
```c
if (exp) {
    stmt1;
} else {
    stmt2;
}
```

它的对应IR应该长这样：
```rust
    let %cond_value = translate_expr(cond)
    br %cond_value, label %true_label, label %false_label
%true_label:
    translate_stmt(stmt1)
    jmp label %exit_label
%false_label:
    translate_stmt(stmt2)
    jmp label %exit_label
%exit_label:
    ...
```
`cond_value` 为真时跳转到 `%true_label`，为假时跳转到 `%false_label`，最后两个基本块的控制流在 `%exit_label` 合并。 
If 语句的**大致**翻译流程为：

- 生成新的基本块 `true_label`，`false_label` 和 `exit_label`，分别用于条件为真时的跳转，条件为假时的跳转，控制流的合并。
- 调用 `translate_expr` 取得其返回值作为跳转条件，用 `true_label` 和 `false_label` 作为条件为真时和条件为假时的跳转位置, 生成条件跳转语句并插入当前基本块。
- 插入真标签，在对应的基本块中递归调用 `translate_stmt` 翻译该分支，然后在分支结束处插入跳转语句跳转到出口。
- 插入假标签，在对应的基本块中递归调用 `translate_stmt` 翻译该分支，然后在分支结束处插入跳转语句跳转到出口。
- 插入出口标签，真假分支在此汇合。


**注意**：当你为 If 语句控制流生成 `true_label` 等新的基本块时，并**不**意味这 If 语句的真分支的语句块语句块 `Stmt` 结构里的所有子语句 `Stmt` 结构都会被翻译到 `true_label` 基本块里。
当出现控制流嵌套时（例如 If 套 While，If 套 If 等），If 语句的真分支就不再是单独一个 `true_label` 基本块了，而是一个以 `true_label` 基本块为源点的**控制流子图**，你需要从 If 语句真分支翻译结束后所在的基本块（也就是这个控制流子图的汇点）跳转到 `exit_label`。

因此，我们可以选择从 `translate_stmt` 返回一个 `exit_bb`，表示翻译结束后所在的基本块，这样 `translate_stmt` 的调用者将知道翻译结束后所在的基本块了，然后就可以把跳转到出口的语句插入次基本块中。一个可供参考的 `translate_stmt` 签名可以是：

```
translate_stmt(stmt, symbol_table, current_bb) -> exit_bb
```

??? tip "模板中的实现"
    具体的实现上，你可以不用把 `symbol_table` `current_bb` `exit_bb` 传来传去，而是保存在一个环境中。
    
    如果你使用我们提供的 C++ 模板，他们都被保存在 `IRTranslator` 中, `IRTranslator` 中的 `current_bb` 会始终指向当前最新的基本块，因此你只要在翻译过程中维护好 `current_bb` 即可，而不用通过返回值声明翻译结束时在哪个基本块。

我们总结语句翻译的规则如下：

![ACCIPIT-STMT](../../assets/image/accipit-stmt.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }
![ACCIPIT-STMT](../../assets/image/accipit-stmt.svg#only-light)


#### 局部变量声明的处理

局部变量（包括函数参数）声明语句需要翻译成 `alloca` 指令，用来将它们放在栈空间上。
你需要注意，所有的 `alloca` 指令都应该放在整个函数开头的入口基本块内，而不是局部变量声明出现的那个语句块对应的基本块。
`alloca` 指令的作用域是整个函数，如果你“原地翻译”，那么可以想象一下 While 循环内声明一个局部变量——每次循环都分配栈空间，循环次数一多就爆栈了——这个变量被反复分配了新的栈空间。
其次，正如上文 `translate_expr` 提到的，你需要即时更新符号表 `sym_table`，以便在声明后我们能查到每个变量最新的地址。

#### 返回语句的处理

返回语句并不是直接插入 `ret` 指令那么简单，在底层汇编中，`return` 通常在函数结尾，除了根据 ABI 将返回值赋给对应的寄存器，你还需要清理栈帧（恢复栈指针，帧指针，恢复 Callee-saved Register 等）。
因此直接原地翻译 `return` 是不合适的，为了你后端能够舒适一些，你应当在函数入口为返回值安排一块栈空间 `%ret.addr`，并在函数末尾单独添加一个 `%exit_bb`。
函数体中遇到的 “early return” 应当按如下翻译：

- 将函数的返回值存入 `%ret.addr`
- 跳转到 `%exit_bb` 处，**同时你不应该再插入别的语句**，特别当 return 位于结构化控制流 If 和 While 内部时

对于 `%return_bb`，你应当：

- 从 `%ret.addr` 读出函数返回值
- 插入真正的 `ret` 指令

文档开头的[例子](#_1)就是如此翻译的，你可以参考。

!!! tip "return 语句缺失的情况"
    在函数的返回类型为 void 时，SysY 语法中并不要求函数必须有 `return` 语句。在这种情况下，可能出现翻译完了函数体，仍然没有跳转到 `%exit_bb` 的情况。此时按照顺序执行也没有问题，但这不符合基本块的要求，即每个基本块的结尾都需要有跳转语句。你需要能够辨别这种情况并且手动在函数末尾插入无条件跳转。
    你可以在翻译完函数体后，检查当前基本块是否存在后继来判断。

### 短路

短路（shortcut）是指在逻辑连词中一部分子表示式计算被“跳过”的情况，例如：

- `1 > 2 && call_foo()` 由于与运算左侧的 `1 > 2` 为假（即 0），那么右侧的表达式不会被执行，即 `call_foo()` 被“跳过”，整个表达式返回 0。
- `1 < 2 || call_bar()` 由于或运算左侧的 `1 < 2`为真（即 1），那么右侧的表达式不会被执行，即 `call_bar()` 被“跳过”，整个表达式返回 1。
- 更进一步地，对于类似 `1 > 2 && 3 > 4 && call_foo()` 等连续连词的情况，在 AST 上为 `(1 > 2 && 3 > 4) && call_foo()`，由于 `1 > 2` 为假，因此 `3 > 4` 会被“跳过”，因此左侧 `(1 > 2 && 3 > 4)` 为 0，进一步右侧 `call_foo()` 被“跳过”。  

纯算术表达式计算被短路对结果并不会有什么影响，但是如果表达式有副作用（例如，考虑上文中的函数调用 `call_bar()` 会输出一些内容），就会产生语义上的区别。

简言之，短路语义的结果是，在 If 和 While 等语句的条件判断中，并不是先算好整个条件表达式的结果再跳转，而是先算左侧，进行判断：或者短路，直接跳转；或者进行右侧计算，再跳转。
相比上一小节平凡的 If 和 While 语句翻译，你需要展开语句，插入辅助的基本块来翻译控制流信息。
例如，下面给出了简单的短路的翻译：
=== "&&"
    ```C
    if (a && b) {
        stmt1;
    } else {
        stmt2;
    }
    ```
    对应的翻译为：
    ```rust
        translate_expr(a)
        br %a, label %LRight, label %LFalse
    %LRight:
        translate_expr(b)
        br %b, label %LTrue, label %LFalse
    %LTrue:
        translate_stmt(stmt1)
        jmp label %LExit
    %LFalse:
        translate_stmt(stmt2)
        jmp label %LExit
    %LExit:
    ...
    ```
=== "||"
    ```C
    if (a || b) {
        stmt1;
    } else {
        stmt2;
    }
    ```
    对应的翻译为：
    ```rust
        translate_expr(a)
        br %a, label %LTrue, label %LRight
    %LRight:
        translate_expr(b)
        br %b, label %LTrue, label %LFalse
    %LTrue:
        translate_stmt(stmt1)
        jmp label %LExit
    %LFalse:
        translate_stmt(stmt2)
        jmp label %LExit
    %LExit:
    ...
    ```
=== "&& 和 || 混合"
    ```C
    if (a && b || c) {
        stmt1;
    } else {
        stmt2;
    }
    ```
    对应的翻译为：
    ```rust
        translate_expr(a)
        br %a, label %LRight, label %LFalse
    %LRight:
        translate_expr(b)
        br %b, label %LTrue, label %LRight2
    %LRight2:
        translate_expr(c)
        br %c, label %LTrue, label %LFalse
    %LTrue:
        translate_stmt(stmt1)
        jmp label %LExit
    %LFalse:
        translate_stmt(stmt2)
        jmp label %LExit
    %LExit:
    ...
    ```
<!-- ![shortcut if and](../../assets/image/shortcut_if_and.svg#only-light)
![shortcut if and](../../assets/image/shortcut_if_and.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" } -->

我们可以使用 `translate_cond` 翻译短路的条件语句，给定一整个条件式的真假跳转标签 `%LTrue` `%LFalse`，一共只有三种情况：

- 根节点是 `and/or` 以外的运算，那么我们计算条件后对应跳转真假标签即可
- 根节点是 `and`，那么我们便确定了左式的真假标签为 `%LRight`，`%LFalse`（左式为真时继续去右式检查，为假时直接去 false 分支），右式的真假标签为 `%LTrue`，`%LFalse`，对左右式递归调用 `translate_cond` 函数即可，其中右式应该在 `%LRight` 中翻译
- 根节点是 `or`，那么我们变确定了左式的真假标签为 `%LTrue`，`%LRight`，右式的真假标签为 `%LTrue`，`%LFalse`，对左右式递归调用 `translate_cond` 函数即可，其中右式应该在 `%LRight` 中翻译

!!! warning "注意"
    C 的赋值语句也有短路语义，例如 `int a = 2 < 1 && 3 > 4`。
    这引起的结果是 Expr 也可能产生控制流转移。但由于我们在[SysY语言规范](../../appendix/sysy.md)中说明了 `and/or` 只在条件语句中出现，所以我们并不需要考虑这种情况。

??? note "模板代码中的中间代码生成"

    与 `TypeChecker` 类似，我们在 `ir/ir_translator.hpp` 中定义了 `IRTranslator` 类，用于遍历 AST 并生成 IR。在 `main` 函数中，在完成类型检查之后，调用 `IRTranslator` 的 `translate` 函数即可生成 IR。

    `IRTranslator` 类的定义如下：

    ```cpp title="src/ir/ir_translator.hpp"
    class IRTranslator {
    public:
        void translate(std::shared_ptr<AST::Node> node);
        IR::ValuePtr translateExp(std::shared_ptr<AST::Node> node);

    private:
        void translateCompUnit(std::shared_ptr<AST::CompUnit> node);
        // ... 对每种 AST 节点类型定义一个 translate 函数 ... //

    private:
        int temp_count = 0;
        // 用来存储翻译出来的所有 IR
        IR::ModulePtr module = IR::Module::create();
        // 当前翻译的基本块和函数
        IR::BasicBlockPtr current_bb;
        IR::FunctionPtr current_fn;
        // 用来从标签找到对应的基本块，以便我们在不同的基本块之间加边
        std::unordered_map<IR::IdPtr, IR::BasicBlockPtr> bb_table;

        // 具体的翻译函数
        void translateCompUnit(std::shared_ptr<AST::CompUnit> node);
        // ... 对每种 AST 节点类型定义一个 translate 函数 ... //
    };
    ```

    其中：

    - `translate` 函数会根据 AST 节点的类型调用对应的 `translate` 函数，生成 IR。该函数的实现与 `TypeChecker::check` 相近，这里不再重复介绍。
    - `translateExp` 函数将会插入表达式的 IR ，并返回表达式的结果。
    - `module` 用于存储生成的 IR，`current_bb` 和 `current_fn` 用于跟踪当前翻译的基本块和函数。
    - 还有一些其他的常量和辅助函数的定义，都以在源码中详细的注释了，你可以直接使用。

    为了支持条件表达式的翻译，你可以参考文档中的 `translate_cond` 部分，设计一个 `translateCond` 函数来根据节点类型调用对应的 `translate` 函数生成条件表达式的 IR。

    我们只需要实现每个 AST 节点类型对应的 `translate` 函数，即可完成 IR 的生成。例如，对于表达式生成中遇到的 `IntConst` 节点，我们可以这样生成 IR：

    ```cpp title="src/ir/ir_translator.cpp"
    IR::ValuePtr IRTranslator::translateIntConst(
        std::shared_ptr<AST::IntConst> node) {
    IR::ConstPtr value = IR::Const::create(node->value);
    return IR::Value::create(value);
    }
    ```

    !!! note "通常对于 IR 的代码，我们不需要对其随机访问，但是在优化过程中往往需要频繁地插入和删除节点。因此，我们使用双向链表  `std::list` 来存储 IR 代码，这样可以在 O(1) 的时间复杂度内进行插入和删除操作。"


## 解释器

为了检测生成的中间代码生成正确性和评测，我们为大家提供了一个该中间表示的解释器，位于测试仓库的 `ir` 目录下。

!!! warning "尽管我们对于解释器的代码有过一些测试，但面对大家使用的实际情况，很难保证没有新的问题。如果你发现生成的中间代码不能被正确的执行，请及时联系助教。你可以直接提供有问题的代码，也可以提供一个最小的复现代码。"

### 使用方法

```console
$ python accipit.py -h
usage: accipit.py [-h] [-d] file

Interpreter for Accipit IR

positional arguments:
  file         The IR file to interpret.

options:
  -h, --help   show this help message and exit
  -d, --debug  Whether to print debug info.
```

你可以使用 `-d` 来启用 debug 模式。此模式下，解释器的每次运算都会打印出其参数和返回值，例如，使用 `call @add 1, 2` 调用下面的函数
```rust
fn @add(#a: i32, #b: i32) -> i32 {
%Lentry:
    let %0 = add #a, #b
    ret %0
}
```
会打印出：
```
STEP 1.    Eval   let %0 = call @add (1, 2)  < Line 9 > 

STEP 2.    Eval   fn @add (#a: i32, #b: i32) -> i32  < Line 1 > 
STEP 2.    Simp   fn @add (#a: 1, #b: 2) -> i32

STEP 3.    Eval   let %0 = BinExpr add, #a, #b  < Line 3 > 
STEP 3.    Simp   let %0 = BinExpr add, 1, 2
STEP 3.    Bind   3 to %0

STEP 4.    Eval   Ret %0  < Line 4 > 
STEP 4.    Simp   Ret 3
STEP 2.    Return 3 for fn @add (#a: i32, #b: i32) -> i32
STEP 1.    Bind   3 to %0
```

!!! info "大部分情况下，debug 模式的信息便足够你进行 debug 了，如果你对这一模式有任何建议，欢迎联系助教"