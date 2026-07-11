[English README](README_EN.md)

# 沙戈荒光伏巡检系统

[![编译状态](https://img.shields.io/badge/build-passing-brightgreen)](#) [![许可证: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![语言](https://img.shields.io/badge/languages-Python%20%7C%20C%2B%2B%20%7C%20C-blue)](#)

**沙戈荒光伏巡检机器人系统**是一款专为荒漠和恶劣环境下的太阳能光伏电站智能运维设计的多功能机器人平台。该系统以高性能飞腾派上位机和STM32下位机为核心，结合深度学习量化模型进行光伏板缺陷自动检测，并通过高精度的底盘运动控制与Qt QML交互面板，实现自动巡检、异常上报和可视化监控。

---

## 1. 系统架构

本系统由图像采集传感器、负责推理的板载飞腾派计算机、负责可视化监控的 Qt QML 前端，以及负责移动的 STM32 驱动底盘组成。

```mermaid
graph TD
    Camera[Camera] -->|Image| PhytiumPi[Phytium Pi (Vision Algorithm)]
    PhytiumPi -->|UART/Serial| STM32[STM32 Chassis Firmware]
    STM32 --> Motors[Motors]
    PhytiumPi <-->|Monitoring & Control| QtUI[Phytium Qt QML UI]
```

---

## 2. 目录结构

```text
photovoltaic-inspection-system/
├── .gitignore              # Git 过滤规则
├── LICENSE                 # 项目许可证 (MIT)
├── CONTRIBUTING.md         # 贡献指南
├── README.md               # 主双语说明文档
├── README_EN.md            # 英文说明文档
├── README_CN.md            # 中文说明文档
├── docs/                   # 项目文档与教程目录
│   └── GIT_TUTORIAL.md     # Git 与 LFS 使用教程
├── qt-ui/                  # 飞腾派 Qt QML 监控界面
│   ├── PhytiumCarUI/       # QML 应用程序源码
│   └── run_system.sh       # 启动脚本
├── stm32-chassis/          # STM32 底盘运动控制固件
│   ├── BSP/                # 板级支持包
│   └── CHASSIS/            # 底盘运动控制与 PID 调试逻辑
└── vision-algorithm/       # 飞腾派摄像头视觉算法
    ├── solar_panel_int8.onnx # 量化后的缺陷检测模型
    └── phytium_accelerator.py # 硬件加速推理脚本
```

---

## 3. 硬件要求

- **STM32底盘控制器**: STM32F103ZET6 开发板，负责电机PID调速、电池电压监测与传感器读取。
- **上位机平台**: 飞腾派（Phytium Pi）单板计算机，运行视觉识别算法与Qt QML监控界面。
- **外设与执行机构**: 高清USB相机/MIPI相机、四驱带编码器的直流减速电机底盘、舵机云台。

---

## 4. 快速入门

### 4.1 stm32-chassis (Keil 编译与烧录)
1. 使用 Keil MDK (uVision5) 打开 `stm32-chassis/` 目录下的工程文件。
2. 点击编译（Build），确保无任何编译错误。
3. 通过 ST-Link 或 J-Link 将生成的 hex/axf 固件烧录至 STM32F103ZET6 开发板中。

### 4.2 vision-algorithm (Python 算法部署)
1. 进入 `vision-algorithm/` 目录。
2. 安装运行环境依赖项（如 OpenCV, ONNX Runtime 等）：
   ```bash
   pip install opencv-python onnxruntime
   ```
3. 执行目标检测算法脚本：
   ```bash
   python realtime_detector_accel.py
   ```

### 4.3 qt-ui (Qt QML 编译与运行)
1. 确保飞腾派上已安装 Qt 5.15 及以上版本（包含 QML 运行环境）。
2. 进入 `qt-ui/PhytiumCarUI` 目录。
3. 执行以下命令进行编译和运行：
   ```bash
   qmake PhytiumCarUI.pro
   make
   ./PhytiumCarUI
   ```
4. 或者，您可以使用根目录/UI下的 `run_system.sh` 脚本同时启动视觉算法和监控界面。

---

## 5. 作者与贡献者

- 李帅
- 赵禹博
- 吴坨鑫

---

## 6. 许可证

本项目遵循 [MIT 许可证](LICENSE)。
