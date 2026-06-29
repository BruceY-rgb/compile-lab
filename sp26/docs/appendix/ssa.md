# App. F: 静态单赋值 SSA

在编译器的优化中，**静态单赋值（Single Static Assignment，简称 SSA）**是一种十分重要的中间表示形式。在这一节中，我们将介绍如何将普通的中间表示转换为符合 SSA 规范的中间表示，以及如何利用 SSA 进行各种优化。

## 静态单赋值与 `PHI` 函数

静态单赋值作为一种特殊的中间表示形式，它的第一个特点是**每个变量只被赋值一次**。这种形式的好处是可以方便地进行各种优化，比如公共子表达式消除、全局值编号、常量传播、死代码消除等。例如，为了实现求三个数的平均值，我们可能会生成如下中间表示：

```c linenums="1" hl_lines="5 6 7"
FUNCTION main:
    T0 = CALL read
    T1 = CALL read
    T2 = CALL read
    T3 = T0 + T1
    T3 = T3 + T2
    T3 = T3 / #3
    ARG T3
    CALL write
    T4 = #0
    RETURN T4
```

这段代码中，`T3` 被赋值了三次，并不符合 SSA 的要求。符合 SSA 的代码应该是这样的：

```c linenums="1" hl_lines="5 6 7"
FUNCTION main:
    T0 = CALL read
    T1 = CALL read
    T2 = CALL read
    T3 = T0 + T1
    T4 = T3 + T2
    T5 = T4 / #3
    ARG T5
    CALL write
    T6 = #0
    RETURN T6
```

让我们再看一个包含循环的例子：

```c linenums="1" hl_lines="3 4 7 8"
FUNCTION main:
    T0 = CALL read  // N
    T1 = #0         // sum
    T2 = #0         // count
LABEL L0:
    T3 = CALL read
    T1 = T1 + T3
    T2 = T2 + #1
    IF T2 < T0 GOTO L0
    T5 = T1 / T0
    ARG T5
    CALL write
    T6 = #0
    RETURN T6
```

这是一个对输入的 N 个数求均值的简单程序。这段代码中，`T1` 和 `T2` 被赋值了多次，显然不符合 SSA 的每个变量只被赋值一次的要求。为此，我们引入一个全新的概念：`PHI` 函数，它可以根据当前基本块的前一个基本块来决定变量的值。在 Zero IR 中，`PHI` 函数的语法如下：

```py
NAME "=" "PHI" "[" LABEL "," NAME "]" ("," "[" LABEL "," NAME "]")*
```

!!! tip "Accipit IR 中的 `PHI` 函数语法见[这里](../appendix/accipit-spec.md#_7)。"

例如，我们有如下代码：

```c
T0 = PHI [L1, T1], [L2, T2], [L3, T3]
```

这个语句的意思是，如果当前的语句从基本块 `L1` 跳转而来，那么将 `T1` 赋值给 `T0`；如果当前的语句从基本块 `L2` 跳转而来，那么将 `T2` 赋值给 `T0`；如果当前的语句从基本块 `L3` 跳转而来，那么将 `T3` 赋值给 `T0`。

有了它，我们可以将上面的代码改写为：

```c hl_lines="6 7" linenums="1"
FUNCTION main:
    T0 = CALL read  // N
    T1 = #0         // sum
    T2 = #0         // count
LABEL L0:
    T3 = PHI [main, T1], [L0, T6]
    T4 = PHI [main, T2], [L0, T7]
    T5 = CALL read
    T6 = T3 + T5
    T7 = T4 + #1
    IF T7 < T0 GOTO L0
LABEL L1:
    T8 = T6 / T0
    ARG T8
    CALL write
    T9 = #0
    RETURN T9
```

这样，我们就解决了循环中变量被赋值多次的问题。需要注意的是，`PHI` 语句只能出现在基本块的开头，有多个 `PHI` 语句时，它们之间的顺序是无关的。

!!! tip "我们的 Zero IR Interpreter & Debugger 以及 Accipit IR Interpreter 已经支持了 `PHI` 函数，你可以在其中尝试使用 `PHI` 函数。"

