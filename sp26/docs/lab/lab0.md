# Lab 0: 环境配置与测试用例编写

!!! info "开始之前"
    请先阅读实验指导文档[主页](../index.md)，了解实验的整体要求和评分标准。

    请务必在 **2026-3-15 23:59** 前完成实验并推送到你自己对应的远程仓库的 `lab0` 分支中，否则将会按照评分标准扣分。

    建议你在开始前，先使用 `git checkout -b lab0 main` 从 `main` 分支创建一个新的 `lab0` 分支，并使用 `git push -u origin lab0` 在远程仓库中创建该分支，这样你就可以在实验过程中随时使用 `git push origin lab0` 提交代码，而不会误交到其他分支。

## 实验环境配置

首先你需要克隆我们为你准备的 starter-code 仓库，所有实验都将基于这个仓库进行。我们已经在 ZJU Git 上的 `Compiler/2026/<授课班>` 项目组中为每一位同学创建了一个仓库，`<授课班>` 为你的所在班级的任课老师名字全拼，仓库命名规则为 `cp-<你的学号>`。例如，如果你是刘老师班级、学号为 `3230108888` 的同学，你可以通过以下命令将这个仓库克隆到本地：

=== "Clone with SSH"
    ```console
    $ git clone --recurse-submodules git@git.zju.edu.cn:compiler/2026/liuzhongxin/cp-3230108888.git
    $ cd cp-3230108888
    $ git remote -v
    origin    git@git.zju.edu.cn:compiler/2026/liuzhongxin/cp-3230108888.git (fetch)
    origin    git@git.zju.edu.cn:compiler/2026/liuzhongxin/cp-3230108888.git (push)
    ```

=== "Clone with HTTPS"
    ```console
    $ git clone --recurse-submodules https://git.zju.edu.cn/compiler/2026/liuzhongxin/cp-3230108888.git
    $ cd cp-3230108888
    $ git remote -v
    origin    https://git.zju.edu.cn/compiler/2026/liuzhongxin/cp-3230108888.git (fetch)
    origin    https://git.zju.edu.cn/compiler/2026/liuzhongxin/cp-3230108888.git (push)
    ```

