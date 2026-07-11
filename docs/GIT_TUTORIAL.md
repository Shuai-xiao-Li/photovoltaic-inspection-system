# 🚀 Git 与 GitHub 完整教程 — 从零到精通

> **适用对象**：正在学习 Git 的团队成员
> **项目地址**：`https://github.com/Shuai-xiao-Li/photovoltaic-inspection-system`
> **最后更新**：2026年7月

---

## 📖 目录

1. [Git 是什么？为什么需要它？](#1-git-是什么为什么需要它)
2. [核心概念图解](#2-核心概念图解)
3. [环境准备](#3-环境准备)
4. [第一次上传：将本项目推送到 GitHub（手把手）](#4-第一次上传将本项目推送到-github手把手)
5. [Git LFS：管理大文件（ONNX 模型）](#5-git-lfs管理大文件onnx-模型)
6. [日常开发工作流](#6-日常开发工作流)
7. [分支管理](#7-分支管理)
8. [团队协作：Pull Request 工作流](#8-团队协作pull-request-工作流)
9. [版本回退与撤销](#9-版本回退与撤销)
10. [.gitignore 详解](#10-gitignore-详解)
11. [常见问题与解决方案（FAQ）](#11-常见问题与解决方案faq)
12. [Git 命令速查表](#12-git-命令速查表)

---

## 1. Git 是什么？为什么需要它？

**Git** 是一个分布式版本控制系统。简单来说：

- 📸 **快照功能**：每次 `commit` 相当于给你的代码拍一张快照，可以随时回到任何一个历史版本
- 👥 **团队协作**：多人可以同时修改同一份代码，Git 帮你自动合并
- 🔒 **安全保障**：代码推送到 GitHub 后，即使本地硬盘坏了也不怕丢失
- 📋 **变更追踪**：谁在什么时间改了什么代码，一目了然

**GitHub** 是基于 Git 的代码托管平台，可以理解为"代码的云盘"，但比云盘强大得多。

---

## 2. 核心概念图解

### 2.1 Git 的四个区域

```
┌─────────────────────────────────────────────────────────────────┐
│                        你的电脑（本地）                          │
│                                                                 │
│  ┌──────────┐  git add   ┌──────────┐  git commit ┌──────────┐ │
│  │ 工作区    │ ────────→  │ 暂存区    │ ──────────→ │ 本地仓库  │ │
│  │(Working  │            │(Staging  │             │(Local    │ │
│  │Directory)│  ←────────  │  Area)   │             │  Repo)   │ │
│  └──────────┘ git restore └──────────┘             └────┬─────┘ │
│       │                                                  │      │
└───────┼──────────────────────────────────────────────────┼──────┘
        │                                                  │
        │                                          git push│
        │                                                  ▼
        │                                          ┌──────────┐
        │              git pull / git clone         │ 远程仓库  │
        │◄─────────────────────────────────────────│(GitHub)  │
        │                                          └──────────┘
```

> **工作区**：你能看到的项目文件夹，就是你平时写代码的地方
> **暂存区**：`git add` 后文件进入这里，相当于"打包等待发货"
> **本地仓库**：`git commit` 后文件进入这里，相当于"已入库记录在案"
> **远程仓库**：`git push` 后推送到 GitHub，相当于"发到云端保管"

### 2.2 文件的生命周期

```
  未跟踪(Untracked)  ──git add──→  已暂存(Staged)
         ▲                              │
         │                         git commit
         │                              │
         │                              ▼
  已修改(Modified)  ◄──编辑文件──  已提交(Committed)
         │                              ▲
         │                              │
         └──────── git add ─────────────┘
```

---

## 3. 环境准备

### 3.1 检查 Git 是否已安装

打开 PowerShell 或终端，运行：

```powershell
git --version
```

如果显示版本号（如 `git version 2.45.0.windows.1`），说明已安装。否则请到 [git-scm.com](https://git-scm.com/) 下载安装。

### 3.2 首次配置（全局只需一次）

```bash
# 设置你的名字（将显示在每次提交记录中）
git config --global user.name "李帅"

# 设置你的邮箱（建议使用与 GitHub 账号关联的邮箱）
git config --global user.email "your-email@example.com"

# （可选）设置默认分支名为 main
git config --global init.defaultBranch main

# （可选）让 Git 输出更易读的颜色
git config --global color.ui auto
```

### 3.3 验证配置

```bash
git config --global --list
```

### 3.4 安装 Git LFS（处理大文件必须）

```bash
# 检查是否已安装
git lfs version

# 如果未安装，Windows 用户可以：
# 方式一：使用 winget
winget install GitHub.GitLFS

# 方式二：去 https://git-lfs.github.com/ 下载安装包

# 安装后初始化（全局只需一次）
git lfs install
```

---

## 4. 第一次上传：将本项目推送到 GitHub（手把手）

> ⚠️ **前提条件**：
> - 你已经有了 GitHub 账号
> - 仓库 `photovoltaic-inspection-system` 已在 GitHub 上创建
> - 本地文件夹已通过 `git init` + `git remote add origin` 关联到远程仓库

### 4.1 打开终端，进入项目目录

```powershell
cd D:\桌面\photovoltaic-inspection-system
```

### 4.2 确认远程仓库已关联

```bash
git remote -v
```

你应该看到：
```
origin  https://github.com/Shuai-xiao-Li/photovoltaic-inspection-system.git (fetch)
origin  https://github.com/Shuai-xiao-Li/photovoltaic-inspection-system.git (push)
```

如果没有，执行：
```bash
git remote add origin https://github.com/Shuai-xiao-Li/photovoltaic-inspection-system.git
```

### 4.3 配置 Git LFS（推送前必须做！）

因为项目中有一个 ~23MB 的模型文件 `solar_panel_int8.onnx`，**必须先配置 LFS**：

```bash
# 初始化 LFS
git lfs install

# 追踪 ONNX 模型文件
git lfs track "vision-algorithm/solar_panel_int8.onnx"

# 追踪所有 onnx 格式的文件（推荐，更通用）
git lfs track "*.onnx"
```

这会自动生成一个 `.gitattributes` 文件，**这个文件也必须提交**。

### 4.4 查看当前状态

```bash
git status
```

你会看到大量文件显示为 `Untracked`（红色）或 `Modified`（红色），这是正常的——这些就是你即将上传的内容。

### 4.5 将所有文件添加到暂存区

```bash
# 先添加 .gitattributes（LFS 配置文件）
git add .gitattributes

# 再添加所有其他文件
git add .
```

> 💡 `git add .` 中的 `.` 表示"当前目录下的所有文件"。`.gitignore` 中列出的文件会被自动排除。

### 4.6 确认暂存区内容

```bash
git status
```

现在所有文件应该变成绿色（`Changes to be committed`）。仔细检查一下有没有不该上传的文件。

### 4.7 提交到本地仓库

```bash
git commit -m "feat: 初始化光伏巡检系统完整项目

- 添加 STM32 底盘运动控制固件 (stm32-chassis/)
- 添加飞腾派视觉检测算法 (vision-algorithm/)
- 添加 Qt QML 监控界面 (qt-ui/)
- 添加中英双语 README 与项目文档
- 配置 Git LFS 追踪 ONNX 模型文件
- 添加 MIT 许可证与贡献指南"
```

> 💡 **提交信息规范**：
> - `feat:` 新功能
> - `fix:` 修复 Bug
> - `docs:` 文档更新
> - `chore:` 杂项维护
> - `refactor:` 代码重构
> - 第一行简要概括（不超过 50 字），空一行后写详细说明

### 4.8 推送到 GitHub！

```bash
git push -u origin main
```

> 📝 `-u` 参数的含义是 `--set-upstream`，设置后以后直接 `git push` 就行，不用每次都写 `origin main`。

### 4.9 首次推送可能遇到的认证问题

如果提示输入用户名密码或认证失败：

**方案 A：使用 Personal Access Token（推荐）**
1. 打开 GitHub → Settings → Developer settings → Personal access tokens → Tokens (classic)
2. 点击 "Generate new token"
3. 勾选 `repo` 权限，生成 Token
4. 推送时，密码栏填入这个 Token（不是你的 GitHub 密码！）

**方案 B：使用 GitHub CLI**
```bash
# 安装 GitHub CLI
winget install GitHub.cli

# 登录认证
gh auth login
```

### 4.10 验证上传成功

打开浏览器访问：
```
https://github.com/Shuai-xiao-Li/photovoltaic-inspection-system
```

你应该能看到完整的目录结构和漂亮的 README 页面！🎉

---

## 5. Git LFS：管理大文件（ONNX 模型）

### 5.1 为什么需要 Git LFS？

| 方面 | 普通 Git | Git LFS |
|------|---------|---------|
| 适合的文件 | 文本文件（代码、文档） | 大型二进制文件（模型、图片、视频） |
| 仓库体积 | 每次修改都完整保存，体积急速膨胀 | 只保存指针，真实文件单独存储 |
| 克隆速度 | 大文件越多越慢 | 按需下载，速度快 |

我们的 `solar_panel_int8.onnx` 有 **~23MB**，属于大文件，必须用 LFS。

### 5.2 LFS 的工作原理

```
                     普通文件                    LFS 文件
                   ┌──────────┐              ┌──────────┐
  git add          │ 源代码.py │              │ 模型.onnx │
     │             │  5 KB     │              │  23 MB    │
     ▼             └──────────┘              └──────────┘
  Git 仓库中       │ 完整文件   │              │ 文本指针   │
  存储的是：       │ 内容       │              │ (~200字节) │
                   └──────────┘              └──────────┘
                                                   │
                                                   ▼
                                             ┌──────────┐
                                             │ LFS 服务器│
                                             │ 存储真实  │
                                             │ 大文件    │
                                             └──────────┘
```

### 5.3 常用 LFS 命令

```bash
# 查看当前 LFS 追踪规则
git lfs track

# 查看 LFS 管理的文件状态
git lfs status

# 查看 LFS 管理的文件列表
git lfs ls-files

# 手动拉取 LFS 文件（克隆后使用）
git lfs pull

# 查看 LFS 使用的存储空间
git lfs env
```

### 5.4 GitHub LFS 免费额度

| 项目 | 免费额度 |
|------|---------|
| 存储空间 | 1 GB |
| 每月下载带宽 | 1 GB |

我们的 ONNX 模型只有 23MB，完全够用。

---

## 6. 日常开发工作流

当你完成了首次推送后，日常开发遵循这个循环：

```
  编写/修改代码
       │
       ▼
  git status          ← 查看哪些文件被修改了
       │
       ▼
  git diff             ← 查看具体改了什么（可选）
       │
       ▼
  git add <文件>       ← 把要提交的文件加入暂存区
       │
       ▼
  git commit -m "..."  ← 提交到本地仓库
       │
       ▼
  git push             ← 推送到 GitHub
```

### 实际操作示例

假设你修改了 PID 参数：

```bash
# 1. 查看修改了什么
git status
# 输出: modified: stm32-chassis/CHASSIS/pid.c

# 2. 查看具体改动（可选）
git diff stm32-chassis/CHASSIS/pid.c

# 3. 添加修改的文件
git add stm32-chassis/CHASSIS/pid.c

# 4. 提交
git commit -m "feat: 优化直行 PID 参数，减少震荡"

# 5. 推送
git push
```

### 部分添加技巧

```bash
# 添加某个目录下的所有修改
git add vision-algorithm/

# 添加所有 .py 文件
git add *.py

# 交互式添加（选择性添加某些行）
git add -p
```

---

## 7. 分支管理

### 7.1 什么是分支？

分支就像平行宇宙——你可以在一条支线上放心改代码，不会影响主线 (`main`)。改好之后再合并回来。

```
          feature/improve-pid
         ┌───● ─── ● ─── ●──┐
         │                    │ merge
main ────●────────────────────●────── ▶
         ▲                    ▲
      创建分支             合并回main
```

### 7.2 分支操作

```bash
# 查看所有分支（* 标记当前分支）
git branch

# 创建并切换到新分支
git checkout -b feature/improve-pid

# 在新分支上正常开发、提交
git add .
git commit -m "feat: 改进 PID 参数"

# 推送新分支到 GitHub
git push -u origin feature/improve-pid

# 开发完成后，切回 main 分支
git checkout main

# 将 feature 分支合并到 main
git merge feature/improve-pid

# 推送合并后的 main
git push

# 删除已合并的分支（可选）
git branch -d feature/improve-pid
```

### 7.3 推荐的分支命名规范

| 前缀 | 用途 | 示例 |
|------|------|------|
| `feature/` | 新功能开发 | `feature/add-thermal-camera` |
| `fix/` | Bug 修复 | `fix/pid-oscillation` |
| `docs/` | 文档更新 | `docs/update-readme` |
| `refactor/` | 代码重构 | `refactor/serial-protocol` |
| `test/` | 测试相关 | `test/add-vision-tests` |

---

## 8. 团队协作：Pull Request 工作流

当团队多人协作时，不要直接 push 到 `main`，而是通过 **Pull Request (PR)** 合并代码。

### 8.1 PR 工作流程

```
  1. 创建 feature 分支
         │
  2. 在分支上开发并提交
         │
  3. push 分支到 GitHub
         │
  4. 在 GitHub 上创建 Pull Request
         │
  5. 团队成员 Review 代码
         │
  6. 通过审查后，在 GitHub 上点击 "Merge"
         │
  7. 删除已合并的分支
```

### 8.2 如何创建 PR

1. 推送你的分支到 GitHub：
   ```bash
   git push -u origin feature/my-feature
   ```

2. 打开 GitHub 仓库页面，会看到黄色提示栏 "Compare & pull request"，点击它

3. 填写 PR 标题和描述（我们已经创建了 PR 模板，会自动填充）

4. 点击 "Create pull request"

5. 等待团队成员审查，审查通过后点击 "Merge pull request"

---

## 9. 版本回退与撤销

### 9.1 撤销工作区的修改（还没 add）

```bash
# 撤销单个文件的修改
git restore stm32-chassis/CHASSIS/pid.c

# 撤销所有修改
git restore .
```

### 9.2 撤销暂存区的文件（已 add，还没 commit）

```bash
# 将文件从暂存区移出，但保留修改
git restore --staged stm32-chassis/CHASSIS/pid.c

# 将所有文件从暂存区移出
git restore --staged .
```

### 9.3 撤销最近一次提交（已 commit，还没 push）

```bash
# 撤销提交，但保留代码修改（推荐）
git reset --soft HEAD~1

# 撤销提交，同时撤销暂存
git reset HEAD~1

# 撤销提交，同时丢弃所有修改（⚠️ 危险！不可恢复）
git reset --hard HEAD~1
```

### 9.4 已经 push 到远程的撤销

```bash
# 创建一个"反向提交"来撤销（安全的方式）
git revert HEAD
git push
```

> ⚠️ **永远不要对已经 push 的提交使用 `git reset --hard`**，这会导致远程仓库历史不一致。

### 9.5 查看历史记录

```bash
# 查看提交历史
git log --oneline -10

# 查看带图形的分支历史
git log --oneline --graph --all

# 查看某个文件的修改历史
git log --oneline -- stm32-chassis/CHASSIS/pid.c

# 查看某次提交的具体改动
git show <commit-hash>
```

---

## 10. .gitignore 详解

`.gitignore` 文件告诉 Git 哪些文件不需要版本管理。

### 10.1 本项目的 .gitignore 结构

```gitignore
# Keil MDK 编译产物 —— STM32 固件编译生成的临时文件
OBJ/
*.o
*.d
*.crf
*.axf

# Python 缓存 —— Python 运行时自动生成
__pycache__/
*.pyc

# 大模型文件 —— 排除训练权重和 FP32 模型
*.pth
fp32.onnx
!solar_panel_int8.onnx    # ← 但保留我们的量化推理模型！

# Qt 构建产物
*.pro.user
build-*

# 系统垃圾文件
.DS_Store
Thumbs.db
```

### 10.2 规则语法

| 模式 | 含义 | 示例 |
|------|------|------|
| `*.o` | 忽略所有 .o 文件 | 匹配 `main.o`, `pid.o` 等 |
| `OBJ/` | 忽略整个目录 | 匹配 `OBJ/` 下所有内容 |
| `!important.o` | 例外：不忽略这个文件 | 即使有 `*.o` 规则也保留 |
| `**/logs` | 匹配任意深度的 logs 目录 | 匹配 `a/logs`, `a/b/logs` |
| `doc/*.txt` | 仅匹配一级深度 | 匹配 `doc/a.txt`，不匹配 `doc/sub/a.txt` |

### 10.3 已经被跟踪的文件怎么办？

如果某个文件已经被 Git 跟踪了，后来才加入 `.gitignore`，需要手动移除追踪：

```bash
# 从 Git 中移除追踪（但不删除本地文件）
git rm --cached path/to/file

# 移除整个目录的追踪
git rm --cached -r OBJ/

# 然后提交
git commit -m "chore: 移除不应跟踪的编译产物"
```

---

## 11. 常见问题与解决方案（FAQ）

### ❓ Q1: `git push` 被拒绝（rejected）

**报错信息**：
```
! [rejected]  main -> main (fetch first)
```

**原因**：远程仓库有你本地没有的新提交。

**解决方案**：
```bash
# 先拉取远程更新
git pull --rebase origin main

# 再推送
git push
```

---

### ❓ Q2: 合并冲突（Merge Conflict）

**报错信息**：
```
CONFLICT (content): Merge conflict in stm32-chassis/CHASSIS/pid.c
```

**解决步骤**：
1. 打开冲突文件，找到冲突标记：
   ```c
   <<<<<<< HEAD
   float kp = 2.5;    // 你的修改
   =======
   float kp = 3.0;    // 别人的修改
   >>>>>>> origin/main
   ```

2. 手动编辑，保留你想要的内容，删除冲突标记：
   ```c
   float kp = 2.8;    // 合并后的最终值
   ```

3. 标记冲突已解决并提交：
   ```bash
   git add stm32-chassis/CHASSIS/pid.c
   git commit -m "fix: 解决 PID 参数合并冲突"
   ```

---

### ❓ Q3: 不小心把密码/密钥提交了怎么办？

**紧急处理**：
```bash
# 1. 立即修改泄露的密码/密钥！

# 2. 从历史中彻底删除文件
git filter-branch --force --index-filter \
  "git rm --cached --ignore-unmatch path/to/secret_file" \
  --prune-empty --tag-name-filter cat -- --all

# 3. 强制推送
git push --force
```

> 💡 **预防措施**：敏感信息写在 `.env` 文件中，并在 `.gitignore` 里添加 `.env`。

---

### ❓ Q4: `git push` 提示文件太大

**报错信息**：
```
remote: error: File xxx is 100.00 MB; this exceeds GitHub's file size limit of 100.00 MB
```

**解决方案**：使用 Git LFS（参见第 5 节），或将大文件加入 `.gitignore`。

---

### ❓ Q5: 想要把某次提交修改一下（改提交信息）

```bash
# 修改最近一次提交的信息
git commit --amend -m "新的提交信息"

# 如果已经 push 了，需要强制推送（⚠️ 仅在个人分支使用）
git push --force
```

---

### ❓ Q6: 克隆后 ONNX 模型文件是空的/很小

**原因**：Git LFS 文件需要单独拉取。

**解决方案**：
```bash
git lfs pull
```

---

### ❓ Q7: Windows 上文件路径过长报错

```bash
# 启用长路径支持
git config --global core.longpaths true
```

---

## 12. Git 命令速查表

### 🟢 基础操作

| 命令 | 功能 | 使用场景 |
|------|------|---------|
| `git status` | 查看文件状态 | 每次操作前都看一眼 |
| `git add .` | 添加所有文件到暂存区 | 准备提交时 |
| `git add <文件>` | 添加指定文件 | 只提交部分文件时 |
| `git commit -m "信息"` | 提交到本地仓库 | 完成一个功能点后 |
| `git push` | 推送到远程仓库 | 提交后同步到 GitHub |
| `git pull` | 拉取远程更新 | 开始工作前先同步 |
| `git log --oneline` | 查看简洁提交记录 | 回顾历史 |
| `git diff` | 查看未暂存的改动 | 确认修改内容 |

### 🔵 分支操作

| 命令 | 功能 |
|------|------|
| `git branch` | 查看分支列表 |
| `git checkout -b <名字>` | 创建并切换分支 |
| `git checkout <名字>` | 切换到已有分支 |
| `git merge <名字>` | 合并分支到当前分支 |
| `git branch -d <名字>` | 删除已合并的分支 |

### 🟡 撤销操作

| 命令 | 功能 | 安全等级 |
|------|------|---------|
| `git restore <文件>` | 撤销工作区修改 | ⚠️ 修改会丢失 |
| `git restore --staged <文件>` | 取消暂存 | ✅ 安全 |
| `git reset --soft HEAD~1` | 撤销最近提交，保留修改 | ✅ 安全 |
| `git reset --hard HEAD~1` | 撤销提交+丢弃修改 | 🔴 危险 |
| `git revert HEAD` | 创建反向提交 | ✅ 安全 |

### 🟣 远程仓库

| 命令 | 功能 |
|------|------|
| `git remote -v` | 查看远程仓库地址 |
| `git remote add origin <URL>` | 添加远程仓库 |
| `git clone <URL>` | 克隆远程仓库 |
| `git fetch` | 获取远程更新（不合并） |

### 🟤 Git LFS

| 命令 | 功能 |
|------|------|
| `git lfs install` | 初始化 LFS |
| `git lfs track "*.onnx"` | 追踪文件类型 |
| `git lfs ls-files` | 查看 LFS 文件列表 |
| `git lfs pull` | 下载 LFS 文件 |
| `git lfs status` | 查看 LFS 状态 |

---

## 🎯 本项目的完整上传命令汇总

如果你是第一次上传本项目，按顺序执行以下命令即可：

```bash
# 进入项目目录
cd D:\桌面\photovoltaic-inspection-system

# 配置 LFS
git lfs install
git lfs track "*.onnx"

# 暂存所有文件
git add .gitattributes
git add .

# 查看即将提交的文件（确认无误）
git status

# 提交
git commit -m "feat: 初始化沙戈荒光伏巡检系统完整项目

- stm32-chassis: STM32F103ZET6 底盘运动控制固件
- vision-algorithm: 基于飞腾派的光伏板缺陷检测算法
- qt-ui: Qt QML 实时监控仪表盘界面
- 中英双语 README 与完整项目文档
- MIT 开源许可证
- GitHub Actions CI 配置"

# 推送到 GitHub
git push -u origin main
```

上传成功后，访问你的仓库页面确认一切正常：
👉 **https://github.com/Shuai-xiao-Li/photovoltaic-inspection-system**

---

> 📝 **作者**：李帅、赵禹博、吴坨鑫
> 📅 **最后更新**：2026年7月
> 💡 **遇到问题？** 在仓库的 Issues 页面提问，或联系团队成员协助！
