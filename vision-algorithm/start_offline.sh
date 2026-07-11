#!/bin/bash
# ================================================================
# @File: start_offline.sh
# @Brief: 脱机测试自动监控与异常自恢复脚本，确保Python崩溃时可自动拉起重启并写本地调试日志。
# @Author: 李帅 赵禹博 吴坨鑫
# @Date: 6月12号
# @Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
# ================================================================

# 飞腾派脱机调试一键启动脚本
# 运行此脚本会自动将所有输出（包括错误）保存到 offline_debug.log 文件中，方便事后分析。

echo "========================================="
echo "[System] 开始飞腾派脱机测试启动程序..."
echo "[System] 当前时间: $(date)"
echo "========================================="

# 设置显示环境变量（如果需要）
export DISPLAY=:0

while true; do
    echo "========================================="
    echo "[System] 正在启动 Python 主程序..."
    
    # 记录日志文件
    LOG_FILE="offline_debug_$(date +%Y%m%d_%H%M%S).log"
    echo "[System] 所有运行日志将保存至: $LOG_FILE"
    
    # 启动 Python 主程序，并将标准输出和错误输出重定向到终端和日志文件
    python3 -u realtime_detector_accel.py --model solar_panel_fp32.onnx --min_laplacian_var 0 --no_save --no_show 2>&1 | tee "$LOG_FILE"
    
    echo "[System] 程序意外退出，3秒后自动重启..."
    sleep 3
done

echo "========================================="
echo "[System] 程序运行结束"
echo "========================================="
