# App. A: SysY 语言规范

SysY 语言是[全国大学生计算机系统能力大赛](https://compiler.educg.net/#/)中编译系统设计赛要实现的编程语言。

你可以在[这里](../assets/SysY2022.pdf)找到正式的 SysY 的文法和语义约束。
为了减轻大家的负担，**我们对SysY语言做了一些简化和修改**，具体如下：

- 删除了 `ConstDecl` `ConstDef` `ConstInitVal` `ConstExp` 等常量相关的语法。
- 删除了 `float` 类型，即不需要实现浮点数类型。
- 删除了 `break` 语句和 `continue` 语句。
- 我们并不使用 SysY 官方提供的运行时库，而是使用编译器内置的 `int read()`、`void write(int)` 函数作为输入输出，其在中间代码和汇编中的实现见具体实验。

## 简化后的 SysY 语言文法

- 符号 `[...]` 表示方括号内包含的项可被重复 0 次或 1 次。
- 符号 `{...}` 表示花括号内包含的项可被重复 0 次或多次。
- 终结符是由双引号括起的串，或者是 `IDENT`、`INT_CONST` 这样全大写的记号。其余均为非终结符。

SysY 语言的文法表示如下，`CompUnit` 为开始符号：

```ebnf
CompUnit      ::= [CompUnit] (Decl | FuncDef);

Decl          ::= VarDecl;
BType         ::= "int";
VarDecl       ::= BType VarDef {"," VarDef} ";";
VarDef        ::= IDENT {"[" INTCONST "]"}
                | IDENT {"[" INTCONST "]"} "=" InitVal;
InitVal       ::= Exp | "{" [InitVal {"," InitVal}] "}";

FuncDef       ::= FuncType IDENT "(" [FuncFParams] ")" Block;
FuncType      ::= "void" | "int";
FuncFParams   ::= FuncFParam {"," FuncFParam};
FuncFParam    ::= BType IDENT ["[" "]" {"[" INTCONST "]"}];

Block         ::= "{" {BlockItem} "}";
BlockItem     ::= Decl | Stmt;
Stmt          ::= LVal "=" Exp ";"
                | [Exp] ";"
                | Block
                | "if" "(" Cond ")" Stmt ["else" Stmt]
                | "while" "(" Cond ")" Stmt
                | "return" [Exp] ";";

Exp           ::= AddExp;
Cond          ::= LOrExp;
LVal          ::= IDENT {"[" Exp "]"};
PrimaryExp    ::= "(" Exp ")" | LVal | Number;
Number        ::= INTCONST;
UnaryExp      ::= PrimaryExp | IDENT "(" [FuncRParams] ")" | UnaryOp UnaryExp;
UnaryOp       ::= "+" | "-" | "!";  注：'!'仅出现在条件表达式中
FuncRParams   ::= Exp {"," Exp};

MulExp        ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
AddExp        ::= MulExp | AddExp ("+" | "-") MulExp;
RelExp        ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
EqExp         ::= RelExp | EqExp ("==" | "!=") RelExp;
LAndExp       ::= EqExp | LAndExp "&&" EqExp;
LOrExp        ::= LAndExp | LOrExp "||" LAndExp;
```

## SysY 语言的终结符特征

### 标识符

SysY 语言中标识符 `IDENT`（identifier）的规范如下：

```ebnf
identifier ::= identifier-nondigit
             | identifier identifier-nondigit
             | identifier digit;
```

其中，`identifier-nondigit` 为下划线，小写英文字母或大写英文字母；`digit` 为数字 0 到 9。

关于其他信息，请参考 [ISO/IEC 9899](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf) 第 51 页关于标识符的定义。

对于同名**标识符**，SysY 中有以下约定：

- 同一个作用域内禁止定义同名标识符。
- 内部作用域中的标识符会遮蔽外部作用域中的同名标识符。

### 数值常量

SysY 语言中数值常量可以是整型数 `INTCONST` (integer-const)，其规范如下：

```ebnf
integer-const       ::= decimal-const
                      | octal-const
                      | hexadecimal-const;
decimal-const       ::= nonzero-digit
                      | decimal-const digit;
octal-const         ::= "0"
                      | octal-const octal-digit;
hexadecimal-const   ::= hexadecimal-prefix hexadecimal-digit
                      | hexadecimal-const hexadecimal-digit;
hexadecimal-prefix  ::= "0x" | "0X";
```

其中，`nonzero-digit` 为数字 1 到 9；`octal-digit` 为数字 0 到 7；`hexadecimal-digit` 为数字 0 到 9，或大写/小写字母 a 到 f。

关于其他信息，请参考 [ISO/IEC 9899](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf) 第 54 页关于整数型常量的定义，在此基础上忽略所有后缀。

### 注释

SysY 语言中注释的规范与 C 语言一致，如下所示：

- 单行注释：以序列 `//` 开始，直到换行符结束，不包括换行符。
- 多行注释：以序列 `/*` 开始，直到第一次出现 `*/` 时结束，包括结束处 `*/`。

关于其他信息，请参考 [ISO/IEC 9899](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf) 第 66 页关于注释的定义。

## 语义约束

### 编译单元

```ebnf
CompUnit    ::= [CompUnit] (Decl | FuncDef);
Decl        ::= VarDecl;
```

1. 一个 SysY 程序由单个文件组成，文件内容对应 EBNF 表示中的 `CompUnit`。在该 `CompUnit` 中，必须存在且仅存在一个标识为 `main`，无参数，返回类型为 `int` 的 `FuncDef`（函数定义）。`main` 函数是程序的入口点。
2. `CompUnit` 的顶层变量声明语句（对应 `Decl`），函数定义（对应 `FuncDef`）都不可以重复定义同名标识符（`IDENT`），即便标识符的类型不同也不允许。
3. `CompUnit` 的变量/函数声明的作用域从该声明处开始，直到文件结尾。

### 变量定义

```ebnf
VarDef  ::= IDENT {"[" INTCONST "]"}
          | IDENT {"[" INTCONST "]"} "=" InitVal;
```

1. `VarDef` 用于定义变量。当不含有 `=` 和初始值时，其运行时实际初值未定义。
2. 全局变量的作用域从定义处开始，直到文件结尾；局部变量的作用域从定义处开始，直到包含该变量的语句块结束。
3. `VarDef` 的数组维度和各维长度的定义部分不存在时，表示定义单个变量；存在时，表示定义多维数组。其语义和 C 语言一致，比如 `[2][4][3]` 表示三维数组，第一到第三维长度分别为 2、4 和 3，每维的下界从 0 开始编号。SysY 在声明数组时各维长度都需要显式给出，而不允许是未知的。
4. 当 `VarDef` 含有 `=` 和初始值时，`=` 右边的 `InitVal` 初始化器必须是以下三种情况之一（注：`int` 型初始值可以是 `INTCONST`，或者是 `int` 型表达式）：
    1. 一对花括号 `{}`，表示所有元素初始为 0。
    2. 与多维数组中数组维数和各维长度完全对应的初始值，如 `{{1,2},{3,4},{5,6}}`、`{1,2,3,4,5,6}`、`{1,2,{3,4},5,6}` 均可作为 `a[3][2]` 的初始值。
    3. 如果花括号括起来的列表中的初始值少于数组中对应维的元素个数，则该维其余部分将被隐式初始化，需要被隐式初始化的整型元素均初始为 0。如 `{{1,2},{3},{5}}`、`{1,2,{3},5}`、`{{},{3,4},5,6}` 均可作为 `a[3][2]` 的初始值，前两个将 `a` 初始化为 `{{1,2},{3,0},{5,0}}`、`{{},{3,4},5,6}` 将 `a` 初始化为 `{{0,0},{3,4},{5,6}}`。
    4. 花括号括起来的列表中的初始值不能多于数组中对应维的元素总数。

例如，下列变量 `a` 到 `e` 的声明和初始化都是合法的：

```c
int a[4][2] = {};
int b[4][2] = {1, 2, 3, 4, 5, 6, 7, 8};
int c[4][2] = {{1, 2}, {3, 4}, 5, 6, 7, 8};
int d[4][2] = {1, 2, {3}, {5}, 7 , 8};
int e[4][2] = {{d[2][1], c[2][1]}, {3, 4}, {5, 6}, {7, 8}};
```

<!-- 4. `VarDef` 中表示各维长度的 `INTCONST` 必须能被求值到非负整数，但 `InitVal` 中的初始值为 `Exp` 可以引用变量。 -->

### 初值

```ebnf
InitVal ::= Exp | "{" [InitVal {"," InitVal}] "}";
```

1. 全局变量声明中指定的初值表达式必须是常量 `INTCONST`。
2. 未显式初始化的局部变量，其值是不确定的；而未显式初始化的全局变量，其（元素）值均被初始化为 0。
3. 变量声明中指定的初值要与该变量的类型一致。如下形式的 `VarDef` 不满足 SysY 语义约束：

```c
a[4] = 4;
a[2] = {{1,2}, 3};
a = {1,2,3};
```

### 函数形参与实参

```ebnf
FuncFParam  ::= BType IDENT ["[" "]" {"[" INTCONST "]"}];
FuncRParams ::= Exp {"," Exp};
```

1. `FuncFParam` 定义函数的一个形式参数。当 `IDENT` 后面的可选部分存在时，表示定义数组类型的形参。
2. 当 `FuncFParam` 为数组时，其第一维的长度省去（用方括号 `[]` 表示），而后面的各维则需要用表达式指明长度，其长度必须是常量 `INTCONST`。
3. 函数实参的语法是 `Exp`。对于 `int` 类型的参数，遵循按值传递的规则；对于数组类型的参数，形参接收的是实参数组的地址，此后可通过地址间接访问实参数组中的元素。
4. 对于多维数组，我们可以传递其中的一部分到形参数组中。例如，若存在数组定义 `int a[4][3]`，则 `a[1]` 是包含三个元素的一维数组，`a[1]` 可以作为实参，传递给类型为 `int[]` 的形参。

### 函数定义

```ebnf
FuncDef ::= FuncType IDENT "(" [FuncFParams] ")" Block;
```

1. `FuncDef` 表示函数定义。其中的 `FuncType` 指明了函数的返回类型。
    - 当返回类型为 `int` 时，函数内的所有分支都应当含有带有 `Exp` 的 `return` 语句。不含有 `return` 语句的分支的返回值未定义。
    - 当返回值类型为 `void` 时，函数内只能出现不带返回值的 `return` 语句。
2. `FuncDef` 中形参列表（`FuncFParams`）的每个形参声明（`FuncFParam`）用于声明 `int` 类型的参数，或者是元素类型为 `int` 的多维数组。`FuncFParam` 的语义参见前文。

### 语句块

```ebnf
Block ::= "{" {BlockItem} "}";
BlockItem ::= Decl | Stmt;
```

1. `Block` 表示语句块。语句块会创建作用域，语句块内声明的变量的生存期在该语句块内。
2. 语句块内可以再次定义与语句块外同名的变量（通过 `Decl` 语句），其作用域从定义处开始到该语句块尾结束，它覆盖了语句块外的同名变量。

### 语句

```ebnf
Stmt ::= LVal "=" Exp ";"
       | [Exp] ";"
       | Block
       | "if" "(" Cond ")" Stmt ["else" Stmt]
       | "while" "(" Cond ")" Stmt
       | "return" [Exp] ";";
```

1. `Stmt` 中的 `if` 型语句遵循就近匹配的原则。
2. 单个 `Exp` 可以作为 `Stmt`。`Exp` 会被求值，所求的值会被丢弃。

### 左值表达式

```ebnf
LVal ::= IDENT {"[" Exp "]"};
```

1. `LVal` 可以为变量或者某个数组元素，使用 `LVal` 必须在对应的变量或数组的作用域内。
2. 当 `LVal` 表示单个变量时，不能出现后面的方括号。
3. 当出现在赋值语句的左侧时，`LVal` 必须是 int 类型。此时它是严格意义上的左值。但这个产生式也可出现在函数的实参中，此时它不一定要是 int 类型，也不是严格意义上的左值。

### 表达式

1. `Exp` 在 SysY 中代表 `int` 或者其数组型表达式。故它定义为 `AddExp`；`Cond` 代表条件表达式，故它定义为 `LorExp`。
2. 函数调用形式是 `IDENT "(" FuncRParams ")"`，其中的 `FuncRParams` 表示实际参数。实际参数的类型和个数必须与 `IDENT` 对应的函数定义的形参完全匹配。
3. SysY 中算符的优先级与结合性与 C 语言一致，上一节定义的 SysY 文法中已体现了优先级与结合性的定义。
