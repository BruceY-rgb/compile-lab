# App. E: RISC-V 32 工具链指南

在类 Unix 操作系统（如 Linux）中，通常需要读取 ELF 文件来加载程序，在我们的 OS 实验中，也尝试加载了一个简易的 ELF 可执行文件。

然而，我们的编译器只完成了从源代码到 RISC-V 汇编代码的转换，并不能直接执行这些汇编代码。因此，接下来我们还需要将 RISC-V 汇编代码进一步转换为 RISC-V 可执行文件，以便在系统中执行。这就需要使用 RISC-V 32 工具链。同时，为了简化实验环境，我们使用 qemu-riscv32 来模拟一个 RISC-V 32 的用户态环境。

## 环境配置指南

遗憾的是 QEMU 官方只提供了 Linux 版本的 qemu-riscv32。因此，Windows 用户推荐使用 WSL 进行实验；macOS 用户则推荐使用 docker 安装一个 amd64 的 Linux 容器进行实验。

### Windows 环境配置指南

首先安装 WSL2。可以参考[官方文档](https://docs.microsoft.com/zh-cn/windows/wsl/install)。建议使用 [Ubuntu 24.04 LTS Windows Subsystem for Linux 2 - Microsoft Store](https://apps.microsoft.com/detail/9nz3klhxdjp5)。

安装完成后，请参阅 [Linux 环境配置指南](#linux) 以继续。

### macOS 环境配置指南

请参考 [Docker 官方文档](https://docs.docker.com/desktop/mac/install/) 安装 Docker Desktop for Mac。安装完成后，可以按照以下步骤来拉取一个与测试环境一致的 Ubuntu 镜像：

```console
$ docker pull ghcr.io/zju-cp/compiler-tester:latest
```

之后，使用以下命令启动一个名为 cp-container 的容器：

```console
$ docker run -it --name cp-container ghcr.io/zju-cp/compiler-tester:latest /bin/bash
```

??? "使用 Dockerfile 自行构建 Docker 镜像"

	如果你不想直接拉取镜像，可以使用下面这个 Dockerfile 一键构建一个 Ubuntu 容器：

	=== "Mac with Apple Silicon"

		```dockerfile
		FROM --platform=linux/arm64 ubuntu:noble

		ENV TZ=Asia/Shanghai

		# use ZJU mirror to speed up apt-get
		RUN sed -i -e 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.zju.edu.cn/ubuntu/|g' \
				-e 's|http://security.ubuntu.com/ubuntu|http://mirrors.zju.edu.cn/ubuntu/|g' \
				/etc/apt/sources.list.d/ubuntu.sources

		RUN apt-get update && apt-get install -y \
			build-essential git tzdata wget tar \
			python3-pip \
			qemu-user \
			flex bison \
			openjdk-17-jdk \
			python3-lark \
			python3-venv \
			python3-toml \
			python3-rich

		RUN wget https://github.com/ZJU-CP/tools/releases/download/v13.2.0/riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-aarch64-linux-ubuntu-24.04.tar.gz && \
			tar -zxvf riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-aarch64-linux-ubuntu-24.04.tar.gz && \
			mv riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-aarch64-linux-ubuntu-24.04 /opt/riscv32-prebuilt && \
			rm riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-aarch64-linux-ubuntu-24.04.tar.gz

		ENV PATH="/opt/riscv32-prebuilt/bin:$PATH"

		CMD ["/bin/bash"]
		```

	=== "Mac with Intel Chip"

		```dockerfile
		FROM --platform=linux/amd64 ubuntu:noble

		ENV TZ=Asia/Shanghai

		# use ZJU mirror to speed up apt-get
		RUN sed -i -e 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.zju.edu.cn/ubuntu/|g' \
				-e 's|http://security.ubuntu.com/ubuntu|http://mirrors.zju.edu.cn/ubuntu/|g' \
				/etc/apt/sources.list.d/ubuntu.sources

		RUN apt-get update && apt-get install -y \
			build-essential git tzdata wget tar \
			python3-pip \
			qemu-user \
			flex bison \
			openjdk-17-jdk \
			python3-lark \
			python3-venv \
			python3-toml \
			python3-rich

		RUN wget https://github.com/ZJU-CP/tools/releases/download/v13.2.0/riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz && \
			tar -zxvf riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz && \
			mv riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04 /opt/riscv32-prebuilt && \
			rm riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz

		ENV PATH="/opt/riscv32-prebuilt/bin:$PATH"

		CMD ["/bin/bash"]
		```

	接下来，在存放这个 Dockerfile 的目录下执行以下命令构建一个名为 cp-image 的镜像：

	```console
	$ docker build -t cp-image .
	```

	构建完成后，使用以下命令，使用刚刚构建的镜像启动一个名为 cp-container 的容器：

	```console
	$ docker run -it --name cp-container cp-image /bin/bash
	```

<!-- !!! tip 
	由于 docker 安装的是一个最小化的 Ubuntu 容器，你可能需要自行安装一些工具，如 `wget` 等。先使用 `apt update` 更新软件源，然后使用 `apt install wget` 安装 wget 等工具。 -->

### Linux 环境配置指南

!!! warning 注意
    请使用 Ubuntu 20.04 及更高的版本，否则可能需要自行编译 qemu 以及 RISC-V 工具链。

=== "bash"

	```console
	# 安装 qemu-user
	$ sudo apt update
	$ sudo apt install qemu-user

	# 下载助教编译好的 RISC-V 工具链并解压
	$ wget https://github.com/ZJU-CP/tools/releases/download/v13.2.0/riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz
	$ tar -zxvf riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz && rm riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz

	# 重命名文件夹，方便使用
	$ mkdir -p $HOME/.local
	$ mv riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04 $HOME/.local/riscv32-prebuilt

	# 将解压后的工具链添加到环境变量中
	$ echo 'export PATH=$PATH:$HOME/.local/riscv32-prebuilt/bin' >> ~/.bashrc
	$ source ~/.bashrc
	```

=== "zsh"


	```console
	# 安装 qemu-user
	$ sudo apt update
	$ sudo apt install qemu-user

	# 下载助教编译好的 RISC-V 工具链并解压
	$ wget https://github.com/ZJU-CP/tools/releases/download/v13.2.0/riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz 
	$ tar -zxvf riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz && rm riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04.tar.gz

	# 重命名文件夹，方便使用
	$ mkdir -p $HOME/.local
	$ mv riscv32-unknown-elf-gcc-13.2.0-rv32gc-ilp32d-x86_64-linux-ubuntu-20.04 $HOME/.local/riscv32-prebuilt

	# 将解压后的工具链添加到环境变量中
	$ echo 'export PATH=$PATH:$HOME/.local/riscv32-prebuilt/bin' >> ~/.zshrc
	$ source ~/.zshrc
	```

??? note "自行编译 RISC-V 32 工具链"
	如果你需要自行编译 RISC-V 32 工具链。你可以根据[这里](https://github.com/riscv-collab/riscv-gnu-toolchain)的指南进行编译：

	```console
	$ git clone https://github.com/riscv/riscv-gnu-toolchain
	$ sudo apt-get install autoconf automake autotools-dev curl python3 python3-pip libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build git cmake libglib2.0-dev libslirp-dev
	$ ./configure --prefix=/opt/riscv --with-arch=rv32gc --with-abi=ilp32d
	$ make -j8	# 或 sudo make -j8
	```

	之后，在环境变量中添加路径 `/opt/riscv/bin` 即可。

### 测试环境

创建一个名为 `test.c` 的文件，内容如下：

```c
#include <stdio.h>

int main() {
    printf("Hello, World!\n");
    return 0;
}
```

使用以下命令编译和运行：

```console
$ riscv32-unknown-elf-gcc test.c -o test
```

如果编译成功，将生成一个名为 `test` 的文件。使用以下命令运行：

```console
$ qemu-riscv32 test
```

如果一切正常，你将看到输出 `Hello, World!`。

你也可以使用以下命令，测试一下本地的 CI 测试脚本能否正常使用这套工具链：

```console
$ python3 sp26-tests/test.py lab0 . --qemu
```

它会像测试 Lab 0 一样，使用你放在 `appends/lab0` 下的代码进行测试，但是使用的是 RISC-V 32 工具链进行编译、运行。

## 工具链指南

### 编译 C 代码到 RISC-V 可执行文件

`riscv32-unknown-elf-gcc` 是一个 RISC-V 32 位的交叉编译器。你可以使用它来编译 C 代码到 RISC-V 可执行文件。他的用法和普通的 gcc 命令类似：

```console
# 查看 test.c 的内容
$ cat test.c
int main() {
    int a = 1, b = 2;
    return a + b;
}

# 编译 test.c 到 test
$ riscv32-unknown-elf-gcc -o test test.c

# 执行 test
$ qemu-riscv32 test

# 查看返回值
$ echo $?
3
```

### 编译 C 代码到 RISC-V 汇编

`riscv32-unknown-elf-gcc` 也可以将 C 代码编译为 RISC-V 汇编代码。使用 `-S` 参数即可：

```console
# 编译 test.c 到 test.s, 使用 -O3 参数优化
$ riscv32-unknown-elf-gcc -O3 -S -o test.s test.c

# 查看 test.s 的内容
$ cat test.s
	.file	"test.c"
	.option nopic
	.attribute arch, "rv32i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.section	.text.startup,"ax",@progbits
	.align	1
	.globl	main
	.type	main, @function
main:
	li	a0,3
	ret
	.size	main, .-main
	.ident	"GCC: (gc891d8dc23e) 13.2.0"
```

!!! tip 
	开启 `-O3` 优化后，GCC 将 1 + 2 的计算优化为了 3。你可以对比不开启优化生成的 `test.s` 文件，看看有什么不同。

### 编译 RISC-V 汇编到 RISC-V 可执行文件

```console
# 编译 test.s 到 test
$ riscv32-unknown-elf-gcc -o test test.s

# 输出结果，能看到是 32 位的 RISC-V 可执行文件
$ file test
test: ELF 32-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, not stripped

# 执行 test
$ qemu-riscv32 test

# 查看返回值
$ echo $?
3
```

### 调试代码

在编译时，你可以使用 `-g` 参数来生成调试信息。这样你就可以使用 GDB 来调试你的 RISC-V 可执行文件。

```console
# 编译 test.s 到 test 并生成调试信息
$ riscv32-unknown-elf-gcc -g -o test test.s
```

在我们编译好的工具链中，还包含了一个 RISC-V 的调试器 `riscv32-unknown-elf-gdb`。你可以使用它来调试你的 RISC-V 可执行文件。这一步需要开启两个 Terminal Session，一个 Terminal 使用 QEMU 启动 Linux，另一个 Terminal 使用 GDB 与 QEMU 远程通信（使用 tcp::1234 端口）进行调试。

```console
# Terminal 1
$ qemu-riscv32 -g 1234 test # 启动 QEMU 并监听 1234 端口

# Terminal 2
$ riscv32-unknown-elf-gdb test
(gdb) target remote :1234   # 连接到 QEMU
(gdb) break main            # 在 main 函数处设置断点
(gdb) stepi                 # 单步执行
(gdb) info registers        # 查看寄存器
(gdb) print $a0             # 查看寄存器值
(gdb) continue              # 继续执行
(gdb) quit                  # 退出 GDB
```

你也可以使用 `gdb-multiarch` 来调试，支持的命令更多。你可以使用以下命令安装：

```console
$ sudo apt install gdb-multiarch
```

!!! warning "由于该工具链是在 Ubuntu 20.04 上编译的，因此在更新的 Ubuntu 上使用 `riscv32-unknown-elf-gdb` 可能会出现动态链接错误。最好使用 `gdb-multiarch`。"

使用方法和上述一致。

更多命令可以参考 GDB 的[官方文档](https://sourceware.org/gdb/current/onlinedocs/gdb/)，或者[100个gdb小技巧](https://wizardforcel.gitbooks.io/100-gdb-tips/content/)。

!!! tip "GDB 插件"
	为了更方便地使用 GDB，你可以安装一些 GDB 插件，例如 [gdb-dashboard](https://github.com/cyrus-and/gdb-dashboard)。

## 高级编译参数指南

在实际编译中，你可能需要使用一些高级参数来控制编译过程。以下是一些常用的参数。

### 禁用默认启动代码

通常，riscv32-unknown-elf-gcc 会在编译时自动插入一个默认的入口点 `_start`。如果你希望替换它，可以使用以下方法：

首先，创建一个自定义的 `_start` 函数，例如在你编译出的汇编代码中加入：

```asm title="test.S"
.section .text
.global _start
_start:
    # 初始化代码，可以包括堆栈指针、全局变量初始化等
    call main       # 调用 main 函数
    li a7, 93       # 系统调用 exit
    ecall           # 退出程序
```

在编译时，使用 `-nostartfiles` 参数禁用默认的启动代码：

```console
$ riscv32-unknown-elf-gcc -nostartfiles -o test test.S
```

`-nostartfiles` 告诉编译器不要链接默认的启动代码，因此，你必须确保自定义的启动代码包含必要的初始化逻辑。

### 禁用标准库

如果想编译一个完全独立于标准库（如本实验提供的工具链中包含的 `newlibc`）的程序，可以使用 `-nostdlib` 参数。该参数会禁用标准库的自动链接，但仍然保留默认的启动代码。

```console
$ riscv32-unknown-elf-gcc -nostdlib -o test test.S
```

### 指定自定义入口点

如果你想指定一个自定义的入口点，可以使用 `-e` 参数：

```console
$ riscv32-unknown-elf-gcc -e my_entry -o test test.S
```

这样，编译器会将 `my_entry` 作为程序的入口点。

### 优化编译

在实验中，你可能需要使用 riscv32-unknown-elf-gcc 生成更高效的代码，来和自己的编译器作为对比，可以使用以下优化选项：

- **开启优化：**`-O1`, `-O2`, `-O3` 或者 `-Ofast`。
- **功能专用优化：**
    - `-funroll-loops`：展开循环，提高性能。
    - `-fomit-frame-pointer`：在某些情况下省略帧指针，节省寄存器。

例如：

```console
$ riscv32-unknown-elf-gcc -O3 -funroll-loops -o test test.c
```

通过实验调整优化选项，可以分析它们对程序大小和运行性能的影响。