!!! tip "ZJU Git 使用指南"
    为了做实验，你需要先配置好自己的 ZJU Git 账户。如果你之前没有使用过 ZJU Git，请你参考 [ZJU Git 101](https://www.pages.zjusct.io/git101/) 设置好自己的账户密码。如果你使用 SSH 进行克隆，还需要参考其中的 [3.2 设置 SSH 访问](https://www.pages.zjusct.io/git101/prep/setup_ssh.html) 章节设置好自己账户的 SSH 访问权限。

    关于 ZJU Git 的网络，同学们需要知晓：
    
    - ZJU Git 目前仅面向校内服务，**仅校内 DNS 有解析记录**。如果使用校外 DNS 或校外网络，则无法访问 ZJU Git。
    - 为了保障网络安全，学校信息中心封禁了 RVPN 的 22 端口，因此仅能访问 ZJU Git 网页，无法使用命令行 Git 以 SSH 方式连接。
    - **为此，ZJU Git 开放了 2222 端口供 RVPN 用户以 SSH 方式使用。**有校外网络需求的同学请注意这一点，将链接替换为 `ssh://git.zju.edu.cn:2222/...`。

!!! info "如果你没有在项目组中找到自己的仓库，请及时联系助教。"

为了能够进行实验，你需要以下四类工具：

1. **构建工具：**我们默认提供 Makefile 作为构建工具。
2. **编译器、词法和语法分析器：**我们默认使用 g++ & Flex & Bison，建议使用以下版本：
    - g++ 版本 13.2.0
    - Flex 版本 2.6.4
    - Bison 版本 3.8.2

    这是 CI 测试中所用到的工具的版本号，但一般来说只要你避免使用特别冷门的特性，其他版本也能正常完成实验。

3. **Python 3.10+：**我们的评测程序和实验 3 的中间代码解释器是基于 Python 3.10 编写的。
    
    推荐使用 [conda-forge/miniforge](https://github.com/conda-forge/miniforge) 或 [venv](https://docs.python.org/3/library/venv.html) 来管理环境。你还需要安装 `sp26-tests/requirements.txt` 中的依赖：`pip install -r sp26-tests/requirements.txt`。

4. **Java 或 RV32 工具链：**实验 4 的汇编模拟器需要 Java 运行或者 QEMU 运行，你可以选择等到实验 4 时再安装。

在后续实验中，我们提供的实验框架使用 C++ 作为编程语言，并使用 Makefile 作为构建工具。如果你选择不使用提供的框架代码，而是希望改用其他编程语言或构建工具，请务必在开始实验前与助教确认。此外，你需要修改 `build.sh` 文件，以确保新的构建方式能够通过 CI 测试（详见 [编译与运行](#_3) 部分）。

!!! danger "除 Flex 和 Bison 等用于词法和语法分析的工具外，**禁止使用任何其他第三方库**。如对某个库是否可用存在疑问，请提前咨询助教。"

!!! tip "建议 Windows 用户使用 [WSL2](https://learn.microsoft.com/en-us/windows/wsl/install) 或者虚拟机完成实验，以和 CI 测试环境尽量保持一致。后续实验提供的实验框架仅适配了 Linux 和 Mac OS 环境。"

??? note "使用与 CI 环境一致的 docker 容器进行实验"
    在 CI 测试中，我们会使用一个 docker 容器来执行测试脚本，编译并测试你的编译器。你可以使用以下命令来拉取这个 docker 镜像：

    ```console
    $ docker pull ghcr.io/zju-cp/compiler-tester:latest
    ```

    再使用以下命令来启动这个 docker 容器：

    ```console
    $ docker run -it ghcr.io/zju-cp/compiler-tester:latest /bin/bash
    ```

    你可以在这个容器中完成实验，以确保你的实验环境与 CI 环境一致。

!!! tip "如果你对 Docker / Git 不熟悉，可以阅读操作系统实验文档中的 [工具讲解](https://zju-os.github.io/doc/manual) 部分。"

## Starter-code 说明

### 代码结构

```text
.
├── .git
├── .gitignore              # 请善用 .gitignore，避免将不必要的文件提交到仓库
├── .gitmodules
├── Makefile                # 我们默认使用 Makefile 作为构建工具
├── build.sh                # 编译构建脚本，**你可以修改这个脚本来使用其他构建工具**
├── config.toml             # 测试脚本配置文件
├── sp26-tests
│   ├── .git
│   ├── .gitignore
│   ├── CONTRIBUTING.md
│   ├── README.md
│   ├── coverage.py         # 测试语法覆盖的脚本，本次实验需要使用
│   ├── ir                  # 中间代码解释器
│   ├── requirements.txt    # 测试脚本依赖
│   ├── test.py             # 测试脚本
│   ├── tests               # 测试用例
│   └── venus.jar           # RISC-V 汇编模拟器，实验4 需要使用
└── src                     # 源代码，**你的主要工作区域**
    ├── main.cpp
    ├── sysy.l
    └── sysy.y
```

在测试的仓库更新时（例如对于测试用例和解释器的修改），你可以通过以下命令将你本地的测试仓库更新到最新版本：

```console
$ git submodule update --recursive --remote --merge
```

在每次开始实验之前，以及 Push 自己的代码到仓库前，最好都先执行这个命令，以确保本地使用的测试仓库是最新的。别忘了多多关注文档的 [News](../news.md) 部分，获取测试仓库的最新更新信息。

### 编译与运行

```console
$ make
$ ./compiler
1 + 2 * 4
<Ctrl+D>
9
```

如果上述内容都与预期一致，那么你已经成功的完成了本次实验的环境配置。

我们为你提供的初始代码是一个支持加法和乘法的计算器，它包含由 Flex 和 Bison 实现的词法分析和语法分析。对于这两个工具的具体介绍和实现见我们的 [Flex, Bison Crash Course](../appendix/lex_yacc.md)。这一代码仅作为一个示例，你可以自由的删除或修改它来完成以后的实验。

在 CI 测试中，我们会执行你仓库中的 `build.sh` 脚本来构建你的编译器，所以请确保你的编译器能够通过这个脚本成功构建。例如，如果你使用 Rust 等其他语言或其他构建工具，你可以修改 `build.sh`，安装必要的依赖并构建你的编译器。但请你**不要**在这个脚本中做出任何会影响接下来 CI 测试的修改，否则将会直接判定为作弊，实验置 0 分。

!!! danger "请不要尝试 hack CI 测试，善待助教，助教会善待你 :-)"

### 调试

在调试之前，建议你首先阅读 [附录 H: VSCode 调试配置指南](../appendix/vscode.md) 来配置你的开发环境。

除了使用 VSCode 进行调试外，你也可以使用 `gdb` 命令行工具：

```console
$ make
$ gdb ./compiler
(gdb) b main
(gdb) run
```

另外也可以利用调试宏进行调试输出：

```cpp
#if DEBUG
#define Debug(format, ...) \
    printf("\33[1;35m[%s,%d,%s] " format "\33[0m\n", \
        __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#else
#define Debug(format, ...);
#endif
```

在编译时加上 `-DDEBUG` 选项即可启用调试输出：

```console
$ make CXXFLAGS="-DDEBUG" 
```

这样，就可以在代码中使用例如 `Debug("test: %d", 1);` 就会显示粗体紫色的调试信息 `[main.cpp,42,calculateResult] test: 1`。这个用法是 ANSI 转义序列，更多用法可以看 [wikipedia](https://zh.wikipedia.org/wiki/ANSI转义序列) 中对 ANSI 转义序列的介绍。

!!! tip "调试建议"
    1. 合理使用断点和单步调试
    2. 使用 watch 窗口监视变量值的变化  
    3. 通过调试宏输出关键信息
    4. 使用版本控制保存调试过的代码

### 执行测试

我们为你提供了一个测试脚本，你可以使用它来测试你的编译器。你可以通过以下命令查看测试脚本的帮助：

```console
$ python sp26-tests/test.py -h
usage: test.py [-h] {lab0,lab1,lab2,lab3,lab4,bonus1,bonus2,bonus3,bonus4} [repo_path]

Test your compiler.

positional arguments:
  {lab0,lab1,lab2,lab3,lab4,bonus1,bonus2,bonus3,bonus4}
                        Which lab to test
  repo_path             Path to your repository. Defaults to the current working directory.

options:
  -h, --help            show this help message and exit
```

除此之外，测试脚本也会从 `repo_path/config.toml` 中读取配置，你可以在这个文件中设置以下选项：

| 选项          | 默认值   | 描述                                                                                           |
|---------------|----------|------------------------------------------------------------------------------------------------|
| `verbose`     | `false`  | 如果设置为 `true`，则测试脚本会输出更多的信息，例如每未通过测试点的报错信息。                                      |
| `use_qemu`    | `false`  | 如果设置为 `true`，则测试脚本在测试 Lab 4、Bonus 1 和 Bonus 2 时会使用 RISC-V 32 工具链进行编译、运行，而不是直接使用 Venus 模拟器来运行。在测试 Lab 0 时，启用该选项也会使用 RISC-V 32 工具链进行编译、运行，这可以帮助你在完成 Bonus 3 时检查你的环境配置是否正确。 |
| `use_accipit` | `false`   | 如果设置为 `true`，则在 Lab 3 中，使用 Accipit IR 作为中间代码，而不是 Zero IR |
| `parallel`    | `true`   | 如果设置为 `false`，则测试脚本会禁用并行测试。 |
| `check_ssa`   | `false`  | 如果设置为 `true`，则测试脚本会在使用 Zero IR 测试 Lab 3 时会检查你的中间代码是否符合 SSA 形式。 |
| `precise_timing` | `true` | 如果为 `true`，则在执行 ELF 文件时，使用 timer.c 来测量 main 函数的执行时间，否则将会测量整个程序的执行时间（包括 qemu 的启动时间等） |
| `timeout` | `10.0` | 设置测试中执行编译、单个测试用例运行的超时时间 |
| `extra_cflags`| `["sp26-tests/libs/io.c"]`     | 在使用 riscv32-unknown-elf-gcc 编译器编译时，该选项会被传递给编译器。|

测试脚本还可以通过命令行指定测试文件、修改测试配置。更多介绍请参见[附录：测试脚本说明](../appendix/tester.md)。

!!! tip "如何修改配置"
    假如你想关闭并行测试，并启用 QEMU，你可以修改 `config.toml` 文件：

    ```toml
    parallel = false
    use_qemu = true
    ```
    
    然后像之前一样运行测试脚本即可：

    ```console
    $ python sp26-tests/test.py lab0 .
    ```

例如，如果你想测试 Lab 0，你可以在仓库目录下使用以下命令：

```console
$ python sp26-tests/test.py lab0 .
```

在 CI 测试中，我们会使用这个脚本来测试你的编译器，并检查你的报告是否存在。CI 脚本会获取当前提交的分支名，作为测试脚本的参数。所以，请你在做实验 X 时，请你将代码提交到远程的 `labX` 分支，例如，为了开始 Lab 0，你现在最好先切换到 `lab0` 分支：

```console
$ git checkout -b lab0                  # 创建一个新的 lab0 分支
$ git push -u origin lab0               # 将 lab0 分支推送到远程
```

在完成 Lab 0 后，请你及时提交代码：

```console
$ git add .
$ git commit -m "lab0: done"
$ git push origin lab0
```

在 push 到仓库的实验对应分支后，CI 测试会自动运行，CI 测试程序显示的 `Test score` 即为对应实验的测试得分。你可以在 ZJU Git 上的查看 CI 测试的结果：在 ZJU Git 上打开你的仓库，选择对应的分支，在右侧面板的 commit id 旁边有一个 ✅、❌ 或 ⭕️ 的图标，分别表示测试通过、测试失败或者测试中。点击这个图标，你可以查看 CI 测试的详细结果。

选择分支后，也可以点击右侧面板右上角的 **History** 按钮，查看该分支的所有提交记录，同时也会在每条 push 前的最后一次提交记录旁有一个与上述一样的图标，表示该次提交的 CI 测试结果，也可以点进去查看详细结果。

你也可以点击左侧菜单中的 **Build** > **Jobs**，查看 CI 测试的详细结果。

!!! danger "注意事项"
    1. **请勿在 `build.sh` 中做出任何会影响 CI 测试的修改**，否则将会直接判定为作弊，对应实验置 0 分。
    2. **实验期间我们会接受同学们对测试样例的贡献并不定期更新**，请你在实验过程中保持关注测试样例的更新：
        - 在实验 X 开始日期后，其测试样例不再更新。建议在截止日期前一周，重新运行 CI 测试以确保通过最新的样例。  
        - 每次 CI 测试都会拉取最新的测试仓库。如本地测试与 CI 测试不一致，请及时更新本地测试仓库。
    3. **我们以实验对应分支的最后一次推送（Push）为准来计分**，因此：
        - 实验开始前，建议先使用 `git checkout -b labX` 创建新分支并使用 `git push -u origin labX` 推送至远程。
        - 实验完成后，请及时推送代码到 ZJU Git 并切换到下一个实验的分支。
        - 如果在截止日期之后才推送代码，我们将以最后一次**推送**的时间来计算迟交惩罚。
    4. **实验截止后将关闭推送到对应分支的权限**。如果未在截止日期前推送到远程，即使本地提交了代码，也无法再推送到远程来提交。因此请务必在 `git commit` 后及时使用 `git push`。

!!! tip "订阅 ZJU Git 仓库更新提醒"

    为了避免在进行后续实验时，测试用例或代码框架更新后与本地代码不匹配的情况，建议按照以下步骤订阅 [sp26-tests](https://git.zju.edu.cn/compiler/sp26-tests) 以及 [sp26-starter](https://git.zju.edu.cn/compiler/sp26-starter) 仓库的动态通知：

    1. 登录 ZJU Git 并打开你需要关注的仓库。
    2. 在仓库页面右上角点击 **小闹钟图标**，选择 **Watch** 来订阅所有仓库通知。这样，当仓库有新动态（如同学提交的 Merge Request、框架 Bug 修复的 commit 等），你将收到邮件通知。
    3. 默认情况下，ZJU Git 会将通知发送到你的 ZJU 邮箱。如果你不常用 ZJU 邮箱，可以按以下步骤修改为常用邮箱：
        - 点击页面左上角的**头像** > **Edit profile**。
        - 在 **Email** 字段中填写你的常用邮箱，并点击 **Update profile settings**。
        - 输入密码确认后，系统会向新邮箱发送确认邮件，点击邮件中的链接即可完成修改。


<a id="bug-checklist"></a>
!!! abstract "Checklist: 测试 Bug 排查"
    - 本地能通过，但是远程 CI 测试失败：
        1. 同步测试代码：执行 `git submodule update --recursive --remote --merge` 确保本地与远程测试一致。
        2. 检查编译问题：查看 CI 测试的日志，检查是否是因为远程 CI 的 gcc 版本差异导致编译失败。
        3. 检查报错信息：修改 `config.toml`，添加 `verbose = true` 并推送，查看 CI 测试点错误详情。
        4. 排查未定义行为：`Makefile` 中 `CXXFLAGS` 添加 `-fsanitize=undefined`，本地重新编译并使用 `./compiler` 复现报错测试点，检查输出中是否有运行时未定义行为警告。
        5. 排查内存问题：`Makefile` 中 `CXXFLAGS` 添加 `-fsanitize=address`，本地重新编译并使用 `./compiler` 复现报错测试点，检查输出中是否有内存错误警告。
        6. 使用相同镜像：`docker run --platform linux/amd64 -it ghcr.io/zju-cp/compiler-tester:latest /bin/bash` 后在此容器中编译运行，复现 CI 问题。
    - 本地或远程出现超时：
        1. 本地死循环检查：使用 `./compiler` 运行超时的测试用例，检查是否有问题。
        2. 调整超时限制：在 `config.toml` 中修改 `timeout` 值，增加时间限制。
    - 更改 `config.toml` 后无效：
        1. 检查注释：`toml` 文件中的 `#` 表示注释，确保相关配置没有被注释。

## SysY 语言介绍

在本实验中你要实现的是简化的 SysY 语言，关于它的由来和具体语法规范请参考[附录](../appendix/sysy.md)，可以认为是 C 语言的子集。在这里我们给出一个简单的递归函数例子让同学们直观的了解一下 SysY 语言。

!!! example "一个简单的SysY程序"

    ```c linenums="1"
    int factorial(int n) {
        if (n == 0)
            return 1;
        return n * factorial(n - 1);
    }
    int main() {
        int n = read();
        int result = factorial(n);
        write(result);
        return 0;
    }
    ```

## 你的任务

为了确保接下来的实验能顺利进行，我们要求你仔细阅读 SysY 文法，对文法中每条规则所定义的语法成分进行分析，了解其作用、限定条件、组合情况和可能产生的文法。

为了让你熟悉测试脚本 `sp26-tests/test.py` 是如何运作的，我们需要你自行设计 5 个测试程序。在测试脚本 `Test` 类的 `parse_file` 方法中，你可以了解测试脚本是如何获取程序的输入和期望输出的。你也可以查看 `sp26-tests/tests` 下四个实验对应的测试样例文件夹中的测试样例，看看测试程序应该如何编写。

??? example "测试程序示例"
    通过阅读代码，不难发现测试程序分为以下三种：
    
    === "能够成功通过编译"
        
        这类测试程序开头没有 `//` 注释，或者有一行 `//` 注释但注释中没有 `Error` 字样，例如：

        ```c title="sp26-tests/tests/lab0/arr_defn1.sy"
        int a[10][10];
        int main(){
            return 0;
        }
        ```

    === "不能通过编译"
    
        这类测试程序开头有一行 `//` 注释，以 `Return` 开头，后接期望返回的错误代码。例如：

        ```c title="sp26-tests/tests/lab0/assign_rvalue.sy"
        // Return 1 - Syntax Error: rvalue cannot be assigned to

        int main() {
            int a = 5;
            3 = a;
            return 0;
        }
        ```
    
    === "能成功编译，给定输入需要有期望的输出"
    
        这类测试程序开头有两行 `//` 注释，第一行注释为 `// Input:` 开头，之后为空格隔开的输入，第二行注释为 `// Output:` 开头，之后为空格隔开的输出。当没有输出时为 `None`。例如：

        ```c title="sp26-tests/tests/lab4/add.sy"
        // Input: 10 -1
        // Output: 9

        int main(){
            int a, b;
            a = read();
            b = read();
            write(a+b);
            return 0;
        }
        ```

本次实验中，你需要编写 5 个测试程序，我们要求测试程序共计能覆盖**所有语法规则**，并且属于**能成功编译，给定输入需要有期望的输出**的类型。你可以参考我们提供的测试用例，但是**不要**直接复制粘贴，否则你将会得到 0 分。

并且，在你编写的 5 个测试程序中，需要至少有一个测试用例的有效行数不少于 50 行。推荐你参考 [LeetCode](https://leetcode.com)、[洛谷](https://www.luogu.com.cn)等在线评测网站上的模拟题的参考答案，来设计一个较为复杂的测试程序，例如测试仓库中已有的数独（`sudoku.sy`）等。对于这类较为复杂的测试程序，请你在程序开头的注释中简要说明程序的功能和来源，以便我们更好的理解你的测试程序。

!!! tip "强烈推荐你将复杂的测试用例提交到 [Bonus 0](../bonus/contribute.md)，作为 Lab 3 & 4 的测试用例。"

请你将编写好的 5 个测试程序放在仓库的 `appends/lab0` 文件夹下，并以 `.sy` 为后缀，以便我们的测试脚本能够找到你的测试程序。

我们为你提供了一个 [coverage.py](https://git.zju.edu.cn/compiler/coverage)（已经包含在仓库中的 `sp26-tests` 文件夹下），它可以帮助你检查你的测试用例是否覆盖了所有语法规则。你可以通过以下命令运行它：

```console
$ python3 sp26-tests/coverage.py -h
usage: coverage.py [-h] [-v] files [files ...]

positional arguments:
  files          Files to check

options:
  -h, --help     show this help message and exit
  -v, --verbose  Print verbose output
```

使用 `-v` 选项可以打印出每个文件解析出的语法树和在详细的未覆盖到的语法规则。

如果你的测试用例覆盖了所有语法规则，那么你将会看到如下输出：

```console
$ python3 sp26-tests/coverage.py appends/lab0/testfile1.sy
Parse Success
All rules are covered!
```

而如果你的测试用例没有覆盖到所有语法规则，那么你将会看到如下输出：

```console
$ python3 sp26-tests/coverage.py appends/lab0/testfile1.sy
Parse Success
Some rules are not covered:
UnaryExp        ::= IDENT "(" [FuncRParams] ")"
```

!!! note "请注意，对于语法规则中的可选项，你需要覆盖两种可能的情况， 例如 If 语句中的 else 分支。对于语法规则中的重复项，理论上你需要覆盖 0、1、多次的情况，但我们的检查并不会如此严格 :-)"

在本次实验的测试脚本中，我们会使用上述提到的 `coverage.py` 来检查你的测试用例是否覆盖了所有的语法规则，并且尝试使用你的测试程序，使用 gcc 编译、测试并输出测试结果。你可以借此机会熟悉我们的测试脚本是如何运作的。使用以下命令执行本次试验的自动化测试：

```console
$ python3 sp26-tests/test.py lab0 .
```

## 实验提交

你需要将你自己设计的测试用例放在仓库中的 `appends/lab0` 文件夹下，以 `.sy` 后缀名结尾，并提交至 `lab0` 分支，以便我们的 CI 测试程序使用你的测试用例来测试。本次实验你只需要熟悉环境和测试脚本，所以不需要提交实验报告。