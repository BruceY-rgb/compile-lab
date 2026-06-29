# App. B: 测试框架说明

为了帮助同学们更好地完成实验，我们提供了一个测试框架 [sp26-tests](https://git.zju.edu.cn/compiler/sp26-tests)，用于自动化测试你的实验代码。框架中提供了评测所用到的测试用例以及测试所需要的脚本等等。为了辅助同学们更好地使用测试框架完成实验，测试脚本看起来十分臃肿，我们在此对测试脚本的使用方法进行简要说明。

## 框架结构

测试框架的目录结构如下：

```
sp26-tests
├── CONTRIBUTING.md     # 贡献指南
├── coverage.py         # 覆盖率测试脚本，Lab 0 中使用
├── ir
│   ├── accipit.py      # Accipit IR Interpreter
│   └── zero.py         # Zero IR Interpreter
├── libs                # 测试框架在用 riscv32-unknown-elf-gcc 编译时会用到的源文件
│   ├── io.c
│   └── timer.c
├── README.md
├── requirements.txt    # 测试框架所需的 Python 依赖
├── test.py             # 测试脚本
├── tests               # 测试用例
│   ├── bonus3
│   ├── lab1
│   ├── lab2
│   ├── lab3
│   └── lab4
└── venus.jar           # Venus 测试工具
```

## 使用方法

在大家的仓库中，已经将测试框架作为 `submodule` 添加到了仓库中，因此你可以直接使用测试框架进行测试。在你的仓库目录中，使用以下命令可以查看测试脚本的使用方法：

```console
$ python3 sp26-tests/test.py -h
```

最基础的使用方式为：

```console
$ python3 sp26-tests/test.py <lab_name> <path_to_your_repo>
```

其中 `<lab_name>` 为实验名，如 `lab1`、`lab2` 等，`<path_to_your_repo>` 为你的仓库路径，若省略则默认为当前目录。使用该命令即可对你的实验代码进行对应实验的测试。除了这两个参数以外，测试脚本还提供了一些其他参数：

| 参数 | 默认值 | 说明 | 示例 |
| --- | --- | --- | --- |
| `-h`, `--help` |  | 显示帮助信息 |
| `-f`, `--file` |  | 指定测试文件或目录来执行测试，设置该参数后将不会输出测试分数 | `-f test_file.sy test_dir` 指定测试文件 `test_file.sy` 和目录 `test_dir` 中的所有 `.sy` 文件进行测试 |
| `-o`, `--output` |  | 指定测试中编译产物的输出目录。如果不指定，则会存放在一个临时文件夹，并在测试结束后删除 | `-o output_dir` 指定测试结果输出到 `output_dir` 目录 |
| `-v`, `--verbose` | `False` | 显示详细的测试信息 | 使用 `-v` `--verbose` 或 `--verbose=True` 即可开启详细信息显示，使用 `--verbose=False` 即可关闭 |
| `--use-qemu`, `--qemu` | `False` | 在 Lab 4、Bonus 1、Bonus 2 中使用 `qemu-riscv32` 运行测试，而不是使用 Venus 来模拟执行汇编。在 Lab 0 中会使用 `riscv32-unknown-elf-gcc` 编译，并用 `qemu-riscv32` 执行，而不是使用本地环境进行编译、执行 | 使用 `--use-qemu` 或 `--qemu` 即可开启使用 `qemu-riscv32` 运行测试；使用 `--use-qemu=False` 或 `--qemu=False` 即可关闭 |
| `--use-accipit`, `--accipit` | `False` | 在 Lab 3 中，使用 Accipit IR 作为中间代码，而不是 Zero IR | 使用 `--use-accipit` 或 `--accipit` 即可开启使用 Accipit IR 运行测试；使用 `--use-accipit=False` 或 `--accipit=False` 即可关闭 |
| `--parallel` | `False` | 并行运行测试用例 | 使用 `--parallel` 即可开启并行运行测试用例；使用 `--parallel=False` 即可关闭 |
| `--check-ssa` | `False` | 在 Lab 3 中，如果你使用 Zero IR，将会在执行 IR 时检查是否符合 SSA 形式，即给 `zero.py` 传递 `--ssa` 参数 | 使用 `--check-ssa` 即可开启检查 SSA 形式；使用 `--check-ssa=False` 即可关闭 |
| `--precise-timing` | `True` | 在执行 ELF 文件时，使用 `timer.c` 来测量 `main` 函数的执行时间，如果关闭，将会测量整个程序的执行时间（包括 qemu 的启动时间等） | 使用 `--precise-timing` 即可开启精确计时；使用 `--precise-timing=False` 即可关闭 | 
| `--timeout` | `10.0` | 设置测试中执行编译、单个测试用例运行的超时时间 | `--timeout=5.0` 设置超时时间为 5 秒 |
| `--extra-cflags` | `"sp26-tests/libs/io.c"` | riscv32-unknown-elf-gcc 编译时所添加的额外参数 | `--extra-cflags="-O2 -Wall"` 添加 `-O2` 和 `-Wall` 参数 |

除了使用命令行参数外，你还可以修改自己仓库中的 `config.toml` 文件来配置测试脚本的默认参数。在 `config.toml` 文件中，你可以配置的参数如下：

```toml
verbose = false                             # bool
use_qemu = false                            # bool
use_accipit = false                         # bool
parallel = false                            # bool
check_ssa = false                           # bool
precise_timing = true                       # bool
timeout = 10.0                              # float
extra_cflags = ["sp26-tests/libs/io.c"]     # list[str]
```

测试时，会先读取 `config.toml` 文件中的配置，然后再读取命令行参数，命令行参数会覆盖 `config.toml` 文件中的配置，这可以便于你在本地测试时快速修改参数。

## 测试流程

在这部分，我们将简要介绍测试脚本对于不同实验的测试流程。

### 解析测试用例

所有的实验，都涉及到对测试用例进行解析，获取测试用例的输入、输出等信息。每个实验会读取的测试用例来源如下：

| 实验 | 测试用例来源 |
| --- | --- |
| Lab 0 | `<repo_path>/appends/lab0` |
| Lab 1 | `sp26-tests/tests/lab1` |
| Lab 2 | `sp26-tests/tests/lab2` |
| Lab 3 | `sp26-tests/tests/lab3` |
| Lab 4 | `sp26-tests/tests/lab4` |
| Bonus 1 | `sp26-tests/tests/lab4` |
| Bonus 2 | `sp26-tests/tests/lab4`, `<repo_path>/appends/bonus2` |
| Bonus 3 | `sp26-tests/tests/lab4`, `sp26-tests/tests/bonus3`, `<repo_path>/appends/bonus3` |

!!! note "如果你使用 `-f` 参数指定了测试用例，将不会使用以上路径中的测试用例，而是使用你指定的测试用例。此时，测试脚本不会输出测试分数。"

解析测试用例时，会读取其中的开头几行 `//` 注释，获取测试用例的输入、输出等信息，有以下几种情况：

1. 无注释格式，即开头没有 `//` 注释，表示期望程序能够正常编译运行：
   ```c
   int main() { return 0; }
   ```
2. 单行注释格式，且注释以 `Return` 开头，表示期望程序编译运行后以特定的返回值退出：
   ```c
   // Return 10 Undeclared - Semantic Error: 'a' is not defined
    int main() { return a; }
   ```
3. 双行注释格式，其中第一行以 `Input:` 开头，指定测试输入，第二行以 `Output:` 开头，指定测试输出，`None` 表示无输入/输出：
   ```c
    // Input: 1 2
    // Output: 3
    int main() {
        write(read() + read());
        return 0;
    }
   ```

对于 Lab 1 和 Lab 2，仅包含前两种情况。对于 Lab 3、Lab 4 和 Bonus，仅包含第三种情况。

??? example "解析测试用例的测试代码相关部分"
    ```python
    @dataclass
    class Test:
        filename: str
        inputs: list[str] | None
        expected: list[str] | None
        should_return: int | None

        @staticmethod
        def parse_file(filename: str) -> "Test":
            content = open(filename).readlines()
            comments: list[str] = []
            for line in content:
                # get comment, start with //
                if line.strip().startswith("//"):
                    comments.append(line.strip()[2:].strip())
                else:
                    break
            if len(comments) >= 2 and comments[0].startswith("Input:") and comments[1].startswith("Output:"):   # input and output
                input = comments[0].replace("Input:", "").split()
                if input[0] == "None":
                    input = []
                expected = comments[1].replace("Output:", "").split()
                if expected[0] == "None":
                    expected = []
                return Test(filename, input, expected, None)
            elif len(comments) >= 1 and comments[0].startswith("Return"):
                # Return 1 - Syntax Error: rvalue cannot be assigned to
                match = re.search(r"Return (\d+)", comments[0])
                return Test(filename, None, None, int(match.group(1)) if match else None)
            else:   # should success
                return Test(filename, None, None, 0)

        def __str__(self):
            return f"Test({self.filename}, {self.inputs}, {self.expected}, {self.should_return})"
    ```

### 测试 Lab 0

对于 Lab 0 的测试，测试单个测试用例的编译逻辑大致如下：

```py
compiler = riscv32-unknown-elf-gcc if use_qemu else (gcc or clang)
extra_cflags += ["-DRV32ASM"] if use_qemu else []   # used in timer.c
if precise_timing:
    run([
        compiler,
        src_file,
        timer_c_file,
        "-o", elf_file,
        "-Wno-implicit-function-declaration",
        *extra_cflags
    ])
else:
    run([
        compiler,
        src_file,
        "-o", elf_file,
        *extra_cflags
    ])
```

简单来说，如果设置了 `use_qemu = true`，则会用 `riscv32-unknown-elf-gcc` 编译，否则会用 `gcc` 或 `clang` 编译。如果设置了 `precise_timing = true`，则会使用 `timer.c` 来测量 `main` 函数的执行时间。

??? note "`timer.c` 是如何工作的"

    `timer.c` 内容如下：

    ```c linenums="1" title="timer.c"
    #include <stdio.h>
    #include <stdint.h>

    #ifdef RV32ASM
    struct timespec {
        __time_t tv_sec;
        long tv_nsec;
    };
    #else   // no RV32ASM
    #include <time.h>
    #endif  // RV32ASM

    static inline struct timespec get_timespec() {
        struct timespec ts;
    #ifdef RV32ASM
        asm volatile (
            "li a7, 403\n"
            "li a0, 1\n"
            "mv a1, %0\n"
            "ecall\n"
            :
            : "r"(&ts)
            : "a0", "a1", "a7"
        );
    #else   // no RV32ASM
        clock_gettime(CLOCK_MONOTONIC, &ts);
    #endif  // RV32ASM
        return ts;
    }

    static struct timespec start_ts, end_ts;

    __attribute__((constructor)) void start_timer() {
        start_ts = get_timespec();
    }

    __attribute__((destructor)) void end_timer() {
        end_ts = get_timespec();
        
        long diff_sec, diff_nsec;
        if ((end_ts.tv_nsec - start_ts.tv_nsec) < 0) {
            diff_sec = end_ts.tv_sec - start_ts.tv_sec - 1;
            diff_nsec = end_ts.tv_nsec - start_ts.tv_nsec + 1000000000;
        } else {
            diff_sec = end_ts.tv_sec - start_ts.tv_sec;
            diff_nsec = end_ts.tv_nsec - start_ts.tv_nsec;
        }
        printf("Execution time: %ld s %ld ns\n", diff_sec, diff_nsec);
    }
    ```

    其中，最关键的就是 `start_timer` 和 `end_timer` 函数。这两个函数的修饰符 `__attribute__((constructor))` 和 `__attribute__((destructor))` 是 GCC 和 Clang 提供的属性，用于在共享库或可执行文件加载和卸载时自动执行特定的函数。这些属性允许我们在程序的 `main` 函数之前和程序退出时执行初始化和清理操作。

    如果有多个 `constructor` 或 `destructor` 函数，它们的执行顺序由编译器决定，通常以它们的定义顺序执行。当然，你也可以通过 `priority` 参数指定执行顺序：

    ```c linenums="1"
    __attribute__((constructor(101))) void high_priority_init() {
        printf("High-priority constructor.\n");
    }

    __attribute__((constructor(201))) void low_priority_init() {
        printf("Low-priority constructor.\n");
    }
    ```

    在 `timer.c` 中，`start_timer` 作为 `constructor` 函数，会在程序开始时执行，获取当前时间；`end_timer` 作为 `destructor` 函数，会在程序结束时执行，获取当前时间、计算时间差并输出。

编译完成后，如果设置了 `use_qemu = true`，则会使用 `qemu-riscv32` 来运行测试用例，否则会使用本地环境来运行，也就是直接运行编译生成的 ELF 文件。执行后，我们会获取程序的输出，与期望输出进行比较，判断是否正确。

测试完所有测试用例后，会使用 `coverage.py` 来统计测试覆盖率，并判断测试用例是否不少于 5 个。如果没有完全覆盖所有测试用例，将会测试失败。如果你使用 `-f` 参数指定了测试用例，将不会进行这部分测试。

??? example "Lab 0 测试代码相关部分"
    设置 `compiler`：

    ```py linenums="1"
    compiler = envs.cc if not cfg.use_qemu else envs.rv32_gcc
    assert compiler is not None, "gcc or clang not found." if not cfg.use_qemu else "riscv32-unknown-elf-gcc not found."
    ```

    编译运行单个测试用例：
    
    ```py linenums="1"
    @test_exception_handling
    def run_with_src(compiler: str, test: Test, qemu: bool = False) -> TestResult:  # lab0
        assert test.expected is not None, f"{test.filename} has no expected output."
        src_file_path = os.path.join(envs.output_dir, Path(test.filename).with_suffix(".c").name)
        shutil.copy(test.filename, src_file_path)
        
        return compile_run_result(compiler, test, src_file_path, qemu)

    @test_exception_handling
    def compile_run_result(compiler: str, test: Test, src_file_path: str, use_qemu: bool) -> TestResult:
        executable_file_path = os.path.join(
            envs.output_dir,
            Path(test.filename).with_suffix(
                ".exe" if sys.platform.startswith("win") else ""
            ).name
        )
        if cfg.precise_timing:
            run_commands([
                [compiler, src_file_path, envs.timer_c_path, "-o", executable_file_path, "-Wno-implicit-function-declaration"] + \
                    (["-DRV32ASM"] if use_qemu else []) + cfg.extra_cflags,
            ])
        else:
            run_commands([
                [compiler, src_file_path, "-o", executable_file_path] + cfg.extra_cflags,
            ])

        if use_qemu:
            assert envs.rv32_qemu is not None, "qemu-riscv32 not found."
            cmd = [envs.rv32_qemu, executable_file_path]
        else:
            cmd = [executable_file_path]
    
        results, run_times = execute_with_timing(
            subprocess.run,
            cmd,
            input="\n".join(test.inputs) if test.inputs is not None else None,
            capture_output=True,
            text=True,
            timeout=cfg.timeout,
            check=True,
        )
        sample_result = results[-1]
        sample_output = sample_result.stdout.strip().split("\n")
        # case 1: Execution time: %llu cycles
        match = re.search(r"Execution time: (\d+) cycles", sample_output[-1])
        if match:
            cycles: list[int] = []
            for result in results:
                output = result.stdout.strip().split("\n")
                time_line = output[-1]
                match = re.search(r"Execution time: (\d+) cycles", time_line)
                assert match, "Execution time not found."
                cycles.append(int(match.group(1)))
            return TestResult(test, sample_output[:-1], run_time=sum(cycles) / len(cycles), run_time_type=TimeType.CYCLES)
        # case 2: Execution time: %ld s %ld ns
        match = re.search(r"Execution time: (\d+) s (\d+) ns", sample_output[-1])
        if match:
            times: list[int] = []
            for result in results:
                output = result.stdout.strip().split("\n")
                time_line = output[-1]
                match = re.search(r"Execution time: (\d+) s (\d+) ns", time_line)
                assert match, "Execution time not found."
                times.append(int(int(match.group(1)) * 1e9 + int(match.group(2))))
            return TestResult(test, sample_output[:-1], run_time=sum(times) / len(times), run_time_type=TimeType.NANOSECONDS)
        # case 3: no match, use run_times avg
        return TestResult(test, sample_output, run_time=int(sum(run_times) / len(run_times) * 1e9), run_time_type=TimeType.NANOSECONDS)
    ```

    最后进行覆盖率测试，并判断是否不少于 5 个测试用例：

    ```py linenums="1"
    try:
        result = subprocess.run([envs.python, envs.coverage_py, *map(lambda x: x.filename, tests)], check=True)
        print(green(f"lab0 coverage test passed."))
    except:
        assert False, f"lab0 coverage test failed."
    global test_score
    test_score = min(100, 100 * len(tests) / 5)
    assert len(tests) >= 5, "lab0 test cases are less than 5."
    ```

### 测试 Lab 1 & 2

对于 Lab 1 和 Lab 2 的测试，我们只会使用如下命令执行你的编译器，判断能否按照期望正常编译或编译失败：

```console
$ ./compiler <input_file>
```

在测试完所有测试用例后，会检查你是否按照要求提交了报告 `reports/lab1.pdf` 或 `reports/lab2.pdf`。

??? example "Lab 1 & 2 测试代码相关部分"
    
    测试单个测试用例：

    ```py linenums="1"
    @test_exception_handling
    def run_only_compiler(compiler: str, test: Test) -> TestResult:  # lab1, lab2
        assert test.inputs is None, "Not implemented input for lab1 or lab2"
        subprocess.run([compiler, test.filename], capture_output=True, timeout=cfg.timeout, check=True)
        return TestResult(test, result_type=ResultType.ACCEPTED)
    ```

    检查是否提交了报告：

    ```py linenums="1"
    if not (Path(repo_path) / "reports" / f"{lab}.pdf").exists():
        print(red(f"Error: reports/{lab}.pdf not found."))
        failed = True
        report_score = 0
    else:
        report_score = 100
    ```

### 测试 Lab 3

对于 Lab 3 的测试，我们会执行以下命令来运行你的编译器：

```console
$ ./compiler <input_file> <output_file>
```

接下来，如果你设置 `use_accipit = true`，则会使用以下命令来运行你生成的 Accipit IR：

```console
$ python3 sp26-tests/ir/accipit.py <output_file>
```

否则，会使用以下命令来运行你生成的 Zero IR：

```console
$ python3 sp26-tests/ir/zero.py <output_file>
```

如果你设置了 `check_ssa = true`，则会检查 Zero IR 是否符合 SSA 形式：

```console
$ python3 sp26-tests/ir/zero.py <output_file> --ssa
```

我们会获取解释器的输出，与期望输出进行比较，判断是否正确。在测试完所有测试用例后，会检查你是否按照要求提交了报告 `reports/lab3.pdf`。

??? example "Lab 3 测试代码相关部分"

    测试单个测试用例：

    ```py linenums="1"
    @test_exception_handling
    def run_with_ir(compiler: str, test: Test, accipit: bool) -> TestResult:  # lab3
        ir_file_path = os.path.join(
            envs.output_dir,
            Path(test.filename).with_suffix(
                ".acc" if accipit else ".zir"
            ).name
        )
        ir_path = envs.accipit_ir_path if accipit else envs.zero_ir_path
        assert os.path.exists(ir_path), f"{ir_path} not found."
        assert test.expected is not None, f"{test.filename} has no expected output."
        result = subprocess.run(
            [compiler, test.filename, ir_file_path, '--ir'],  # add --ir flag
            capture_output=True,
            check=True,
            timeout=cfg.timeout)
        
        result = subprocess.run(
            [envs.python, ir_path, ir_file_path] + (["--ssa"] if cfg.check_ssa and not accipit else []),
            input="\n".join(test.inputs) if test.inputs is not None else None,
            capture_output=True,
            text=True,
            timeout=cfg.timeout,
            check=True
        )
        output = result.stdout.split("\n")
        output = [line.strip() for line in output if line.strip() != ""]
        if output:
            # extract run step in the last line "Exit with code {exit_code} within {run_step} steps."
            match = re.search(r"within (\d+) steps", output[-1])
            if match:
                run_step = int(match.group(1))
            else:
                run_step = None
            output = output[:-1]
        else:
            run_step = None
        return TestResult(test, output, run_time=run_step, run_time_type=TimeType.STEPS)
    ```

    检查是否提交了报告：

    ```py linenums="1"
    if not (Path(repo_path) / "reports" / f"{lab}.pdf").exists():
        print(red(f"Error: reports/{lab}.pdf not found."))
        failed = True
        report_score = 0
    else:
        report_score = 100
    ```

### 测试 Lab 4 & Bonus

对于 Lab 4 和 Bonus 的测试，我们会执行以下命令来运行你的编译器：

```console
$ ./compiler <input_file> <output_file>
```

如果你设置了 `use_qemu = true`，则会执行以下命令来运行你的编译器：

```console
$ ./compiler <input_file> <output_file> --qemu
```

接下来，如果你设置了 `use_qemu = true`，则会像 [测试 Lab 0](#lab-0) 那样编译并运行你的程序，只不过其中的 `src_file` 为你编译出的 `.S` 文件。对于 Bonus 3，我们会强制令 `use_qemu = true`。否则，将会使用 Venus 来模拟执行汇编：

```console
$ java -jar sp26-tests/venus.jar <output_file> -ahs -ms -1      # set max steps to inf
$ java -jar sp26-tests/venus.jar <output_file> -ahs -ms -1 -n   # get run steps
```

我们会获取 QEMU 或 Venus 的输出，与期望输出进行比较，判断是否正确。最后，我们会检查你是否按照要求提交了报告 `reports/lab4.pdf`、`reports/bonus1.pdf`、`reports/bonus2.pdf` 或 `reports/bonus3.pdf`。

??? example "Lab 4 & Bonus 测试代码相关部分"

    测试单个测试用例：

    ```py linenums="1"
    @test_exception_handling
    def run_with_asm(compiler: str, test: Test, qemu: bool = False) -> TestResult:  # lab4
        assert test.expected is not None, f"{test.filename} has no expected output."

        # compile to assembly
        assembly_file_path = os.path.join(envs.output_dir, Path(test.filename).with_suffix(".S").name)
        subprocess.run(
            [compiler, test.filename, assembly_file_path] + (['--qemu'] if qemu else []),  # add --qemu flag
            capture_output=True,
            timeout=cfg.timeout,
            check=True
        )
        
        if qemu:
            assert envs.rv32_gcc is not None, "riscv32-unknown-elf-gcc not found."
            assert envs.rv32_qemu is not None, "qemu-riscv32 not found."

            return compile_run_result(envs.rv32_gcc, test, assembly_file_path, use_qemu=True)    
        else:   # run with venus
            assert envs.java is not None, "java not found."
            assert os.path.exists(envs.venus_jar), f"{envs.venus_jar} not found."

            result = subprocess.run(
                [envs.java, "-jar", envs.venus_jar, assembly_file_path, '-ahs', '-ms', '-1'],         # -ms -1: ignore max step limit
                input="\n".join(test.inputs) if test.inputs is not None else None,
                capture_output=True,
                text=True,
                timeout=cfg.timeout,
                check=True
            )
            output = result.stdout.strip().split("\n")[:-1]  # remove the last line "Exit with code {exit_code} within {run_step} steps."
            output = [line.strip() for line in output if line.strip() != ""]
            result_step = subprocess.run(
                [envs.java, "-jar", envs.venus_jar, assembly_file_path, '-ahs', '-n', '-ms', '-1'],   # -n: only output step count
                input="\n".join(test.inputs) if test.inputs is not None else None,
                capture_output=True,
                text=True,
                timeout=cfg.timeout,
                check=True
            )
            run_step = int(result_step.stdout.strip())
            return TestResult(test, output, run_time=run_step, run_time_type=TimeType.STEPS)

    @test_exception_handling
    def compile_run_result(compiler: str, test: Test, src_file_path: str, use_qemu: bool) -> TestResult:
        executable_file_path = os.path.join(
            envs.output_dir,
            Path(test.filename).with_suffix(
                ".exe" if sys.platform.startswith("win") else ""
            ).name
        )
        if cfg.precise_timing:
            run_commands([
                [compiler, src_file_path, envs.timer_c_path, "-o", executable_file_path, "-Wno-implicit-function-declaration"] + \
                    (["-DRV32ASM"] if use_qemu else []) + cfg.extra_cflags,
            ])
        else:
            run_commands([
                [compiler, src_file_path, "-o", executable_file_path] + cfg.extra_cflags,
            ])

        if use_qemu:
            assert envs.rv32_qemu is not None, "qemu-riscv32 not found."
            cmd = [envs.rv32_qemu, executable_file_path]
        else:
            cmd = [executable_file_path]
    
        results, run_times = execute_with_timing(
            subprocess.run,
            cmd,
            input="\n".join(test.inputs) if test.inputs is not None else None,
            capture_output=True,
            text=True,
            timeout=cfg.timeout,
            check=True,
        )
        sample_result = results[-1]
        sample_output = sample_result.stdout.strip().split("\n")
        # case 1: Execution time: %llu cycles
        match = re.search(r"Execution time: (\d+) cycles", sample_output[-1])
        if match:
            cycles: list[int] = []
            for result in results:
                output = result.stdout.strip().split("\n")
                time_line = output[-1]
                match = re.search(r"Execution time: (\d+) cycles", time_line)
                assert match, "Execution time not found."
                cycles.append(int(match.group(1)))
            return TestResult(test, sample_output[:-1], run_time=sum(cycles) / len(cycles), run_time_type=TimeType.CYCLES)
        # case 2: Execution time: %ld s %ld ns
        match = re.search(r"Execution time: (\d+) s (\d+) ns", sample_output[-1])
        if match:
            times: list[int] = []
            for result in results:
                output = result.stdout.strip().split("\n")
                time_line = output[-1]
                match = re.search(r"Execution time: (\d+) s (\d+) ns", time_line)
                assert match, "Execution time not found."
                times.append(int(int(match.group(1)) * 1e9 + int(match.group(2))))
            return TestResult(test, sample_output[:-1], run_time=sum(times) / len(times), run_time_type=TimeType.NANOSECONDS)
        # case 3: no match, use run_times avg
        return TestResult(test, sample_output, run_time=int(sum(run_times) / len(run_times) * 1e9), run_time_type=TimeType.NANOSECONDS)
    ```

    检查是否提交了报告：

    ```py linenums="1"
    if not (Path(repo_path) / "reports" / f"{lab}.pdf").exists():
        print(red(f"Error: reports/{lab}.pdf not found."))
        failed = True
        report_score = 0
    else:
        report_score = 100
    ```