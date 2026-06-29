# Lab3实验报告：中间代码生成

## 1. 实验思路

本实验选择实现 Zero IR。整体思路是在 Lab 2 已经完成语法分析和语义分析的基础上，继续利用 AST 和符号表信息，将 SysY 程序翻译为可以被 Zero IR 解释器执行的中间代码。实现时没有重新设计一套 IR，而是严格按照实验文档中给出的 Zero IR 指令格式生成代码，包括`FUNCTION`,`PARAM`,`ARG`,`CALL`,`RETURN`等等指令

代码结构上，我主要围绕几个翻译函数展开。

表达式部分通过`translateExp(exp, place)`实现，其含义是将表达式`exp`的计算结果放入指定的临时变量`place`中。常量表达式翻译为立即数加载；变量表达式根据变量类型翻译为普通复制或内存加载；二元表达式先递归翻译左右子表达式，在生成对应的二元运算指令；函数调用则优先翻译实参并生成连续的`ARG`指令，最后生成`CALL`指令

语句部分通过递归遍历`AST`生成顺序`IR`。赋值语句先翻译右值表达式，再根据左值是普通变量还是数组元素决定生成`store`或数组地址写入。`return`语句先将返回表达式翻译到临时变量，再生成`RETURN`。`if`和`while`语句则结合标签和跳转指令实现控制流。为了更好地处理条件判断，我单独实现了`translateCond(cond, true_label, false_label)`，让条件表达式不只是生成一个`0/1`值，而是直接根据真假跳转到对应标签

变量和数组处理事本实验中比较重要的部分。对于普通局部标量变量，我没有直接把变量名反复赋值，而是为其分配一个`.addr`形式的内存槽，后续通过`load/store`读写，这样可以统一处理多次赋值、循环更新和函数参数等修改等情况。对于数组，我记录数组维度信息，并根据下标计算字节偏移量；一维数组按照`i*4`计算，多维数组按照行优先规则计算，例如`a[i][j]`的偏移为`i * dim2 * 4 + j * 4`。对于全局变量和全局数组，则使用`GLOBAL`声明，并在使用时先通过`&label`获取地址，再进行读写

## 2. 难点与亮点

### 2.1 难点

本实验最大的难点是变量作用域和重名的问题。`Zero IR`本身没有源语言中的块级作用域。如果不同作用域中存在同名变量，直接使用原始变量名会导致IR中变量互相覆盖。为了解决这个问题，我使用语义分析阶段符号表中保存的`unique_name`作为IR层面的变量名。这样即使源程序中多个块内都定义了同名变量，翻译到`IR`层面的变量名。这样即使源程序中多个块内都定义了同名变量，翻译到`IR`后仍然是不同变量，从而避免作用域冲突

第二个难点是局部变量的更新问题。最开始直接用`x = value`的方式处理局部变量，看起来比较简单，但在循环、多次复制、参数修改等航警中容易出现值更新不一致的问题。后来我将局部变量同意抽象为内存槽，例如`.addr`，变量定义时用`DEC x.addr #4`分配空间，赋值时写入改地址，读取时从该地址加载。这样做虽然生成的`IR`稍微多一些，但是语义更稳定也更接近真实编译器中局部变量存放在栈上的处理方式

第三个难点是初始化列表的处理。全局数组和局部数组都可能出现部分初始化，例如`int a[4] = {1, 2};`，未显式初始化的部分应该补`0`。对于多维数组，初始化列表还可能嵌套。因此我实现了初始化列表展开逻辑，将嵌套的`InitList`按数组布局拍平成一维序列，再根据数组总容量进行阶段或者补零。这样可以统一生成`GLOBAL a:#size = ...`或局部数组的逐元素`StoreOffset`初始化代码

第四个难点是数组和指针语义。数组在 `IR` 中本质上要按地址处理，而不是普通整数值。局部数组通过 `DEC` 得到首地址，全局数组通过 `&label` 得到首地址，函数数组参数本身也被视为地址。对于 `a[i]` 这样的访问，需要先计算元素地址，再根据语境决定是读取该地址中的值，还是把该地址作为子数组实参继续传递。尤其是二维数组和数组参数结合时，如果把 `a[i]` 错误地当作普通整数读取，就会导致排序、矩阵、图算法等测试点出错。最终我通过记录数组维度，并在下标数量少于数组维度时返回子数组地址，解决了这类问题。

### 2.2 亮点

