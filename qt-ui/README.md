# 飞腾派智能巡检车图形化控制终端 (Qt QML UI) — 研电赛提交版

## 一、 项目简介
本项目为全国研究生电子设计竞赛参赛作品——“沙漠光伏板检测机器人系统”的飞腾派上位机可视化交互控制终端。

项目基于 **Qt 5.12.8 + QML** 框架开发，采用现代扁平化扁平玻璃（Glassmorphism）设计风格，提供直观、科技感强的小车行进姿态和检测仪表盘。本界面彻底淘汰了第一阶段开发使用的虚拟随机模拟接口，**全量接入了真实的物理硬件串口（`QSerialPort`）和 AI 检测数据链路**，实现了下位机状态采集、AI 故障识别、地图运动拟真以及下行控制指令发布的闭环集成。

*   **设计开发团队**：李帅、赵禹博、吴坨鑫
*   **开发日期**：6月12号

---

## 二、 核心真实接口设计与技术特色

### 1. 物理串口通信链路 (`QSerialPort`)
本版本在 C++ 核心类中引入了标准的 Qt 串口支持，用以与 STM32 控制底盘进行二进制双向通信：
*   **端口自适应发现**：程序启动后会自动探测并尝试打开 `/dev/ttyUSB0` 串口（USB-to-TTL 接口）。如果未检测到，将自动降级打开 `/dev/ttyS1`（飞腾派通用物理引脚串口）。
*   **断线自动重连**：当串口由于供电抖动或物理断开触发 `ResourceError` 时，系统将自动把在线指示置红（`OFF`），并在后台开启 3 秒周期的自动重连定时器，无需重启 UI 即可自动恢复。
*   **二进制高可靠协议反序列化**：
    数据帧格式为 10 字节定长封包：`0xAA 0x55 [Length=6] [SpeedL] [SpeedH] [VoltL] [VoltH] [Temp] [Fault] [Checksum]`。
    *   **校验与解码**：对接收流进行滑窗帧头对齐，并使用异或校验和（CheckSum）验证完整性。
    *   **单位转换**：提取速度值并物理转换为 $\text{km/h}$ 并自动在表盘绘图更新；提取毫伏级电压，基于 $9.9\text{V} \sim 12.6\text{V}$ 的锂电池工作区间，真实估算剩余电量百分比。

### 2. 真实下行控制指令映射
当用户在 UI 界面点击“手动控制”控制摇杆、前/后/左/右方向按键、或调整速度滑块时，`VehicleData` 模块会实时构造物理命令写入串口发往 STM32 底盘：
*   **前进**：`"vel 150 0\r\n"`
*   **后退**：`"vel -150 0\r\n"`
*   **左转/右转**：`"vel 0 20\r\n"` / `"vel 0 -20\r\n"`
*   **刹车急停**：`"vel 0 0\r\nstop\r\n"`
*   **设置速度**：`"vel [value] 0\r\n"`

### 3. AI 算法状态与巡检指标同步
本终端利用 QML 信号槽机制，将 `DetectionBridge`（读取 Python YOLO 异常检测脚本的 IPC 桥梁）和底盘数据中心 `VehicleData` 进行了逻辑打通：
*   **图像渲染**：以 66Hz 的最高帧率轮询读取 `/tmp/phytium_frame.bmp` 帧（由 AI 算法脚本在处理完毕后写入），直接将带标注框的异常画面渲染至屏幕中心。
*   **指标数据流**：当 `DetectionBridge` 读取到算法累加的 `detection_count` 变化时，立即触发同步更新，直接将真实检测的光伏板总数传递至已巡检面板指示卡中，实现完全真实的数据反馈。

---

## 三、 代码及目录结构说明

本工程已完成清洗，移除了编译临时残留，文件清单如下：
*   `PhytiumCarUI.pro`：项目 QMake 配置文件，已启用 `QT += serialport`。
*   `main.cpp`：C++ 程序的生命周期起点，向 QML 上下文注册核心 C++ 类并连接信号。
*   `src/vehicledata.h` / `vehicledata.cpp`：底盘实时状态总线，实现 `QSerialPort` 异步读取、异或校验、单位物理量映射及手控命令写入。
*   `src/panelmodel.h` / `panelmodel.cpp`：侧边栏受检光伏板卡片的 MVC 模型类，管理故障分类及其置信度的追加渲染。
*   `src/detectionbridge.h` / `detectionbridge.cpp`：与上位机 AI 检测进程的桥梁，负责读取检测 JSON 帧与标注帧图像。
*   `run_system.sh`：飞腾派系统一键全流程拉起脚本，检测摄像头、启动后台 AI 进程（直接默认加载量化版的 `solar_panel_int8.onnx`）、拉起全屏 UI。
*   `qml/main.qml`：主应用窗口，组织状态栏、侧边卡片、中心视频流及底部控制台。
*   `qml/components/`：自定义 QML 复合组件库，包括各种毛玻璃卡片、速度表盘和日志台。

---

## 四、 编译、部署与运行指南

### 1. 安装 Qt 串口开发包
在飞腾派终端运行以下命令，确保系统已安装 Qt 串口及其开发库：
```bash
sudo apt-get install qtserialport5-dev qtdeclarative5-dev
```

### 2. 编译项目
进入 `PhytiumCarUI` 项目目录，使用 qmake 编译：
```bash
cd PhytiumCarUI
qmake
make -j4
```
编译通过后会在当前目录下生成 `PhytiumCarUI` 可执行文件。

### 3. 一键联调运行
将 `飞腾派视觉算法提交版` 中的 Python 脚本和 `solar_panel_int8.onnx` 模型放置于飞腾派中，并修改 `run_system.sh` 脚本中的路径指向。然后，在 UI 目录下执行：
```bash
chmod +x run_system.sh
./run_system.sh
```
该脚本将一键启动摄像头、后台异构加速 AI 检测算法，并立即全屏展开本交互终端，与底盘 STM32 实现全真实链路通信。
