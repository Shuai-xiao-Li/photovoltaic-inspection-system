# Contributing Guidelines / 贡献指南

Welcome to the Desert Photovoltaic Inspection Robot System project! To ensure code quality and team collaboration efficiency, please follow these guidelines when contributing.

欢迎参与“沙戈荒光伏巡检机器人系统”项目！为了保证代码质量和团队协作效率，请在贡献代码时遵循以下指南。

---

## 1. Branching Strategy / 分支策略

We use a structured branching workflow to manage code changes:
我们使用结构化的分支工作流来管理代码变更：

- **`main`**: The stable branch. Code here must always compile and run successfully. Direct commits to `main` are discouraged; all changes should go through Pull Requests.
  **`main` 分支**: 稳定分支。该分支的代码必须始终能够成功编译和运行。不建议直接向 `main` 分支提交代码，所有更改应通过 Pull Request 进行。
- **`feature/xxx`**: Used for developing new features.
  **`feature/xxx` 分支**: 用于开发新功能。
- **`fix/xxx`**: Used for bug fixes and patches.
  **`fix/xxx` 分支**: 用于修复问题和打补丁。
- **`docs/xxx`**: Used for documentation updates.
  **`docs/xxx` 分支**: 用于更新项目文档。

---

## 2. Commit Message Format / 提交信息规范

All commit messages must follow the standard prefixes below to clarify the purpose of the change:
所有提交信息必须遵循以下标准前缀，以明确阐述变更的目的：

- **`feat: [description]`**: A new feature (e.g., `feat: add STM32 speed loop PID control`).
  **`feat: [描述]`**: 新功能（例如：`feat: 添加STM32速度环PID控制`）。
- **`fix: [description]`**: A bug fix (e.g., `fix: resolve motor reversing issue`).
  **`fix: [描述]`**: 问题修复（例如：`fix: 修复右侧电机方向反转问题`）。
- **`docs: [description]`**: Documentation-only changes (e.g., `docs: update serial wiring guide`).
  **`docs: [描述]`**: 仅文档更新（例如：`docs: 更新飞腾派与STM32串口接线说明`）。
- **`refactor: [description]`**: Code changes that neither fix a bug nor add a feature, but improve code structure (e.g., `refactor: optimize serial parser`).
  **`refactor: [描述]`**: 代码重构，既不修复 Bug 也不添加新功能，但改善了代码结构（例如：`refactor: 优化串口解析器`）。
- **`test: [description]`**: Adding or modifying tests (e.g., `test: add motor encoder verification tests`).
  **`test: [描述]`**: 增加或修改测试（例如：`test: 添加电机编码器验证测试`）。
- **`chore: [description]`**: Maintenance tasks, build system updates, or auxiliary tool updates (e.g., `chore: update .gitignore patterns`).
  **`chore: [描述]`**: 杂项维护、构建系统更新或辅助工具更新（例如：`chore: 更新 .gitignore 过滤规则`）。

---

## 3. Code Submission Workflow / 代码提交工作流

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/Shuai-xiao-Li/photovoltaic-inspection-system.git
   ```
2. **Create a Local Branch**: Choose a branch type based on the task:
   ```bash
   git checkout -b feature/your-feature-name
   # or
   git checkout -b fix/bug-description
   ```
3. **Develop and Test**: Implement your changes and verify that they compile and run correctly.
   - For `stm32-chassis`, verify using Keil MDK.
   - For `vision-algorithm`, verify the Python scripts.
   - For `qt-ui`, compile using QMake and run.
4. **Commit Your Changes**: Follow the commit format.
   ```bash
   git add .
   git commit -m "feat: implement motor speed control"
   ```
5. **Push and Submit Pull Request**: Push to your branch and submit a PR to `main` for review.
   ```bash
   git push origin feature/your-feature-name
   ```

---

1. **克隆仓库**:
   ```bash
   git clone https://github.com/Shuai-xiao-Li/photovoltaic-inspection-system.git
   ```
2. **创建本地分支**: 根据任务类型选择合适的分支类型：
   ```bash
   git checkout -b feature/your-feature-name
   # 或
   git checkout -b fix/bug-description
   ```
3. **开发与测试**: 实施您的更改，并验证其是否可正常编译与运行。
   - STM32 底盘固件: 使用 Keil MDK 验证。
   - 视觉算法: 运行 Python 脚本验证。
   - Qt 用户界面: 使用 QMake 编译并运行验证。
4. **提交代码**: 遵循提交信息规范。
   ```bash
   git add .
   git commit -m "feat: 实现电机速度控制"
   ```
5. **推送分支并提交 PR**: 推送至您的远程分支，并向 `main` 分支提交 Pull Request 以供评审。
   ```bash
   git push origin feature/your-feature-name
   ```