本实验的亮点在于没有只追求生成看起来像的IR，而是围绕解释执行语义做了较为完整的处理。标量变量统一内存话，数组地址统一计算、全局变量使用地址访问、函数参数区分标量和数组、短路日傲剑直接生成控制流，这些设计使得表达式、语句、函数、数组和全局变量之间的处理方式比较统一。

## 3. 心得体会

中间代码生成并不是简单地把`AST`按照格式打印出来，而是要真正保证`IR`的执行语义和源程序一致。表达式、赋值、条件跳转、函数调用看起来都比较直接，但一旦涉及变量作用域、函数参数、数组地址和短路逻辑，就很容易出现细节错误。

这次实验中最难的是区分“值”和“地址”。例如普通变量、局部数组、全局变量、数组参数在源代码里都表现为标识符，但翻译到 IR 时处理方式完全不同。局部数组名可以看作首地址，全局变量需要先取地址，数组元素访问还要计算偏移量。通过调试数组和函数参数相关测试，我对数组本质上按地址访问这一点理解更深了。

总体来说，这次实验让我更清楚地理解了 AST 到 IR 的转换过程，也让我认识到编译器实现中“格式正确”和“语义正确”是两件不同的事。只有把变量存储、控制流、函数调用和数组寻址这些细节都处理好，生成的 IR 才能真正被正确执行。

## 4. 附加内容

**借鉴声明**

- 要参考实验文档和模板代码：说明参考了课程提供的 Lab 3 文档和 Zero IR 模板。

**AI工具使用记录**

- `ChatGpt`:用于理解实验文档、解释`Zero IR`指令、分析编译报错
- `Codex`:用于辅助修改`IR`生成代码、记录`update notes`

![alt text](image.png)
![alt text](image-1.png)
![alt text](image-2.png)
![alt text](image-3.png)
![alt text](image-4.png)
![alt text](image-5.png)
![alt text](image-6.png)
![alt text](image-7.png)
![alt text](image-8.png)
![alt text](image-9.png)
![alt text](image-10.png)
![alt text](image-11.png)
![alt text](image-12.png)
![alt text](image-13.png)
![alt text](image-14.png)
![alt text](image-15.png)
![alt text](image-16.png)
![alt text](image-17.png)
![alt text](image-18.png)
![alt text](image-19.png)
![alt text](image-20.png)
![alt text](image-21.png)
![alt text](image-22.png)
![alt text](image-23.png)
![alt text](image-24.png)
![alt text](image-25.png)
![alt text](image-26.png)
![alt text](image-27.png)
![alt text](image-28.png)
![alt text](image-29.png)
![alt text](image-30.png)
![alt text](image-31.png)
![alt text](image-32.png)
![alt text](image-33.png)
![alt text](image-34.png)
![alt text](image-35.png)
![alt text](image-36.png)
![alt text](image-37.png)
![alt text](image-38.png)
![alt text](image-39.png)
![alt text](image-40.png)
![alt text](image-41.png)
![alt text](image-42.png)
![alt text](image-43.png)
![alt text](image-44.png)
![alt text](image-45.png)
![alt text](image-46.png)
![alt text](image-47.png)
![alt text](image-48.png)
![alt text](image-49.png)
![alt text](image-50.png)
![alt text](image-51.png)
![alt text](image-52.png)
![alt text](image-53.png)
![alt text](image-54.png)
![alt text](image-55.png)
![alt text](image-56.png)
![alt text](image-57.png)
![alt text](image-58.png)
![alt text](image-59.png)
![alt text](image-60.png)
![alt text](image-61.png)
![alt text](image-62.png)
![alt text](image-63.png)
![alt text](image-64.png)
![alt text](image-65.png)
![alt text](image-66.png)
![alt text](image-67.png)
![alt text](image-68.png)
![alt text](image-69.png)
![alt text](image-70.png)
![alt text](image-71.png)
![alt text](image-72.png)
![alt text](image-73.png)
![alt text](image-74.png)
![alt text](image-75.png)
![alt text](image-76.png)
![alt text](image-77.png)
![alt text](image-78.png)
![alt text](image-79.png)
![alt text](image-80.png)
![alt text](image-81.png)
![alt text](image-82.png)
![alt text](image-83.png)
![alt text](image-84.png)
![alt text](image-85.png)
![alt text](image-86.png)
![alt text](image-87.png)
![alt text](image-88.png)
![alt text](image-89.png)
![alt text](image-90.png)
![alt text](image-91.png)
![alt text](image-93.png)