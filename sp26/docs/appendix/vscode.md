# App. H VSCode 调试配置

## 1. 安装插件

打开 VSCode，点击左侧的扩展按钮，搜索 名称: C/C++ Extension Pack 插件，点击安装。

## 2. 配置文件

在项目根目录下创建 `.vscode/tasks.json` 文件，内容如下：

```json
{
    // See https://go.microsoft.com/fwlink/?LinkId=733558
    // for the documentation about the tasks.json format
    "version": "2.0.0",
    "tasks": [
        {
            "label": "make",
            "type": "shell",
            "command": "make -j10",
        },
        {
            "label": "test2",
            "type": "shell",
            "command": "python3 ./test.py ./compiler lab2",
            "dependsOn": "make"
        },
    ]
}
```

```make``` 为编译任务，```test2``` 为测试任务，```dependsOn``` 为依赖任务。
```label``` 为任务名称，```command``` 为执行的命令。
根据自己实际需求修改。

在项目根目录下创建 `.vscode/launch.json` 文件，内容如下：

```json
{
    "configurations": [
        {
            "name": "(gdb) 启动",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/compiler",
            "args": [
                "${workspaceFolder}/add.sy",
                "${workspaceFolder}/add.s"
            ],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "为 gdb 启用整齐打印",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                },
                {
                    "description": "将反汇编风格设置为 Intel",
                    "text": "-gdb-set disassembly-flavor intel",
                    "ignoreFailures": true
                }
            ],
            "preLaunchTask": "make"
        }
    ]
}
```

```${workspaceFolder}``` 为工作目录，即项目根目录。
```args``` 为命令行运行时的参数，根据实验要求修改。
```preLaunchTask``` 为启动前执行的任务，这里为 ```make``` 。

## 3. 启动调试

点击左侧的运行和调试按钮，选择 ```(gdb) 启动```，添加断点，点击运行按钮即可开始调试。

## 4. 常见问题及解决方案

### 4.1 调试器无法启动
- 确保已安装 gdb
- 检查编译时是否添加了 `-g` 调试信息
- 验证程序路径是否正确

### 4.2 断点无法命中
- 确保源代码与编译后的程序匹配
- 检查是否开启了优化选项（-O2, -O3 等）
- 尝试重新编译程序

### 4.3 调试快捷键
- ++f5++ 启动/继续
- ++f9++ 切换断点
- ++f10++ 单步跳过
- ++f11++ 单步进入
- ++shift+f11++ 单步退出
- ++ctrl+shift+f5++ 重启调试