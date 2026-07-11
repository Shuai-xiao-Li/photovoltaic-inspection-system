"""
@File: benchmark.py
@Brief: 量化后INT8模型与原版FP32模型在飞腾派平台上的单推理时延与吞吐性能评测基准。
@Author: 李帅 赵禹博 吴坨鑫
@Date: 6月12号
@Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
"""

import time
import os
import cv2
import numpy as np
import argparse
import subprocess
from phytium_accelerator import ONNXInferenceEngine

def get_cpu_temp():
    """读取 Linux 系统的 CPU 温度"""
    try:
        with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
            temp = int(f.read().strip()) / 1000.0
        return temp
    except Exception:
        return -1.0

def get_cpu_usage():
    """获取 CPU 占用率（使用 top 命令快照）"""
    try:
        # 运行 top 一次并解析 Cpu(s) 这一行
        out = subprocess.check_output("top -b -n 1 | grep 'Cpu(s)'", shell=True).decode('utf-8')
        # out 示例: %Cpu(s):  5.0 us,  2.0 sy, ... 
        # 简单取 us 和 sy 的和，或者让用户手动看
        return out.strip()
    except Exception:
        return "无法自动获取，请手动运行 top/htop"

def benchmark_model(model_path, num_frames=100):
    if not os.path.exists(model_path):
        print(f"[错误] 找不到模型文件: {model_path}")
        return None
        
    print(f"\n--- 正在加载模型: {os.path.basename(model_path)} ---")
    try:
        engine = ONNXInferenceEngine(model_path=model_path, input_size=64, num_threads=2)
    except Exception as e:
        print(f"模型加载失败: {e}")
        return None

    # 生成一个假的 640x480 测试帧
    dummy_frame = np.random.randint(0, 255, (480, 640, 3), dtype=np.uint8)

    print("预热模型 (10帧)...")
    for _ in range(10):
        engine.detect(dummy_frame)

    print(f"开始连续推理 {num_frames} 帧...")
    start_time = time.time()
    total_infer_time = 0.0

    for i in range(num_frames):
        _, _, infer_time_ms = engine.detect(dummy_frame)
        total_infer_time += infer_time_ms

    end_time = time.time()
    wall_time = end_time - start_time
    avg_infer_time = total_infer_time / num_frames

    print(f"【测试结果】")
    print(f"模型: {os.path.basename(model_path)}")
    print(f"总计 Wall Time: {wall_time:.3f} 秒")
    print(f"=> 单帧平均推理耗时 (纯推理阶段): {avg_infer_time:.2f} ms")
    
    return avg_infer_time

def stress_test(model_path, duration_mins=10):
    if not os.path.exists(model_path):
        print(f"[错误] 找不到模型文件: {model_path}")
        return
        
    print(f"\n========== 开始连续运行稳定性测试 ({duration_mins} 分钟) ==========")
    engine = ONNXInferenceEngine(model_path=model_path, input_size=64, num_threads=2)
    dummy_frame = np.random.randint(0, 255, (480, 640, 3), dtype=np.uint8)
    
    start_time = time.time()
    duration_secs = duration_mins * 60
    
    frame_count = 0
    last_print_time = start_time
    
    print(f"初始温度: {get_cpu_temp()} °C")
    
    try:
        while True:
            current_time = time.time()
            elapsed = current_time - start_time
            if elapsed > duration_secs:
                break
                
            engine.detect(dummy_frame)
            frame_count += 1
            
            # 每分钟打印一次状态
            if current_time - last_print_time >= 60:
                temp = get_cpu_temp()
                print(f"[进度: {int(elapsed/60)}/{duration_mins} 分钟] 已处理 {frame_count} 帧, 当前温度: {temp} °C")
                last_print_time = current_time
                
    except KeyboardInterrupt:
        print("\n压力测试被用户手动中断！")
        
    print(f"========== 压力测试结束 ==========")
    print(f"最终温度: {get_cpu_temp()} °C")
    print(f"共计处理: {frame_count} 帧")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="飞腾派 ONNX 推理性能基准测试脚本")
    parser.add_argument("--fp32", type=str, default="solar_panel_fp32.onnx", help="FP32模型路径")
    parser.add_argument("--int8", type=str, default="solar_panel_int8.onnx", help="INT8模型路径")
    parser.add_argument("--frames", type=int, default=100, help="测试帧数")
    parser.add_argument("--stress", type=int, default=0, help="如果大于0，则运行指定分钟数的稳定性压力测试")
    args = parser.parse_args()

    print("========================================")
    print("      飞腾派 (Phytium Pi) 性能硬核测试")
    print("========================================")
    
    print(f"环境初始 CPU 温度: {get_cpu_temp()} °C")
    print(f"当前 CPU 占用情况:\n{get_cpu_usage()}\n")

    # 1. 测 FP32
    if args.fp32 and os.path.exists(args.fp32):
        benchmark_model(args.fp32, num_frames=args.frames)
    else:
        print(f"\n--- [信息] 跳过 FP32 推理测试 (FP32 模型已清理，以减小提交包体积) ---")
    
    # 2. 测 INT8
    benchmark_model(args.int8, num_frames=args.frames)
    
    # 3. 运行稳定性压力测试（仅在显式指定时）
    if args.stress > 0:
        # 默认用量化后的模型进行压力测试，这是工程化最终部署的目标
        stress_test(args.int8, duration_mins=args.stress)
    else:
        print("\n提示：若需运行10分钟稳定性测试，请添加参数：python benchmark.py --stress 10")
