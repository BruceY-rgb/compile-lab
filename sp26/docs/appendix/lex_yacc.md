# App. C: Flex, Bison Crash Course

Flex 和 Bison 是两个用于词法分析和语法分析的工具，它们可以帮助我们快速的实现词法分析和语法分析。它们的理论基础——正则文法与上下文无关文法在课堂上已经讲过，这里我们将介绍它们的基本用法。

## Flex 与词法分析

Flex 是一个用于生成词法分析器的工具，词法分析的工作就是将一个字符串转换为一个 token 序列。例如将：
```c
1 + 3 * 4
```
转化为
```c
INT(1) ADD INT(3) MUL INT(4)
```
其中的 `INT(1)` `ADD` 代表的就是 token。我们需要在一个 `.l` 文件中定义每种 token 对应的正则表达式与相应操作，然后 Flex 就可以将 `.l` 文件转化为词法分析器的 C 实现供我们使用，此文件的基本结构如下：

```c
%{
    user code
%}
    definitions
%%
    rules
%%
    user subroutines
```

除去开头的 `%{}%` 里的部分进行一些必要的 include 等操作，第一部分为定义部分。在这里我们给某些后面可能经常用到的正则表达式取一个别名，从而简化词法规则的书写。例如：

```c
digit [0-9]
blank [ \t\n]
```

其中 `digit` 就是一个别名，它代表了 `[0-9]` 这个正则表达式，即一个数字字符；而 `blank` 代表了一个空白字符。

Flex 源代码文件的第二部分为规则部分，它由正则表达式和相应的响应函数组成，其格式为：

```c
pattern {action}
```

