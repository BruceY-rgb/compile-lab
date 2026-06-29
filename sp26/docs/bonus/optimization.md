# Bonus 3: 编译优化

!!! info "开始之前"
    请务必在 **2026-6-24 23:59** 前完成实验并推送到你自己对应的远程仓库的 `bonus3` 分支中，否则将会按照评分标准扣分。

    建议你在开始前，先使用 `git checkout -b bonus3 lab4` 从 `lab4` 分支创建一个新的 `bonus3` 分支，并使用 `git push -u origin bonus3` 在远程仓库中创建该分支，这样你就可以在实验过程中随时使用 `git push origin bonus3` 提交代码，而不会误交到其他分支。

!!! info "Bonus 3 最高可获得 5 分加分"

在基础实验中，我们实现了一个非常 naïve 的编译器，它仅仅将 SysY 语言翻译为 RISC-V 汇编代码，并没有做任何形式的优化，主打一个能跑就行。在这个 Bonus 实验中，我们将会介绍一部分编译优化的技术，你可以尝试实现这些优化技术，来提高你的编译器的性能。

<!-- https://groups.seas.harvard.edu/courses/cs153/2019fa/lectures/Lec19-Optimization.pdf -->

其中，**控制流分析**、**数据流分析**以及**基于图染色的寄存器分配算法**几乎是每年期末考的**必考内容**，推荐同学们可以优先实现这部分内容。窥孔优化部分也有一些容易实现的小 trick。在此基础上，可以优先参考附录 [静态单变量 SSA](../appendix/ssa.md)，通过 SSA 形式实现全局优化，以显著提升优化效果。如果希望进一步优化性能，可以尝试实现循环优化，这些优化在性能提升方面有显著效果。

## 控制流与数据流分析

在编译优化过程中，控制流分析和数据流分析是一个重要步骤，特别是在寄存器分配前的准备阶段。这部分的核心任务包括基本块划分、活跃变量分析和支配关系分析等等，为后续如寄存器分配、循环展开等优化提供关键信息。

### 基本块划分

在虎书第 8 章的教学中，我们已经学习了基本块的概念。**基本块（Basic Block）**是程序中一段具有以下特性的指令序列：

- 第一个指令是一个 `LABEL`，即基本块的入口。
- 最后一个指令是一个 `GOTO` 或 `IF`，即基本块的出口。
- 在基本块内部，没有其他的 `LABEL`, `GOTO`, `IF` 指令。

对基本块划分的算法描述如下：

1. 从头到尾扫描指令序列。
2. 每当遇到 `LABEL` 时，开始一个新的基本块（结束前一个基本块）。
3. 每当遇到 `GOTO` 或 `IF` 指令时，结束当前基本块（并开始下一个基本块）。
4. 添加必要的 `LABEL` 和 `GOTO` 指令。

之后，我们可以分析基本块的 Traces，去除不可达的基本块，合并连续的基本块等，由于课程中已经讲解了这部分内容，这里不再赘述。

例如，对于以下代码：

```c linenums="1"
FUNCTION Example:
    T0 = #0
    T1 = #10
    T2 = #0
LABEL L0:
    IF T0 < T1 GOTO L3
    GOTO L2
LABEL L3:
    IF T2 < T1 GOTO L1
    GOTO L2
LABEL L1:
    T2 = T2 + T0
    T0 = T0 + #1
    GOTO L0
LABEL L2:
    RETURN T2
```

我们可以划分出以下基本块：

```c linenums="1"
// Basic Block Example
FUNCTION Example:
    T0 = #0
    T1 = #10
    T2 = #0
    GOTO L0
// Basic Block L0
LABEL L0:
    IF T0 < T1 GOTO L3
// Basic Block Example_N0
LABEL Example_N0:
    GOTO L2
// Basic Block L3
LABEL L3:
    IF T2 < T1 GOTO L1
// Basic Block Example_N1
LABEL Example_N1:
    GOTO L2
// Basic Block L1
LABEL L1:
    T2 = T2 + T0
    T0 = T0 + #1
    GOTO L0
// Basic Block L2
LABEL L2:
    RETURN T2
```

根据每个块最后的 `GOTO` 或 `IF` 指令，我们可以将基本块连接起来，形成**控制流图（CFG, Control Flow Graph）**，如下所示：

