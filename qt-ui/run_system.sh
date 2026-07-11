#!/bin/bash
# ================================================================
# @File: run_system.sh
# @Brief: 飞腾派系统一键稳定拉起脚本，包含旧进程清理、摄像头自探测、后台 AI 推理（INT8量化版）后台守护与全屏 Qt UI 界面的联动启动。
# @Author: 李帅 赵禹博 吴坨鑫
# @Date: 6月12号
# @Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
# ================================================================


# =================================================================
# 飞腾派沙漠光伏板检测系统 — 一键稳定运行脚本 (带信号捕获清理与无窗口优化)
# =================================================================

ALGO_PID=""

# 定义退出时的清理函数
cleanup() {
    echo ""
    if [ -n "$ALGO_PID" ]; then
        echo "=== [退出清理] 正在安全杀死后台检测算法进程 (PID: $ALGO_PID)... ==="
        kill -9 $ALGO_PID 2>/dev/null
    fi
    echo "=== [退出清理] 正在清理其他可能残留的算法实例... ==="
    pkill -f realtime_detector_accel.py 2>/dev/null
    echo "=== 系统已安全关闭 ==="
    exit 0
}

# 注册信号捕获：当按下 Ctrl+C (SIGINT)、SIGTERM 或脚本意外退出 (EXIT) 时触发 cleanup
trap cleanup INT TERM EXIT

# 1. 预先清理可能残留的旧 python 进程
echo "=== [步骤 1/4] 清理残留进程 ==="
pkill -f realtime_detector_accel.py 2>/dev/null
sleep 1

# 2. 自动检测摄像头索引
echo "=== [步骤 2/4] 自动探测摄像头设备 ==="
CAMERA_ID=0
if [ -e /dev/video8 ]; then
    CAMERA_ID=8
    echo "  📍 检测到 /dev/video8，选择摄像头索引: 8"
elif [ -e /dev/video0 ]; then
    CAMERA_ID=0
    echo "  📍 检测到 /dev/video0，选择摄像头索引: 0"
else
    CAMERA_ID=0
    echo "  ⚠️ 未找到 /dev/video0 或 /dev/video8，尝试默认索引: 0"
fi

# 3. 启动后台算法 (增加 --no_show 隐藏 OpenCV 独立窗口，大幅降低 CPU 开销)
echo "=== [步骤 3/4] 启动后台算法 ==="
cd /home/user/Desktop/PhytiumProject/Solar-Panel-Fault-Detection-main/ || { echo "❌ 找不到算法目录"; exit 1; }

# 确认模型文件存在
if [ ! -f "solar_panel_int8.onnx" ]; then
    echo "❌ 错误: 找不到模型文件 solar_panel_int8.onnx"
    exit 1
fi

python3 realtime_detector_accel.py --model solar_panel_int8.onnx --camera $CAMERA_ID --no_save --no_show > detector.log 2>&1 &
ALGO_PID=$!

# 等待 3 秒检查算法是否正常运行
sleep 3
if ps -p $ALGO_PID > /dev/null; then
    echo "  ✅ 算法启动成功，PID: $ALGO_PID"
else
    echo "  ❌ 算法启动失败！日志输出如下："
    cat detector.log
    exit 1
fi

# 4. 运行 Qt UI
echo "=== [步骤 4/4] 启动 Qt UI 界面 ==="
cd /home/user/Desktop/PhytiumProject/ui/ || { echo "❌ 找不到 UI 目录"; exit 1; }

if [ ! -f "./PhytiumCarUI" ]; then
    echo "❌ 错误: 找不到编译好的 UI 可执行文件 PhytiumCarUI，请先编译。"
    exit 1
fi

# 启动 UI (前台运行，阻塞等待其关闭。在此期间 Ctrl+C 会被 trap 捕获，触发 cleanup)
./PhytiumCarUI