其中 `pattern` 为正则表达式，书写规则与定义部分的正则表达式相同。而 `action` 则为将要进行的具体操作。Flex 将按照这部分给出的规则至上而下依次尝试每一个规则，且尽可能匹配最长的输入串，这点在 [Flex 手册](https://westes.github.io/flex/manual/Matching.html#Matching)中有所说明。例如：

```c
{digit}+    { printf("INT(%s)\n", yytext); }
"+"         { printf("ADD\n"); }
"*"         { printf("MUL\n"); }
{blank}     {  }
.           { printf("ERROR(%s)\n", yytext); }
```

其中 `yytext` 为 Flex 预定义的一个 `char*` 指针，在每个规则对应的 `{}` 内，它会指向当前规则匹配到的字符串。而我们的操作就是简单的打印匹配到的字符串到标准输出。

其中最后一行的 `.` 代表了除了换行符外的任意字符，如果前面的规则都没有匹配到，那么就会执行这个规则，打印出错误信息，告诉用户我们遇到了预期之外的字符。换句话说，虽然 `.` 可以匹配所有非换行字符，但是由于他的匹配长度最短（长度为 1），优先级最低，所以此规则仅在所有其他规则都无法匹配时才会被执行。

最后，Flex 源代码文件的第三部分为用户自定义代码部分。这部分代码会拷贝到生成的 C 文件中，`main` 函数就可以写在这里。

最终我们的代码如下所示：

??? example "sysy.l"
    ```c
    %{
    // C 代码，也在拷贝到生成的 C 文件中
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    %}

    %option noyywrap

    digit [0-9]
    blank [ \t\n]

    %%
    {digit}+  { printf("INT(%s)\n", yytext); }
    "+"     { printf("ADD\n"); }
    "*"     { printf("MUL\n"); }
    {blank}   {}
    .       { printf("ERROR(%s)\n", yytext); }
    %%

    int main() {
        yylex(); // 调用词法分析器
        return 0;
    }

    ```

使用 `flex` 生成 C 代码，然后使用 `gcc` 编译，最后运行有如下效果：

```console
$ flex -o sysy.yy.c sysy.l # -o 指定输出文件
$ gcc sysy.yy.c -o compiler
$ ./compiler
1 + 3 * 4
INT(1)
ADD
INT(3)
MUL
INT(4)
```

但只是一边匹配一边打印显然并不足够我们完成词法分析。我们需要的是一边匹配一边输出一个 token 序列。为此，我们只需要在 action 部分返回对应 token 即可。

假设我们已经预先定义好了 `INT` `ADD` `MUL` 这三种 token，我们只需要在 action 部分返回这三个 token 即可：

```c
...
{digit}+  { return INT; }
"+"     { return ADD; }
"*"     { return MUL; }
...
```

然而你可能注意到了，有些 token 只需要返回 token 类型（ `ADD` `MUL` ）；而有些 token 需要返回 token 类型和相应的属性值，如对于 `INT`，我们带上对应的整形字面量返回。在 Flex 中，我们通过对 Flex 预定义的 `yylval` 变量赋值来使 token 携带属性值：

```c
{digit}+  { yylval = atoi(yytext); return INT; }
```

假设 `{digit}+` 匹配到了 `123`，那么我们先将字符转化为 int 类型，然后赋值给 `yylval`， 最后返回的 token 就会是 `INT(123)`。

这样的代码就能返回一个带有丰富信息的 token 序列了。然而 Flex 的大厦上还飘着一朵乌云：token 是在哪里以及如何定义的？ 实际上这一部分与下一节要介绍的 Bison 有关：作为这些 token 的使用者，Bison 需要知道所有的 token 类型，因此这些 token 实际上是在 Bison 中定义，再在 Flex 中使用的。

## Bison 与语法分析

与 Flex 类似，我们可以在 `.y` 文件中声明一个语言的上下文无关文法。Bison 就可以从这个文件中生成一个语法分析器的 C 实现，来对一个 token 序列进行语法分析并完成相应的动作，如计算，打印信息或者构建抽象语法树。该文件的基本结构如下：

```c
%{
    user code
%}
    bison declarations
%%
    bison grammar rules
%%
    more user code
```

接下来我们基于之前定义的 Flex 代码，来实现一个简单的计算器。首先，我们要在 Bison 中声明将会出现的 token：

```c
%token INT
%token ADD MUL
```

我们在 Flex 中返回的就是这些 token ，其中的 `INT` 还会带上一个整形的属性值，剩下两个 token 不带有属性值。

???+ "让 Flex 使用 Bison 中定义的 token"
    随后在使用 Bison 编译这份源代码时，我们需要加上 `-d` 参数：

    ```console
    $ bison -d sysy.y
    ```

    其含义是将编译的结果增加一个头文件，这样 Flex 代码在开头的 `%{}%` 部分通过 include 就可以使用 Bison 中定义的词法单元。

    如果你使用我们的 C++ 模板，你并不需要担心 Flex 与 Bison 相关的依赖与编译问题，我们已经为你处理好了。

在定义部分定义的所有 token 构成了此上下文无关文法的终结符。而在第二部分，也就是规则部分，我们将定义该文法剩下的三部分，也就是非终结符，产生式和起始符号。

我们直接来看下面给出的规则部分，直观来看它就是一组规则。以 `:` 为分隔，所有规则的左边部分就声明了所有的非终结符（`Calc` `Exp` `Factor` `Term`）；右边的每条产生式是一串由终结符和非终结符组成的序列；默认情况下第一条规则的非终结符就是起始符号（`Calc`），这样我们就完全定义了上下文无关文法。

```c
%%
Calc : /* empty */ { printf("Empty!\n"); }
    | Exp { printf("= %d\n", $1); }
    ;

Exp : Factor { $$ = $1;}
    | Exp ADD Factor { $$ = $1 + $3; }
    ;

Factor : Term { $$ = $1; }
    | Factor MUL Term { $$ = $1 * $3; }
    ;

Term: INT { $$ = $1; }
    ;
%%

void yyerror(const char *s) {
    printf("error: %s\n", s);
}
```

除了文法定义部分，其实更值得关心的是每条产生式右边 `{}` 中的语义动作部分。Bison 在每一条产生式的规约完成后，就会执行其中的语义动作。我们可以在语义动作中完成简易计算器的计算部分。

对于下面的输入：
```c
1 + 3 * 4
```
不难看出，其对应的语法树为：

```
level 0             Calc        
                      │         
level 1              Exp        
               ┌──────┼──────┐    
level 2     Factor   ADD   Factor 
              │        ┌─────┼─────┐
level 3     Term     Factor MUL  Term
              │        │           │    
level 4     INT(1)   Term        INT(4) 
                       │          
level 5              INT(3)       
```

从概念上来说，想要完成计算，我们需要将叶子的 `INT(3)` 和 `INT(4)` 向上传递到第 3 层，然后在第 2 层完成乘法计算，计算的结果 12 和另一边传上来的 1 一起在第 1 层完成加法计算，最终把计算结果 13 上传到 `Calc`。

幸运的是，Bison 的所有非终结符也都可以带上一个属性值，我们通过在语义动作中对 `$$` 赋值来给将属性值传递给规则左边的非终结符。而对于规则右边的每个子终结符/子非终结符，我们也可以通过 `$1` `$2` `$3` ... 来访问它们的属性值，注意这里下标是从 1 开始的。

!!! warning "访问未定义属性值节点的属性值会导致未定义行为"

那么我们就可以在语义动作中实现我们在上述过程中完成的计算过程了，对于向上传递值，我们只需要将子节点的属性值赋给父节点即可。

```c
Term: INT { $$ = $1; }
```

对于加法和乘法，我们只需要将左右两个子节点的值相加或相乘即可。
```c
Exp : Factor { $$ = $1;}
    | Exp ADD Factor { $$ = $1 + $3; }
    ;
```

最终在 `Calc` 节点，我们可以将子节点传递上来的计算结果打印出来
```c
Calc : /* empty */ { printf("Empty!\n"); }
    | Exp { printf("= %d\n", $1); }
    ;
```

补足剩余规则，我们就完成了！最终的代码如下所示：

=== "sysy.l"

    ``` c
    %option noinput
    %option nounput
    %option noyywrap

    %{
    #include "sysy.tab.hh"
    %}

    digit [0-9]
    blank [ \t\n]

    %%

    {digit}+        { yylval = atoi(yytext); return INT; }
    "+"             { return ADD; }
    "*"             { return MUL; }
    {blank}         { }
    .               { printf("ERROR(%s)\n", yytext); }

    %%

    ```

=== "sysy.y"
    ```c
    %{
    #include <stdio.h>
    void yyerror(const char *s);
    extern int yylex(void);
    %}

    %token INT
    %token ADD MUL

    %%
    Calc : /* empty */ { printf("Empty!\n"); }
        | Exp { printf("= %d\n", $1); }
        ;

    Exp : Factor { $$ = $1;}
        | Exp ADD Factor { $$ = $1 + $3; }
        ;

    Factor : Term { $$ = $1; }
        | Factor MUL Term { $$ = $1 * $3; }
        ;

    Term: INT { $$ = $1; }
        ;
    %%

    void yyerror(const char *s) {
        printf("error: %s\n", s);
    }

    ```

=== "main.cc"
    ```c
    extern int yyparse();

    int main() {
    yyparse();
    return 0;
    }
    ```

编译运行：

```console
$ make
$ ./compiler
1 + 3 * 4
<Ctrl+D>
13
```

这就是我们在 Lab 0 中提供的简易计算器。

!!! tip
    - **Bison 允许递归定义**：例如，在定义 `Exp` 时，我们可以在右边的产生式中使用 `Exp` 本身。这样的递归定义在上下文无关文法中本就很常见。在实验中善加利用递归定义，可以让你的代码更加简洁。
    - **空产生式的写法**：你可能注意到了 `Calc` 的定义中有一个空产生式，空产生式就是一个什么规则都没写的分支，但你仍然可以在其中写一些语义动作。我们一般用一个 `/* empty */` 的占位注释来提醒读者这是一个空产生式。
    - **从文件读取输入**：上述例子都是从标准输入读取的，如果想要从文件读取，可以使用 `yyin = fopen("filename", "r")` 来指定输入文件。在 C++ 模板的 main 函数中，我们已经为你完成了这一操作。

除了进行计算，打印信息，我们当然也可以在语义动作中构建抽象语法树--从子节点获取信息，然后调用构造函数构建一个新的节点，再将这个节点传递给父节点。如此往复直到在根节点构建起一整棵语法树。这就是你要在 Lab 1 实现的。

###  在 Bison 中使用别名

在 Bison 中 声明 token 的时候，你可以在它的后面给定一个字符串，那么在接下来的规则中，你就可以用这个字符串作为 token 的别名。这样可以使你的代码更加易读，例如：

```c hl_lines="1-2 10 14"
%token ADD "+"
%token MUL "*"

%%
Calc : /* empty */ { printf("Empty!\n"); }
    | Exp { printf("= %d\n", $1); }
    ;

Exp : Factor { $$ = $1;}
    | Exp "+" Factor { $$ = $1 + $3; }
    ;

Factor : Term { $$ = $1; }
    | Factor "*" Term { $$ = $1 * $3; }
    ;

Term: INT { $$ = $1; }
    ;
%%
```

!!! warning "别名是一个字符串，请勿使用单引号的字符"

### 更多的属性类型

在只有一个属性类型的时候，你可以像上面的定义那样不声明属性类型。但是如果不同的 token 需要携带不同类型的属性值，你就需要显式的声明他们。首先在 Bison 中用 `%union` 定义所有可能的属性值类型：

```c
%union {
    int val;
}
```

此后我们就可以用 val 来代表整型属性值。

在 Flex 中，在赋值给 `yylval` 时，你需要指定属性值的类型，如：

```c
{digit}+        { yylval.val = atoi(yytext); return INT; }
```

这里的 `.val` 指定了这个 token 的属性值类型为整型。

在 Bison 中，你需要为每个有属性值的 token 和 rule 指定属性值类型，如：

```c
%token <val> INT
%type <val> Exp Factor Term
```

### 在 Bison 中处理优先级问题

!!! warning "阅读这部分内容需要你至少掌握了 LR(0) 的理论知识"

你可能已经注意到了，在上面的例子中，我们用一种比较复杂的方式来构造了文法，一种更显然更直接的文法是：

```c
Exp : Exp ADD Exp { $$ = $1 + $3; }
    | Exp MUL Exp { $$ = $1 * $3; }
    | INT { $$ = $1; }
    ;
```

虽然这看起来是一个直观的表达式语法，但它实际上存在二义性，Bison 会在解析过程中会因为操作符的结合性（associativity）和优先级（precedence）问题而产生 shift-reduce conflicts：

- **结合性问题**：对于 `1 + 2 + 3`，解析器在遇到 `1 + 2 · + 3` 时（`·` 表示当前分析的位置），有两种可能的解析方式：
    - **Shift**：将 `+ 3` 作为新的一部分解析，形成 `1 + (2 + 3)`（右结合）
	- **Reduce**：将 `1 + 2` 先计算，形成 `(1 + 2) + 3`（左结合）
 
    由于我们没有声明加法的结合性，这在解析中就体现为了 shift-reduce conflict。

- **优先级问题**：对于 `1 * 2 + 3`，解析器在遇到 `1 * 2 · + 3` 时，有两种可能的解析方式：
    - **Shift**：继续解析 `+ 3`，形成 `1 * (2 + 3)`（加法优先）
	- **Reduce**：先计算 `1 * 2`，然后解析 `+ 3`，形成 `(1 * 2) + 3`（乘法优先）

    由于我们没有声明加法和乘法的优先级，这在解析中也体现为了 shift-reduce conflict。

如果你用 Bison 解析这个语法，Bison 会警告你这里存在 shift-reduce conflict，你可以加入对应的参数来让 Bison 打印出具体的例子。

要解决这样的二义性，我们可以重写语法规则，得到像前文例子中那样的文法。如果你尝试使用之前的语法来解析这两个例子，会发现并不存在二义性。

除了重写语法规则之外， Bison 还给我们提供了额外的工具来处理结合性和优先级问题，详见 Bison 手册中的 [Operator Precedence](https://www.gnu.org/software/bison/manual/html_node/Precedence.html) 部分。我们这里作简要介绍，足够 Lab 1 的使用需求。

<!-- 在上面的 shift-reduce conflicts 中，不难发现，shift 路线都是对应了一个新的 token（`ADD`），而 reduce 路线都对应了一条产生规则（`Exp ADD Exp`，`Exp MUL Exp`）。想要告知 Bison 如何在这二者之间选择，我们只要给每个规则和 token 都指定优先级即可。 -->

#### 结合性声明

在 `1 + 2 + 3` 这种情况下，我们希望加法是左结合，即 `(1 + 2) + 3`。Bison 提供了四种结合性声明：

- `%left`：左结合
- `%right`：右结合
- `%nonassoc`：不可结合（例如 `%nonassoc LT` 会让 `1 < 2 < 3` 解析时报错）
- `%precedence`：不定义任何结合性（只用于定义优先级），任何与结合性相关的冲突都会在编译时报错

例如，对于加法，我们使用**左结合**：

```c
%left ADD
```

这样，`1 + 2 + 3` 就会被解析为 `(1 + 2) + 3`，解决了结合性问题导致的 shift-reduce conflicts。

<!-- 这样会让 `Exp ADD Exp` 的优先级高于 `ADD`，`1 + 2 + 3` 就会被解析为 `(1 + 2) + 3`，解决了结合性问题导致的 shift-reduce conflicts。 -->

#### 优先级声明

在 Bison 中，运算符的优先级由声明的顺序决定，**越先声明的优先级越低**。例如：

```c
%left ADD    // 加法是左结合，且优先级较低
%left MUL    // 乘法是左结合，且优先级较高
```

这里，我们声明了加法的优先级低于乘法，这样 `1 * 2 + 3` 就会被解析为 `(1 * 2) + 3`，解决了优先级问题导致的 shift-reduce conflicts。

<!-- 这里，我们声明了 `MUL` 的优先级低于 `ADD`，也就是 `Exp MUL Exp`（shift 对应的 rule）的优先级高于 `ADD`（reduce 对应的 token）。这样 `1 * 2 + 3` 就会被解析为 `(1 * 2) + 3`，解决了优先级问题导致的 shift-reduce conflicts。 -->

<!-- 可以说，当代表规则优先级的 token 和 reduce 对应的 token 优先级相同时（比如结合性问题中的的 `ADD` 和 `MUL`），`%left` 和 `%right` 的作用就是告诉 Bison 该如何选择 reduce 还是 shift。 -->

加入这两行后，Bison 就会根据这些优先级和结合性来解决 shift-reduce conflicts 了。此时你重新编译，Bison 已经不会再警告你存在 shift-reduce conflicts 了。

!!! warning "默认处理方式"
    Bison 只会警告你存在 shift-reduce conflicts，而不是报错，因为默认情况下，Bison 会在二者中选择 shift，有时候默认的处理正好就是你想要的，但是你不应该依赖这一点来保证你的代码运行正确。

!!! tip "覆盖默认优先级"
    一条规则的默认优先级是其最右边的 non-terminal 的优先级，但是你可以通过 `%prec` 来覆盖这一默认行为，比如：

    ```c
    Exp : Exp MUL Exp %prec HIGHEST { $$ = $1 + $3; }
    ```

    这条规则的优先级就是 `HIGHEST`。 你可以用 `%nonassoc` 来定义这样只用于优先级声明的 token。

    ```c
    %left ADD
    %left MUL
    %nonassoc HIGHEST
    ```

我们在这里使用的其实是 Flex 和 Bison 中最基本功能，我们推荐阅读以下资料来进一步了解这两个工具的使用：

- [南京大学编译原理课程实验手册](https://cs.nju.edu.cn/changxu/2_compiler/projects/Project_1.pdf) 2.2 节实验指导，不要使用其中 yacc 里的代码 `#include lex.yy.c`，这会和我们提供的构建工具冲突。
- [北大编译实践在线文档 Lv1.2 节](https://pku-minic.github.io/online-doc/#/lv1-main/lexer-parser?id=cc-实现)
- [Flex 手册](https://westes.github.io/flex/manual/)
- [Bison 手册](https://www.gnu.org/software/bison/manual/html_node/index.html#SEC_Contents)

<!-- 如果你认为你已经完全掌握了，也可以直接前往阅读 [Lab 1 模板代码导读](../lab/lab1.md#_6) 并且开始实验。 -->