![Control Flow](../assets/image/controlflow.svg#only-light)
![Control Flow](../assets/image/controlflow.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

### 活跃变量分析

在虎书第 10 章中，我们学习了数据流分析中最核心的两个公式：
$$
in[n] = use[n] \cup (out[n] - def[n]) \\\\
out[n] = \bigcup_{s \in succ[n]} in[s]
$$
在课堂上，我们已经介绍了利用上述两个公式进行活跃变量分析的方法。不过，在实际应用中，我们可以先以基本块为单位计算 `live_in` 和 `live_out`，然后再扩展到基本块内的每一条指令。这样可以减少计算量，提高效率：

```c title="Pseudocode for Liveness Analysis" linenums="1"
Function LivenessAnalysis(G: ControlFlowGraph):
    // Analyze liveness for each basic block
    For each node (basic block) n in G:
        in[n] = {}; out[n] = {}
    Repeat
        For each node n in G:
            in'[n] <- in[n]; out'[n] <- out[n]
            in[n] <- use[n] ∪ (out[n] - def[n])  // Equ. 1
            out[n] <- ∪_{s in succ[n]} in[s]     // Equ. 2
    Until in[n] = in'[n] and out[n] = out'[n] for all n

    // Propagate liveness to each instruction
    For each node n in G:
        current_out <- out[n]
        For each instruction i in n in reverse order:
            out[i] <- current_out
            in[i] <- use[i] ∪ (out[i] - def[i]) // Equ. 1
            current_out <- out[i]               // Equ. 2
```

!!! tip "在实现中，可以将基本块和指令封装成类，并为其定义 `use`、`def`、`live_in` 和 `live_out` 等属性。"

为了方便分析，对于函数调用 `CALL` 指令，它的 `use` 集合应该包含所有用到的参数寄存器（比如，两个参数的函数的 `use` 集合应该包含 `a0` 和 `a1`），而 `def` 集合应该包含所有可能被修改的寄存器（例如函数返回值 `a0`，调用者保存寄存器 `t0-t6` 等）。

例如，上面的代码中，我们可以计算出每个基本块的 `live_in` 和 `live_out` 集合，然后再逐条指令计算 `live_in` 和 `live_out` 集合，最终结果如下：

```c linenums="1"
// Basic Block Example     in = {},             out = {T2, T0, T1}
FUNCTION Example:       // in = {},             out = {}
    T0 = #0             // in = {},             out = {T0}
    T1 = #10            // in = {T0},           out = {T0, T1}
    T2 = #0             // in = {T0, T1},       out = {T2, T0, T1}
    GOTO L0             // in = {T2, T0, T1},   out = {T2, T0, T1}
// Basic Block L0          in = {T2, T0, T1},   out = {T2, T0, T1}
LABEL L0:               // in = {T2, T0, T1},   out = {T2, T0, T1}
    IF T0 < T1 GOTO L3  // in = {T2, T0, T1},   out = {T2, T0, T1}
// Basic Block Example_N0  in = {T2},           out = {T2}
LABEL Example_N0:       // in = {T2},           out = {T2}
    GOTO L2             // in = {T2},           out = {T2}
// Basic Block L3          in = {T2, T0, T1},   out = {T2, T0, T1}
LABEL L3:               // in = {T2, T0, T1},   out = {T2, T0, T1}
    IF T2 < T1 GOTO L1  // in = {T2, T0, T1},   out = {T2, T0, T1}
// Basic Block Example_N1  in = {T2},           out = {T2}
LABEL Example_N1:       // in = {T2},           out = {T2}
    GOTO L2             // in = {T2},           out = {T2}
// Basic Block L1          in = {T2, T0, T1},   out = {T2, T0, T1}
LABEL L1:               // in = {T2, T0, T1},   out = {T2, T0, T1}
    T2 = T2 + T0        // in = {T2, T0, T1},   out = {T2, T0, T1}
    T0 = T0 + #1        // in = {T2, T0, T1},   out = {T2, T0, T1}
    GOTO L0             // in = {T2, T0, T1},   out = {T2, T0, T1}
// Basic Block L2          in = {T2},           out = {}
LABEL L2:               // in = {T2},           out = {T2}
    RETURN T2           // in = {T2},           out = {}
```

### 支配关系分析

在编译器优化中，支配关系分析是必不可少的一环，尤其是在进行循环优化或将代码转换为 SSA（Static Single Assignment）形式时。支配关系用于描述程序控制流图中节点间的控制流约束关系。

#### 支配关系

在控制流图中，如果一个基本块 A 在执行上**一定会先于**另一个基本块 B 执行，那么我们说 A 支配 B。形式化地，如果每一条从开始节点 `s_0` 到节点 `n` 的路径都经过 `d`，则称 `d` **支配（Dominates）**`n`，`d` 是 `n` 的**支配者（Dominator）**。支配关系具有以下性质：

- 每一个节点都支配它自己。
- 每一个节点（除开始节点）都可以有多个支配者。

不难发现，支配关系图是一个有向图。寻找支配关系的核心公式如下：
$$
\begin{aligned}
D[s_0] &= \{s_0\} \\\
D[n] &= \{n\} \cup \left( \bigcap_{p \in pred[n]} D[p] \right), \forall n \neq s_0
\end{aligned}
$$

我们可以利用上述公式，采用迭代法计算支配关系，先初始化所有节点的支配者集合为图中所有节点，然后根据公式逐步收敛到正确结果。伪代码如下：

```c title="Pseudocode for Computing Dominators" linenums="1"
Function ComputeDominators(G: ControlFlowGraph):
    // Initialize to the universal set
    For each node n in G:
        D[n] <- G.V
    D[s_0] <- {s_0}
    Repeat
        For each node n in G:
            D'[n] <- D[n]
            D[n] <- {n} ∪ (∩_{p in pred[n]} D[p])
    Until D[n] = D'[n] for all n    
```

例如，对于上面例子中的 `L2` 基本块，从开始节点 `Example` 到 `L2` 一共有三条路径，如下图三种不同颜色的路径所示。显然这三条路径都经过 `Example`, `L0` 和 `L2`，因此 `D[L2] = {Example, L0, L2}`。

![Dominator](../assets/image/dominator.svg#only-light)
![Dominator](../assets/image/dominator.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

通过上面的伪代码，我们可以计算出所有节点的支配者集合，最终结果如下：

| Node | Dominators |
|------|------------|
| `Example` | `{Example}` |
| `L0` | `{Example, L0}` |
| `Example_N0` | `{Example, L0, Example_N0}` |
| `L3` | `{Example, L0, L3}` |
| `Example_N1` | `{Example, L0, L3, Example_N1}` |
| `L1` | `{Example, L0, L3, L1}` |
| `L2` | `{Example, L0, L2}` |

#### 直接支配者与支配树

**直接支配者（Immediate Dominators）**是指一个基本块的最直接的、最近的支配者。具体来说，对于每一个除开始节点外的节点 `n`，都有且仅有一个直接支配者 `idom[n]`，满足以下条件：

- `idom[n]` 严格支配 `n`，即 `idom[n]` 支配 `n` 且 `idom[n] != n`。
- `idom[n]` 不支配任何其他支配 `n` 的节点。

例如，对于上面的例子，`L2` 的直接支配者就是 `L0`，`L0` 的直接支配者是 `Example`。
 
我们可以通过以下伪代码计算直接支配者：

```c title="Pseudocode for Computing Immediate Dominators" linenums="1"
Function ComputeIdom(G: ControlFlowGraph):
    For each node n in G:
        idom[n] <- null
    For each node n in G except s_0:
        candidates <- dom[n] - {n}
        For each p in candidates:
            If candidates - {p} is a subset of dom[p]:
                idom[n] <- p
```

基于直接支配者关系 `idom[n] -> n`，我们可以构建**支配树（Dominance Tree）**，以开始节点 `s_0` 为根节点。支配树在优化过程中非常重要，深度优先遍历支配树可以高效传递优化信息。比如上面的例子的支配树如下：

![Dom Tree](../assets/image/domtree.svg#only-light)
![Dom Tree](../assets/image/domtree.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

#### 支配边界

**支配边界（Dominance Frontier）**`DF[n]` 是节点 `n` 的一个特性，用于标识 `n` 对哪些节点的支配关系被打破，它由所有那些**被基本块 `n` 支配的基本块的出口**的点组成。具体地说，节点 `w` 属于 `DF[n]` 当且仅当：

- `n` 不严格支配 `w`。
- `n` 支配 `w` 的某个前驱节点 `p`。

这里我们给出一种计算支配边界的算法：

```c title="Pseudocode for Computing Dominance Frontier" linenums="1"
Function ComputeDF(G: ControlFlowGraph):
    For each node n in G:
        DF[n] <- {}
    For each node n in G:
        For each p in pred[n]:
            x <- p
            While x != idom[n]:
                DF[x] <- DF[x] ∪ {n}
                x <- idom[x]
```

比如在上面的例子中，对于 `L1`，由于它只支配它自己，因此它的支配边界就是自己的后继节点 `{L2}`。对于 `L3`，它支配 `Example_N1` 和 `L1` 两个节点，因此它的支配边界是这两个节点的后继节点 `{L0, L2}`。通过上述伪代码，我们可以计算出所有节点的支配边界，最终结果如下：

| Node | Dominance Frontier |
|------|--------------------|
| `Example` | `{}` |
| `L0` | `{L0}` |
| `Example_N0` | `{L2}` |
| `L3` | `{L0, L2}` |
| `Example_N1` | `{L2}` |
| `L1` | `{L0}` |
| `L2` | `{}` |


支配边界的概念在插入 `PHI` 函数以构造 SSA 形式时尤为重要，它决定了在哪些位置需要合并变量值。

## 寄存器分配

<!-- [Register allocation - Wikipedia](https://en.wikipedia.org/wiki/Register_allocation) -->

寄存器分配直接影响程序性能和寄存器使用效率。在此，我们介绍两种经典的寄存器分配算法：

### 基于图染色的寄存器分配

!!! info "完成本部分内容最高可获得 2 分加分"

<!-- !!! warning "TODO: 简要展开一下" -->

在虎书第 11 章，我们已经深入探讨了 [TOPLAS 1996: Iterated Register Coalescing](https://dl.acm.org/doi/pdf/10.1145/229542.229546) 中，George 等人提出的一种基于图染色的寄存器分配算法。你可以尝试实现该算法以完成寄存器分配任务。实现时，可以参考教材中的伪代码，或直接参考原论文。在论文的附录部分，提供了完整的伪代码实现，与教材中的内容一致。

!!! tip "建议你自己实现一遍基于图染色的寄存器分配算法，帮助你更好地理解这个算法，以准备期末考试。"

### 基于线性扫描的寄存器分配

!!! info "完成本部分内容最高可获得 1 分加分"

考虑到基于图染色的算法复杂性较高，且时间复杂度较大，[TOPLAS 1999: Linear Scan Register Allocation](https://dl.acm.org/doi/pdf/10.1145/330249.330250) 中，Poletto 等人提出了一种基于线性扫描的寄存器分配算法。该算法通过单次线性扫描变量的活跃范围来分配寄存器。相比于基于图染色的算法，线性扫描算法具有以下优势：

- **更高的效率**：时间复杂度低，运行速度更快。
- **实现简洁**：算法逻辑简单，易于实现。
- **接近的性能**：生成的代码效率几乎与更复杂的基于图染色算法相当。

在这篇论文中，作者也提供了详细的伪代码，供你参考实现。

## 循环优化

我们已经在虎书第 18 章学习过归纳变量分析、提取循环不变量、循环展开等，你可以将这些知识应用到你的编译器中，对循环进行优化。

<!-- https://groups.seas.harvard.edu/courses/cs153/2019fa/lectures/Lec23-Loop-optimization.pdf -->

<!-- https://www.cs.cornell.edu/courses/cs6120/2023fa/lesson/8/ -->

<!-- !!! warning "TODO: 加几个例子介绍一下，贴个参考文档" -->

### 循环基础概念

**反向边（Back Edge）**是控制流图中的一种特殊边，它从一个节点 `n` 指向该节点的支配者。利用反向边，我们可以识别一个控制流图中的所有循环（Loop）。对于反向边 `n -> h`，其对应的**自然循环（Natural Loop）**是一组节点 `x`，满足：

- `h` 支配 `x`，即 `h` 支配循环中的所有节点。
- 存在一条路径，从 `x` 到 `n` 且不经过 `h`。

这样一个循环的**头节点（Header）** `h` 是循环的入口，而它的前驱节点被称为**前驱头（Preheader）**。如果一个循环的头节点有多个前驱，我们可以通过插入一个前驱头来简化循环的分析。

**嵌套循环（Nested Loop）**是指一个循环中包含另一个循环。如果循环 `A` 和 `B` 的循环头不同，且循环 `B` 的循环头是循环 `A` 的内部节点，我们称循环 `B` 是循环 `A` 的内部循环。在进行循环优化时，我们通常从内层循环开始，逐层向外进行优化。

### 循环不变量提取

!!! info "完成本部分内容最高可获得 1 分加分"

**循环不变量（Loop Invariant）**是指在循环中不会改变的变量，通过将循环不变量提取到循环外部，可以减少重复计算，从而提高效率。在一个循环中，当一个赋值语句 `x = v1 op v2` 满足以下的其中一个条件时，我们说它是不变的：

- `vi` 是常量。
- 所有定义该处 `vi` 的语句都在循环之外。
- 只有一个定义该处 `vi` 的语句，且该语句也是循环不变的。

当我们找到所有循环不变的变量后，需要找到能够提取到循环外的语句。一个语句 `d: x = v1 op v2` 能被安全地提取到循环外，需要满足：

- `d` 支配所有 `x` 还在活跃的循环出口。
- 循环中只有一个 `x` 的定义语句。
- `x` 在循环入口处不是活跃变量。

同学们实现的 While 循环往往很难满足上述的第一个条件，比如下面的左侧代码，循环体 `L1` 中的所有语句都没法支配循环出口 `L0`，因此很难进行循环不变量提取。我们可以把 While 循环改为 Do-While 循环，如右图所示，这样，循环出口就变成了 `L1_0`，显然 `L1` 支配 `L1_0`，这样就可以很容易地提取循环不变量了。

![While to Do-While](../assets/image/while-to-do-while.svg#only-light)
![While to Do-While](../assets/image/while-to-do-while.svg#only-dark){ style="filter: invert(1) hue-rotate(180deg)" }

### 归纳变量

!!! info "完成本部分内容最高可获得 1 分加分"

如果一个循环中有一个变量 `i`，它是递增或者递减的，还有一个变量 `j = i * c + d`，其中 `c` 和 `d` 是循环不变的，显然我们可以在不引用 `i` 的情况下计算 `j` 的值。比如，如果每次循环 `i = i + a`，可以得到每次循环我们只需要做 `j = j + a * c`。这样的一个过程就是归纳变量优化。

首先，我们需要识别循环中的像 `i` 这样的变量，我们称这样的变量 `i` 是**基本归纳变量（Basic Induction Variable）**，它是循环中的一个变量，需要满足 `i` 在循环 `L` 中的唯一一次定义是 `i = i + c` 或 `i = i - c` 的形式，其中 `c` 是循环不变的。

接下来，我们可以找到循环中的像上文中 `j` 这样的变量，我们称这样的变量 `j` 是**派生归纳变量（Derived Induction Variable）**。一个变量 `k` 是循环 `L` 中的一个派生归纳变量，需要满足：

1. `L` 中只有唯一一次定义 `k` 的语句的形式应该是 `k = j * c` 或者 `k = j + c`，其中 `j` 是一个归纳变量，`c` 是循环不变的。
2. 如果 `j` 是 `i` 的归纳变量族中的变量（即 `j` 和 `i` 是线性相关的），则：
    - 到达 `k` 的 `j` 的唯一定义是在循环中的定义。
    - 在 `j` 的定义和 `k` 的定义之间不存在 `i` 的定义。

在找到归纳变量后，我们就可以进行强度削弱（Strength Reduction），因为做乘法的开销要比做加法的开销要大，我们可以将乘法操作替换为加法操作。对于每一个派生归纳变量 `j`，其中 `j = e1 * i + e0`，执行以下步骤：

1. 为 `j` 创建一个新的临时变量 `j'`。
2. 在循环的前驱头（Preheader）的末尾，初始化 `j'` 为 `e0`，即 `j' = e0`。
3. 在每次 `i = i + c` 后，添加 `j' = j' + e1 * c`，其中 `e1 * c` 可以在循环 Preheader 中先计算好，因为它是循环不变的。
4. 将循环中唯一一次对 `j` 的赋值操作改为 `j = j'`。

这可能会带来一些额外的开销，你可以进一步进行常量传播和死代码消除来优化这些额外的开销。

<!-- ### 循环展开 -->

<!-- !!! info "完成本部分内容最高可获得 1 分加分" -->

## 窥孔优化

!!! info "完成本部分内容最高可获得 1 分加分"
<!-- 南大 -->

窥孔优化（Peephole Optimization）是一种局部优化技术，可以用来优化中间代码或目标代码。窥孔指的是在目标代码中的一个小的滑动窗口，因此也可以称作滑动窗口优化。窥孔优化会检查目标代码的一个滑动窗口，尝试用执行速度更快或者更简短的指令替换当前滑动窗口中的指令。在你滑动你的窗口时，你可以关注以下几个方面：

### 冗余指令消除

在我们生成的中间代码或目标代码中，可能存在一些冗余的指令，可以通过窥孔优化来删除或者合并这些冗余指令，减少不必要的计算。

- **使用零寄存器替换常量零：**在我们使用的 RISC-V 汇编中，有一个特殊的寄存器 `zero` （ `x0` ），它的值永远为 0。如果你发现一个寄存器的值恒等于 0，你可以使用 `zero` 寄存器来代替这个寄存器，从而减少一些不必要的赋值操作，比如以下汇编代码：

    ```asm
    li t1, 0
    beq t0, t1, L1
    ```

    可以优化为：

    ```asm
    beqz t0, L1
    ```

- **指令合并：**将两条相邻的指令合并为一条，从而提高代码效率。例如：

    ```asm
    addi t1, t1, 8
    lw t2, 0(t1)
    ```

    可以优化为：

    ```asm
    lw t2, 8(t1)
    ```

- **删除冗余的跳转指令：**例如以下代码中的跳转指令可合并或删除：

    ```asm
        T0 = #1
        GOTO L2
    L2:
        GOTO L3
    L3:
        T1 = T0 + #1
    ```

    可以优化为：

    ```asm
        T0 = #1
        T1 = T0 + #1
    ```

- **删除不必要的内存访问：**如果你只使用了朴素的寄存器分配，还可以使用窥孔优化来消除一些多余的 `LOAD` 和 `STORE` 指令，例如对于以下代码：

    ```asm hl_lines="4 5" linenums="1"
    lw t0, 12(sp)   # load x
    lw t1, 16(sp)   # load y
    sub t2, t0, t1  # a = x - y
    sw t2, 20(sp)   # store a
    lw t0, 20(sp)   # load a
    lw t1, 24(sp)   # load z
    sub t2, t0, t1  # b = a - z
    sw t2, 28(sp)   # store b
    ```

    显然，高亮的两行 `lw` 和 `sw` 指令是多余的。我们可以使用寄存器 `t2` 来保存中间结果，从而消除这两条指令。消除后的代码如下：

    ```asm linenums="1"
    lw t0, 12(sp)   # load x
    lw t1, 16(sp)   # load y
    sub t2, t0, t1  # a = x - y
    lw t1, 24(sp)   # load z
    sub t0, t2, t1  # b = a - z
    sw t0, 28(sp)   # store b
    ```

### 强度削弱

**强度削弱（Strength Reduction）**是通过将计算代价高的操作替换为代价较低的等效操作来提高程序效率。特别是对于乘法和除法，可以使用位移操作代替，从而减少计算复杂度：

```c
a = a * 2       ->   a = a << 1
a = a / 2       ->   a = a >> 1
a = a * 10      ->   a = (a << 3) + (a << 1)
a = a / 32767   ->   a = (a >> 15) + (a >> 30)
```

<!-- ## 目标机器相关优化

别忘了，RISC-V 有一个特殊的寄存器 `zero` （ `x0` ），它的值永远为 0。在你的编译器中，你可以尝试将一些常量替换为 `zero` 寄存器，从而减少一些不必要的赋值操作。 -->

### 不可达代码消除

窥孔优化还可以用于消除目标代码中的一部分不可达代码，例如将有条件跳转 `IF` 替换为无条件跳转 `GOTO`，删除无条件跳转指令 `GOTO` 或返回指令 `RETURN` 之后的在下一个 `LABEL` 前的指令，重复执行这个删除过程，就可以保证将不可达的指令序列全部删除。例如，对于如下的中间代码：

```c linenums="1"
    T0 = #1
    T1 = #1
    IF T0 == T1 GOTO L1
    GOTO L2
LABEL L1:
    T2 = #1
    GOTO L3
LABEL L2:
    T2 = #2
LABEL L3:
```

在这里，我们可以发现第 3 行的 `IF` 语句永远为真，可以将其替换为 `GOTO L1`。之后，可以发现第 4 行的 `GOTO L2` 指令是不可达的，因此可以将其删除。删除后的代码如下：

```c linenums="1"
    T0 = #1
    T1 = #1
    GOTO L1
LABEL L1:
    T2 = #1
    GOTO L3
LABEL L2:
    T2 = #2
LABEL L3:
```

## 局部常量传播/折叠

!!! info "完成本部分内容最高可获得 1 分加分"

在程序中，如果一个变量的值是常量，我们可以将这个变量的值替换为常量。例如：

```c linenums="1"
FUNCTION main:
    T0 = #3
    T1 = T0 + #4
    T2 = T1 - #2
    T3 = T2 * #5
    T4 = T3 / #2
    T5 = CALL read
    T6 = T5 * #1
    T7 = T6 / #1
    T8 = T7 + #0
    T9 = T8 * #0
    IF T9 > T4 GOTO L1
    RETURN T9
LABEL L1:
    RETURN T4
```

经过常量传播并折叠后，可以优化为：

```c linenums="1"
FUNCTION main:
    T0 = #3
    T1 = #7
    T2 = #5
    T3 = #25
    T4 = #12
    T5 = CALL read
    T6 = T5
    T7 = T6
    T8 = T7
    T9 = #0
    IF #0 > #12 GOTO L1
    RETURN T9
LABEL L1:
    RETURN T4
```

!!! note "这里为了方便解释，使用了不符合我们 Zero IR 规范的 `IF` 语句"

对于常量传播的优化，一个 naïve 的解法是在一个基本块内，从前往后遍历，如果一个变量的值是常量，就将在这之后且在这个变量被再次赋值之前的所有变量的值替换为这个常量，再进行折叠，直到我们无法再替换为止。

对于跨越基本块的变量，由于涉及到控制流，我们需要进行更复杂的分析。我们推荐你采用静态单变量形式的中间代码表示，这样可以更方便地进行全局的常量传播。具体可参见附录 [静态单变量 SSA](../appendix/ssa.md)。

## 局部死代码消除

在此处，死代码指的是不可达的代码（即永远不会被执行到的代码）以及对程序状态没有影响的代码（例如 Write-After-Write 之类的无用赋值）。消除不可达代码可以减小生成代码的大小，消除无用计算还可以加快代码运行速度。

在我们对中间代码进行控制流分析后，我们可以轻松地发现不可达的基本块，将这些基本块剔除即可（别忘了，一些无用的 `GOTO` 语句也可以一起删除）。在进行常量传播后，我们或许也可以发现一些必然执行的分支。例如：

```c
T1 = #1
T2 = T1
IF T1 == T2 GOTO L1
GOTO L2
```

可以优化为：

```c
GOTO L1
```

对于无用赋值，一个 naïve 的解法是在一个基本块内，从后往前遍历，如果一个变量的值没有被使用，就将这个变量的赋值语句删除。对于跨越基本块的变量，同样地，我们需要基于控制流进行更复杂的分析，才能更好地进行全局的死代码消除，如果你采用了静态单变量形式的中间代码表示，这样的分析会更加容易。具体可参见附录 [静态单变量 SSA](../appendix/ssa.md)。

需要注意的是，有些赋值语句看似无用，实则存在一些副作用，例如对一个函数的调用，或者对一个全局变量的赋值，这些赋值语句可能会影响程序的状态，因此不能轻易删除。

!!! example "一个结合了常量传播和死代码消除的例子"

    如下代码：

    ```c linenums="1"
        T0 = #27
        T1 = CALL read
        T2 = T0 * #2
        T2 = T2 + T1
        IF T0 < #0 GOTO L1
        GOTO L2
    L1:
        T1 = T2 - #3
        GOTO L3
    L2:
        T1 = #12
    L3:
        ARG T1
        CALL write
    ```

    经过常量传播：

    ```c linenums="1"
        T0 = #27
        T1 = CALL read
        T2 = #54
        T2 = #54 + T1
        IF #27 < #0 GOTO L1 // will never goto L1
        GOTO L2
    L1:
        T1 = T2 - #3
        GOTO L3
    L2:
        T1 = #12
    L3:
        ARG T1
        CALL write
    ```

    再进行死代码消除：

    ```c linenums="1"
        T1 = CALL read  // can't be removed because of the side effect
        T1 = #12
        ARG T1
        CALL write
    ```

## 过程间优化

前面的部分我们讲的都是过程内优化，即在一个函数内部进行的优化。而过程间优化则是在多个函数之间进行的优化。这里我们介绍两种简单的过程间优化技术：

### 内联优化

!!! info "完成本部分内容最高可获得 0.5 分加分"

对于一些简单的函数，我们可以将函数调用内联到调用处，减少函数调用的开销。例如：

```c linenums="1"
FUNCTION add:
    PARAM a
    PARAM b
    T0 = a + b
    RETURN T0

FUNCTION main:
    T1 = CALL read
    T2 = CALL read
    ARG T1
    ARG T2
    T3 = CALL add
    ARG T3
    CALL write
    T4 = #0
    RETURN T4
```

可以优化为：

```c linenums="1"
FUNCTION main:
    T1 = CALL read
    T2 = CALL read
    T3 = T1 + T2
    ARG T3
    CALL write
    T4 = #0
    RETURN T4
```

如你所见，这样可以减少函数调用的开销，但如果有多处调用同一个函数，内联优化可能会导致代码膨胀，增加代码大小。

### 尾调用优化

!!! info "完成本部分内容最高可获得 0.5 分加分"

如果一个函数的最后一个操作是一个函数调用，我们可以将这个函数调用优化为跳转，这样可以减少函数调用造成的栈空间开销。例如：

```asm
foo:
    call bar
    call baz
    ret
```

可以优化为：

```asm
foo:
    call bar
    jmp baz
```

!!! tip "需要注意的是，我们的 Zero IR 的解释器和调试器暂不支持直接 GOTO 到另一个函数。如果你想要实现这个优化，可能需要对 IR 进行一些简单的改造。"

## 你的任务

接下来，就请你尽情优化你的编译器吧！你可以尝试实现指导中提到的优化技术，也可以探索其他的编译优化方法。

我们推荐完成**与期末考试高度相关**的一些优化，例如控制流与数据流分析、寄存器分配等。图染色寄存器分配算法几乎是每年的期末考试的必考内容。窥孔优化则是一个比较容易实现的小 trick，可以试着添加一些窥孔优化中提到的内容。在此基础上，我们推荐参考附录 [静态单变量 SSA](../appendix/ssa.md)，基于 SSA 进行优化，能够帮助你们更好地进行全局优化，如全局值编号、常量传播、死代码消除等等。循环相关优化也是一个能显著提升性能的优化方向，如循环不变量提取、归纳变量优化、循环展开等等。

在实验报告中，请**定量分析**你的优化对编译器性能的影响，例如编译时间、生成代码的大小、代码运行速度等（主要是代码运行速度）。我们还鼓励你将优化效果与 GCC 的不同优化等级（如 `-O1`, `-O2`, `-O3`）进行对比，挑战一下 GCC 的性能吧！:smiling_imp:

!!! tip "即使你的优化尝试未能显著提升性能，甚至带来了一些负面效果，或者只做了一些前置内容（例如只做了到 SSA 的转换、支配关系分析），仍然欢迎你提交成果，我们会给予一定的加分。"

如果你觉得目前的测试用例不够充分，你也可以自行设计测试用例。你提供的测试文件需要按照如 Lab 0 中所述的格式，添加对应的输入输出数据在开头的注释中，并以 `.sy` 为后缀，放在 `appends/bonus3` 文件夹下。我们的测试脚本会自动使用 Lab 4 的测试用例以及你提供的自定义测试用例进行测试。不过，我们更鼓励你将设计的符合 SysY 语法的高质量测试用例贡献到公共测试仓库中，以帮助更多同学测试和改进编译器。

与 Lab 4 不同的是，本次实验**只会使用 QEMU 进行测试**。你的编译器 compiler 必须支持**两个**命令行参数的情形，即：

```console
$ ./compiler <input_file> <output_file>
```

该程序必须接受一个输入的源代码文件名作为参数，并且将输出的汇编代码写入到指定的 `output_file` 文件中，我们只会使用这种方式来测试你的编译器。

## 测试

运行以下命令来测试你的编译器：

```console
$ python3 sp26-tests/test.py bonus3 .
```

在测试中，如果继续使用并行测试，大概率会影响 QEMU 的运行时间，因此我们建议你在测试时关闭并行测试，即修改 `config.toml` 中的 `parallel` 为 `false`：

```toml
parallel = false
```

## 实验提交

你需要将作为 Bonus 3 评分的代码放到 `bonus3` 分支中，并提交一个实验报告，**页数不限**，内容包括：

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
        - 上述 AI 工具使用的完整记录可存放于仓库的 `reports/appends/bonus3` 文件夹中，并在实验报告中简要说明。

!!! danger "实验报告存在以下情况的，将由助教现场验收，并相应扣分"
    - 若实验报告中**缺乏实现思路**或**未真实描述 AI 工具使用情况**，将由助教现场验收，并相应扣分。
    - 实验报告**禁止使用 AI**，包括但不仅限于直接生成报告内容、润色等。提交的报告将会使用 AI 检测工具进行检测。若 AI 生成的内容占比过高，或者检测出明显的 AI 生成痕迹，将由助教现场验收，并相应扣分。

我们只接受 PDF 格式的实验报告。你需要将报告放在仓库的 `reports/bonus3.pdf` 中。

你也需要将你自己设计的测试用例以 `.sy` 为后缀放在 `appends/bonus3` 文件夹中，以便我们的 CI 测试程序使用你的测试用例来测试。