除了每个变量只被赋值一次的要求外，SSA 还有一个特点是，**每个变量的定义点必须支配其使用点**。不满足这一条件的 SSA 被称为**部分 SSA（Partial-SSA）**。例如，对于这个获取第 10 个输入的函数：

```c
int Example() {
    int i = 0;
    int x;
    while (i < 10) {
        x = read();
        i = i + 1;
    }
    return x;
}
```

你可能会想到以下中间代码：

![Partial SSA](../assets/image/partial-ssa.svg#only-light){ width=400 }
![Partial SSA](../assets/image/partial-ssa.svg#only-dark){ width=400 style="filter: invert(1) hue-rotate(180deg)" }

乍一看，这段代码似乎符合 SSA 的要求，因为每个变量只被赋值了一次。但其实这只是一个 Partial-SSA，因为 `T3` 的定义点（基本块 `L1` 内）显然不支配其使用点（基本块 `L2` 内）。为了解决这个问题，你可以在 `L0` 块内插入一个 `PHI` 函数，将 `T3` 的定义点扩展到 `L0` 块内：

![Full SSA](../assets/image/full-ssa.svg#only-light){ width=400 }
![Full SSA](../assets/image/full-ssa.svg#only-dark){ width=400 style="filter: invert(1) hue-rotate(180deg)" }

这里的 $\bot$ 是一个特殊标记，你可以理解为某些语言中的 `undefined`，也就是未定义的值。这样，我们就把 `T3` 的定义点挪到了 `L0` 块内，使得它可以支配 `L2` 块内的使用点。

!!! tip "关于实践中的 $\bot$"
    在实际应用中，最好不要出现 $\bot$ 这种未定义的值，而是给未定义的变量手动赋一个初始值。例如，在上面的例子中，你可以在 $\bot$ 的来源，即 `Example` 块内加上 `T3u = #0`，然后将 `L0` 内定义 `T3` 的 `PHI` 函数改为 `T3 = PHI [Example, T3u], [L1, T3o]`。

## 生成符合 SSA 规范的中间表示

!!! info "构建含有 `PHI` 函数的 SSA 最高可获得 1 分加分"

为了生成符合 SSA 规范的中间表示，我们需要对代码进行一些改写。你有多种方法可以实现这一目的，例如将普通的 IR 转换为符合 SSA 形式的 IR，也可以直接生成符合 SSA 的中间表示。下面我们将介绍这两种方法。

### 将已有的 IR 转换为 SSA 形式

不难发现，会出现多个赋值或者活跃周期跨越多个块语句的往往是源代码中的局部变量，而在生成中间代码时增加的临时变量可以保证不重复赋值，也不会跨越多个块。基于这个前提，我们可以使用栈来存储这些变量，转化为符合 SSA 形式的中间代码。例如，在直接生成 IR 时，我们可以将 `N` `sum` `count` 存储在栈上：

```c linenums="1" hl_lines="2 3 4 6 8 10 12 13 16 18 19 22 23"
FUNCTION main:
    _T0 = DEC #4    // N
    _T1 = DEC #4    // sum
    _T2 = DEC #4    // count
    T0 = CALL read
    *_T0 = T0
    T1 = #0
    *_T1 = T1
    T2 = #0
    *_T2 = T2
LABEL L0:
    T3 = *_T1
    T4 = *_T2
    T5 = CALL read
    T6 = T3 + T5
    *_T1 = T6
    T7 = T4 + #1
    *_T2 = T7
    T8 = *_T0
    IF T7 < T8 GOTO L0
LABEL L1:
    T9 = *_T0
    T10 = *_T1
    T11 = T10 / T9
    ARG T11
    CALL write
    T12 = #0
    RETURN T12
```

像这样，使用栈来存储局部变量，避免了多次赋值的问题，成功地生成了符合 SSA 规范的 IR。如果你使用的是 Accipit IR，你在完成 Lab 3 之后就已经是这样的形式了。

虽然这种方法确实能快速生成 SSA 形式的中间表示，但把所有局部变量都存储在栈上，不仅会多出许多无用的存取内存的开销（如上述例子中的 `N`），也不利于后续的常量传播、死代码消除等优化。因此，我们需要在此基础上，将其中分配的内存空间替换为临时变量，并在合适的地方插入 `PHI` 函数并进行重命名来得到符合 SSA 规范的中间表示，这一过程也被称作 mem2reg。

例如，对于上面使用栈的代码，`_T0` `_T1` `_T2` 对应栈上的位置可以替换为 `_T0` `_T1` `_T2` 变量，然后在 `L0` 标签处插入 `PHI` 函数：

```c linenums="1" hl_lines="3 5 7 9 10 13 15 16 19 20"
FUNCTION main:
    T0 = CALL read
    _T0 = T0
    T1 = #0
    _T1 = T1
    T2 = #0
    _T2 = T2
LABEL L0:
    T3 = PHI [main, _T1], [L0, _T1]
    T4 = PHI [main, _T2], [L0, _T2]
    T5 = CALL read
    T6 = T3 + T5
    _T1 = T6
    T7 = T4 + #1
    _T2 = T7
    T8 = _T0
    IF T7 < T8 GOTO L0
LABEL L1:
    T9 = _T0
    T10 = _T1
    T11 = T10 / T9
    ARG T11
    CALL write
    T12 = #0
    RETURN T12
```

再对不同块的 `_T0` `_T1` `_T2` 进行重命名，就可以得到符合 SSA 规范的中间表示：

```c linenums="1" hl_lines="3 5 7 9 10 13 15 16 19 20"
FUNCTION main:
    T0 = CALL read
    _T0_0 = T0
    T1 = #0
    _T1_0 = T1
    T2 = #0
    _T2_0 = T2
LABEL L0:
    T3 = PHI [main, _T1_0], [L0, _T1_1]
    T4 = PHI [main, _T2_0], [L0, _T2_1]
    T5 = CALL read
    T6 = T3 + T5
    _T1_1 = T6
    T7 = T4 + #1
    _T2_1 = T7
    T8 = _T0_0
    IF T7 < T8 GOTO L0
LABEL L1:
    T9 = _T0_0
    T10 = _T1_1
    T11 = T10 / T9
    ARG T11
    CALL write
    T12 = #0
    RETURN T12
```

进一步地，通过复制传播，即可获得 `PHI` 函数介绍部分中的更简洁的中间表示。

在通过 mem2reg 转化生成 SSA 的过程中，你需要经过插入 `PHI` 函数和变量重命名两个阶段。

1. **插入 `PHI` 函数**：你需要记录局部变量相关的 `DEC` `LOAD` `STORE` 指令，并按照块的支配关系在块的开头插入 `PHI` 函数。你需要对 `DEC` `LOAD` `STORE` 指令进行改写，以保证程序语义的正确性。在遍历一个基本块的所有指令后，维护该基本块的所有后继基本块中的 `PHI` 指令。
2. **变量重命名**：在此之后，我们需要对变量进行重命名，在保证正确性的同时，确保每个变量只被赋值一次。

或许你已经注意到，这种将局部变量先存到栈上再取出来的方式简直是多此一举。实际上，我们可以更加优雅地绕过这个冗余过程：我们不需要将所有局部变量都保存到栈上，而是直接在中间代码中继续使用这些变量，然后仅针对这些局部变量，进行插入 `PHI` 函数的操作，最后对变量进行重命名以完成 SSA 形式的构造。

例如，在求 N 个数均值的例子中，我们仅需关注那些局部变量（如例子中的 `N`、`sum` 和 `count`），它们在中间代码中对应的临时变量（如 `T0`、`T1`、`T2`）可能会被重复赋值。而其他临时变量则只会被赋值一次。**你需要保证你生成的中间代码也是符合这个规则的。**

!!! tip "必要的支配关系分析"
    在插入 `PHI` 函数前，需要先对基本块进行控制流分析，找到支配关系。这其中有许多新的概念，例如支配关系、支配树、支配边界等，但这些概念其实并不复杂，在 Bonus 3 的 [支配关系分析](../bonus/optimization.md#_4) 中也做了详细介绍。

具体的插入 `PHI` 函数以及变量重命名的过程，在 [SSA Book](https://pfalcon.github.io/ssabook/latest/book-full.pdf) 3.1 节中提供了伪代码，你可以参考这个算法来实现这个过程。书中的例子十分详细，感兴趣的同学可以阅读书中的内容，构建符合 SSA 的中间表达。书中的伪代码有些许纰漏，你可以结合书中的例子和你自己的理解进行修改。我们这里提供一个参考：

1. **插入 `PHI` 函数**：对于这类活跃在多个块或重复定义的变量，我们需要对所有定义他们的块的支配边界中插入 `PHI` 函数。显然，在支配边界中插入 `PHI` 函数会引入对这个变量的新的定义点，因此，我们需要迭代地插入 `PHI` 函数，直到没有新的需要插入 `PHI` 函数的基本块为止。在下面的伪代码中，`variable v which need to insert PHI function in G` 指的就是我们上面提到的这种局部变量。

    ``` title="Pseudo Code for Insert PHI" linenums="1"
    Function InsertPHI(G: ControlFlowGraph):
        For each variable v which need to insert PHI function in G:
            F <- {}     // Set of basic blocks where PHI is added
            W <- {}     // Set of basic blocks that contain definitions of v
            For each instruction d which defines v:
                Let B be the basic block containing d
                W <- W U {B}
            While W is not empty:
                Remove a basic block X from W
                For each Y in DF(X) - F:
                    Insert v = PHI [P, v], ... (for each predecessor P of Y) at entry of Y
                    F <- F U {Y}
                    If Y not in Defs(v):
                        W <- W U {Y}
    ```

2. **变量重命名**：我们需要确保每个变量只被赋值一次，且不会活跃在多个块，这可以通过维护一个变量的别名表来实现。需要注意的是，下面伪代码中的参数 `aliases` 传递的是一个深拷贝，递归调用时对它的修改不会影响到上一层的 `aliases`。我们这里的伪代码看起来和 [SSA Book](https://pfalcon.github.io/ssabook/latest/book-full.pdf) 3.1 节中的伪代码不太一样，其实只是将书中的递推形式还原回了递归形式，便于你理解。

    ``` title="Pseudo Code for Rename Variables" linenums="1"
    Function RenameVariables(G: ControlFlowGraph):
        For each variable v in G:
            aliases[v] <- ⊥
        Rename(G.EntryBlock, aliases)

    Function Rename(B: BasicBlock, aliases: Map[Variable -> Variable]):
        For each x = PHI ... in B:
            x' <- new variable
            aliases[x] <- x'
        For each non-PHI-instruction I in B:
            If I := (dst, ...) is a definition:
                dst' <- new variable
                aliases[dst] <- dst'
                Replace dst with dst' in I
            For each variable v used in I:
                Replace v with aliases[v] in I
        For each successor block C of B:
            For each x = PHI ... in C:
                Replace [B, x] with [B, aliases[x]] in PHI parameters
        For each child block C of B in the dominator tree:
            Rename(C, aliases)
    ```

!!! tip "尽管带有 `PHI` 函数的 SSA 更利于后续的优化，但是需要进行支配关系分析、插入 `PHI` 函数和变量重命名等操作。如果你对这些内容没兴趣，也可以直接生成不带 `PHI` 函数的 SSA 中间表示用于后续的优化，当然效果也会打一定折扣。"

### 直接生成 SSA 中间表示

当然，我们也可以直接生成符合 SSA 规范的中间表示。这里我们提供一个参考资料 [SSA.to](https://ssa.to/static-analysis-guide/intro)，具体内容不再赘述。

## 消除 `PHI` 函数

!!! info "完成本部分最高可获得 1 分加分"

显然，`PHI` 语句没有对应的 RISC-V 汇编指令，因此我们需要将其转换为普通的中间表示，也就是将 `PHI` 语句消除。一种简单的方法是在每一个基本块后面插入 `MOVE` 语句，将 `PHI` 函数的参数赋值给变量。例如，对于上面的代码，我们可以将 `PHI` 函数消除为：

```c hl_lines="5 6 8 9 13 14" linenums="1"
FUNCTION main:
    T0 = CALL read  // N
    T1 = #0         // sum
    T2 = #0         // count
    T10 = T1
    T11 = T2
LABEL L0:
    T3 = T10
    T4 = T11
    T5 = CALL read
    T6 = T1 + T5
    T7 = T2 + #1
    T10 = T6
    T11 = T7
    IF T7 < T0 GOTO L0
LABEL L1:
    T8 = T6 / T0
    ARG T8
    CALL write
    T9 = #0
    RETURN T9
```

这样，我们就消除了 `PHI` 函数，得到了一个不包含 `PHI` 函数的中间表示。

在 [SSA Book](https://pfalcon.github.io/ssabook/latest/book-full.pdf) 3.2 节中，提供了相关算法的伪代码，你可以参考这个算法来实现消除 `PHI` 函数。

## 基于静态单赋值的编译优化

在 SSA 中，每个变量都只被赋值一次，我们可以更轻松地维护使用-定义链（Use-Def Chain）和定义-使用链（Def-Use Chain），这样可以方便地进行各种优化。
<!-- 例如，前文中 N 个数求均值的程序 SSA 形式的中间表示的 Use-Def Chain 可以画成这样： -->

<!-- !!! warning "TODO: 画 Use-Def Chain 图" -->

不难发现，由于每个变量只被赋值一次，它们的 Use-Def Chain 都是单向的，不会出现互相依赖的情况。这样，我们就可以方便地进行各种优化。

### 公共子表达式消除与全局值编号

!!! info "完成本部分最高可获得 1 分加分"

如果两个变量的定义是一样的，那么这两个变量就是公共子表达式。例如，对于下面的代码：

```c
T3 = T1 + T2
T4 = T1 + T2
```

`T1 + T2` 就是一个公共子表达式。在 SSA 中，我们可以将这两个变量合并为一个变量：

```c
T3 = T1 + T2
T4 = T3
```

这样，我们就只需要做一次加法运算，而不是两次。（多余的 `MOVE` 操作在寄存器分配时会被 Coalescing 优化掉）

如果不使用 SSA，我们需要耗费大量精力判断 `T0` 和 `T3` 所用到的 `T1` 和 `T2` 是否是同一个值，而在 SSA 中，我们只需要看所用到的变量是否是同一个即可。

<!-- ### 全局值编号 -->

而**全局值编号（Global Value Numbering，简称 GVN）**可以看作是一个更强大的公共子表达式消除。在 GVN 中，我们不仅要找到公共子表达式，还要找到所有等价的表达式。例如，对于下面的代码：

```c
T0 = #3
T1 = #3
T2 = T0 + #3
T3 = T1 + #3
```

显然，`T1` 和 `T0` 是等价的，因此 `T3` 和 `T2` 也是等价的。因此，上面的代码可以优化为：

```c
T0 = #3
T1 = T0
T2 = T0 + #3
T3 = T2
```

为了实现 GVN，我们需要维护一个等价类，每个变量都属于一个等价类，等价类中的变量是等价的。当我们遇到一个新的变量时，我们需要判断它是否与等价类中的变量等价，如果是，则将其加入等价类；如果不是，则创建一个新的等价类。最后，我们将等价类中的变量合并为一个变量。

这看起来似乎很复杂，但我们可以借助 Hash 表来实现。对于每一个赋值语句，我们可以计算出一个 Hash 值：

```c
Hash(T2) = Hash(T0, +, #3) = Hash(Hash(T0), +, Hash(#3))
```

这样，我们就可以方便地判断两个变量是否等价。

在合并变量时，你同样要考虑到基本块之间的支配关系。你可以深度优先遍历支配树，将等价类向下传递；在每个基本块中，如果遇到某个变量在一个已有的等价类中，则将其与等价类中的变量合并：

``` title="Pseudo Code for GVN" linenums="1"
Function DFS_GVN(B: BasicBlock, T: HashTable[hash -> variable]):
    For each instruction I in basic block B:
        If I := (dst, ...) is a definition:
            h <- Hash(I)
            If h in T:
                replace I with (dst, T[h])
            Else:
                update T[h] <- dst
    For each child block C of B in the dominator tree:
        DFS_GVN(C, T)
```

??? question "为什么要深度优先遍历支配树"
    在 SSA 中，变量的定义点是唯一的，并且每个变量的定义点严格支配其所有的使用点。而在支配树中，一个节点是严格支配其所有子孙节点的。因此，我们利用 SSA 和支配树的性质，深度优先遍历支配树，可以保证我们在合并变量时，所用到的等价的变量一定是已经定义好的。

需要注意的是，上面的伪代码中的参数 `T` 传递的是深拷贝。这种方法会引入一些额外的 `MOVE` 操作，你可以自行修改实现逻辑，不使用 `MOVE`，而是将之后出现的变量直接替换为与他等价的变量，也可以在完成 GVN 后再做拷贝传播，或者留到寄存器分配时进行 Coalescing。

!!! tip "别忘了加法和乘法的交换律"
    由于加法和乘法的交换律，我们在计算 Hash 值时，需要考虑这两个运算符的交换律。在你设计 Hash 函数时，不要忘了它要满足如下性质：

    ```c
    Hash(a, +, b) = Hash(b, +, a)
    Hash(a, *, b) = Hash(b, *, a)
    ```

### 常量传播

!!! info "完成本部分最高可获得 1 分加分"

<!-- !!! warning "TODO: 加一个例子" -->

使用了 SSA 后，我们可以利用简洁的 Use-Def Chain 来进行常量传播。这里提供一种思路：

1. 遍历所有的语句，找到所有的常量赋值语句，将其加入一个常量表中。
2. 遍历所有的语句，对于每一个变量赋值语句，如果它的右值是一个常量，那么将其左值加入常量表中。
3. 重复第 2 步，直到没有新的常量加入为止。

在找到所有常量后，将它们替换为对应的常量值即可。

### 死代码消除

!!! info "完成本部分最高可获得 1 分加分"

<!-- !!! warning "TODO: 加一个例子" -->

同样地，利用 SSA 的 Use-Def Chain，我们可以方便地进行死代码消除：

1. 遍历所有的语句，找到所有的**必要值**变量，将其赋值语句加入一个集合中。
2. 遍历集合中的所有赋值语句，顺着 Use-Def Chain，将其所用到的变量的赋值语句加入集合中。
3. 重复第 2 步，直到没有新的赋值语句加入为止。

在找到所有必要值的赋值语句后，其余的指令就可以被认为是死代码，可以被消除。

那么，如何判断一个变量是必要值呢？必要值指的是在函数中要返回的值，或者会在函数外被使用的值。例如函数的返回语句（ `RETURN T0` 中的 `T0` ）、函数调用的参数（ `ARG T0` 中的 `T0` ）、修改全局变量和数组的语句（ `*T0 = T1` 中的 `T0` 和 `T1` ）等。

除此之外，在进行常量传播后，我们还可以发现一些条件分支中的常量条件，这样我们就可以进一步简化条件分支。对于这类不可达代码的消除，可以参考以下步骤：

1. 按照常量传播中的做法，先获取一个常量表。
2. 遍历所有基本块，对于每一个条件分支，如果条件判断所需要的所有变量都在常量表中，那么就可以直接判断条件的值。
3. 如果条件为真，那么将指向条件为假的分支的边删除，并将 `IF` 语句改为对应的跳转到真分支的 `GOTO` 语句。反之亦然。
4. 刷新控制流图，重新计算支配关系，修改 `PHI` 函数对应来源，并去除不可达的基本块。