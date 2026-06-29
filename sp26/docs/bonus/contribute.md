# Bonus 0: 贡献优质测试用例

!!! info "Bonus 0 最高可获得 1 分加分"

尽管我们已经在实验中提供了大量的测试用例，但是总有一些特殊的情况没有覆盖到。我们欢迎同学们发起 Merge Request 来完善实验仓库，例如补充测试样例，修复错误代码，帮助其他同学更好地完成实验。以下是相关准则和建议：

1. 在 Lab X 开始之后，我们原则上只会合并修复该实验错误的代码。例如，在 Lab 1 开始，即 Lab 0 截止时间 2026.3.15 23:59 之后，我们不会合并增加 Lab 1 测试用例的代码，来保证其他同学不受影响。而对于 Lab 2 补充测试用例的 Merge Request 则暂时不受影响。对于此类 Merge Request，我们会在实验结束后统一处理，不影响 Bonus 加分。
2. 建议想要贡献的同学们在 Fork 本项目之后，始终保持主分支和该项目同步，通过创建独立分支来进行贡献。这样更有利于多次贡献以及保持提交记录整洁。具体操作请参考下文的指南。
<!-- 3. 对于提交的测试点，我们将按照以下标准进行评估：
    - **非常欢迎**：实验中某个大的测试用例无法通过，你发现了问题并提供了一个简单的测试用例，帮助其他同学更好地 debug。我们会优先合并这类测试用例。
    - **慎重考虑**：你发现了一个语法中一个已有测试没覆盖到的 corner case，并为此设计了测试用例。虽然它能够覆盖到更多情况，但可能会要求其他同学付出较多的额外精力来处理此特殊情况。在这种情况下，我们会慎重评估是否合并该测试用例。 -->

## 如何正确发起 Merge Request

这部分内容将帮助你了解如何正确地向测试框架提交贡献。

### Fork 仓库并保持同步

1. 首先在 ZJU Git 上 Fork [sp26-tests](https://git.zju.edu.cn/compiler/sp26-tests) 到你的个人账号下
2. 将你 Fork 的仓库克隆到本地：
    ```console
    $ git clone git@git.zju.edu.cn:your-username/sp26-tests.git
    ```
3. 添加原始仓库作为上游仓库：
    ```console
    $ git remote add upstream git@git.zju.edu.cn:compiler/sp26-tests.git
    ```
4. 在开始修改之前，确保你的仓库与上游仓库保持同步：
    ```console
    $ git fetch upstream
    $ git checkout main
    $ git merge upstream/main
    $ git push origin main
    ```

### 创建分支并进行修改

1. 为你的修改创建一个独立分支：
   ```console
   $ git checkout -b feat/add-xxx
   ```
2. 修改内容，比如增加新的测试用例等
3. 提交你的更改：
   ```console
   $ git add .
   $ git commit -m "feat: add xxx"
   ```
4. 推送到你的远程仓库：
   ```console
   $ git push origin feat/add-xxx
   ```

### 发起 Merge Request

1. 访问你在 ZJU Git 上 Fork 的项目页面，也就是你自己 Fork 的仓库
2. 点击 **Code** > **Merge Requests** > **New Merge Request**，选择：
    - Source branch：你做了修改的分支，如 `feat/add-xxx`
    - Target branch：`compiler/sp26-tests` 的 `main` 分支
3. 填写 Merge Request 信息，包括：
    - 标题：简短描述你的修改
    - 描述：详细说明修改内容和原因
4. 确认无误后，点击 **Create merge request** 提交

!!! tip "注意事项"
    - 每个 Merge Request 只做一件事，比如只提交一个测试用例，便于审查和合并
    - 分支名和提交信息要有意义，方便他人理解
    - 关注 Merge Request 的评论并及时回复和修改
    - 如果出现冲突，需要先同步上游仓库，然后在本地解决冲突：
        ```console
        $ git checkout feat/add-xxx
        $ git fetch upstream
        $ git merge upstream/main
        # 解决冲突后提交
        $ git add .
        $ git commit
        $ git push origin feat/add-xxx
        ```
    - 如果需要更新已提交的 Merge Request，直接在原分支继续提交，Merge Request 会自动更新
    - 如果发现问题需要重新开始，可以关闭当前 Merge Request，创建新的分支重新提交

