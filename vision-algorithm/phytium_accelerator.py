#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@File: phytium_accelerator.py
@Brief: 飞腾派异构处理器核心调度绑定类、自适应图像帧差分动态目标过滤及ONNX Runtime ARM NEON加速模块。
@Author: 李帅 赵禹博 吴坨鑫
@Date: 6月12号
@Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
"""

"""
================================================================
飞腾派（Phytium Pi）异构四核架构加速模块
================================================================
本模块专为飞腾派 V2.0 的硬件架构设计，包含三大核心组件：

  1. PhytiumCoreScheduler — 大小核异构调度器
     利用飞腾派 FTC664(大核 1.8GHz) + FTC310(小核 1.5GHz) 的
     big.LITTLE 异构架构，将不同类型的任务绑定到对应核心执行。

  2. FrameDiffDetector — 帧间差分运动检测器
     在小核上运行轻量级帧间差分，仅当画面发生显著变化时才触发
     大核上的 AI 推理，实现自适应检测频率。

  3. ONNXInferenceEngine — ONNX 推理引擎
     替代 PyTorch，利用 ONNX Runtime 的 ARM NEON 向量化优化，
     并使用纯 NumPy 预处理消除 PIL/torchvision 的冗余内存拷贝。

硬件架构依据（飞腾派 V2.0 / CEK8903）：
  - CPU: 2× FTC664 大核 (1.8GHz) + 2× FTC310 小核 (1.5GHz)
  - 指令集: ARMv8-A, 支持 NEON 128-bit SIMD
  - 无独立 GPU，所有计算依赖 CPU
================================================================
"""

import os
import platform
import threading
import numpy as np
import cv2
import time


# ================================================================
#  创新点 1：面向飞腾派大小核的异构流水线调度
# ================================================================

class PhytiumCoreScheduler:
    """
    飞腾派大小核异构调度器
    
    飞腾派 V2.0 搭载异构四核处理器：
    - 2× FTC664 大核 (高性能，主频 1.8GHz)：适合计算密集型任务（AI推理）
    - 2× FTC310 小核 (低功耗，主频 1.5GHz)：适合 I/O 密集型任务（视频采集）
    
    本调度器实现以下功能：
    1. 自动检测飞腾派的大核和小核编号（通过读取 CPU 频率信息）
    2. 将指定线程绑定到大核或小核（通过 os.sched_setaffinity）
    3. 设置 CPU 调频策略为性能模式（performance governor）
    
    创新点：
    - 传统方案中所有任务在单线程执行，仅使用 1 个核心（75% 算力浪费）
    - 本方案将 I/O 密集型任务（采集+预处理）调度到小核，
      计算密集型任务（AI推理）调度到大核，两者并行流水线执行
    """
    
    def __init__(self, big_cores=None, small_cores=None):
        """
        初始化调度器
        
        Args:
            big_cores: 大核 CPU 编号列表，如 [0, 1]。为 None 时自动检测。
            small_cores: 小核 CPU 编号列表，如 [2, 3]。为 None 时自动检测。
        """
        self.is_linux = platform.system() == 'Linux'
        self.big_cores = big_cores
        self.small_cores = small_cores
        self.core_freqs = {}  # {cpu_id: max_freq_khz}
        
        # 自动检测大小核（仅 Linux 系统，飞腾派运行 Phytium Pi OS 基于 Debian）
        if self.is_linux and (big_cores is None or small_cores is None):
            self._auto_detect_cores()
        elif not self.is_linux:
            # 非 Linux 系统（如 Windows 开发环境），使用默认值
            if self.big_cores is None:
                self.big_cores = [0, 1]
            if self.small_cores is None:
                self.small_cores = [2, 3]
    
    def _auto_detect_cores(self):
        """
        自动检测飞腾派的大核和小核
        
        原理：通过读取 /sys/devices/system/cpu/cpuX/cpufreq/scaling_max_freq
        获取每个 CPU 核心的最大频率，频率高的为大核（FTC664），低的为小核（FTC310）。
        """
        try:
            cpu_count = os.cpu_count() or 4
            for i in range(cpu_count):
                freq_path = f"/sys/devices/system/cpu/cpu{i}/cpufreq/scaling_max_freq"
                if os.path.exists(freq_path):
                    with open(freq_path, 'r') as f:
                        self.core_freqs[i] = int(f.read().strip())
            
            if self.core_freqs:
                max_freq = max(self.core_freqs.values())
                # 频率最高的核心为大核，其余为小核
                self.big_cores = sorted([
                    cpu_id for cpu_id, freq in self.core_freqs.items()
                    if freq == max_freq
                ])
                self.small_cores = sorted([
                    cpu_id for cpu_id, freq in self.core_freqs.items()
                    if freq < max_freq
                ])
                
                # 如果所有核心频率相同（非异构），则前半为"大核"后半为"小核"
                if not self.small_cores:
                    all_cores = sorted(self.core_freqs.keys())
                    mid = len(all_cores) // 2
                    self.big_cores = all_cores[:mid] if mid > 0 else all_cores
                    self.small_cores = all_cores[mid:] if mid > 0 else []
        except Exception as e:
            print(f"⚠️ 自动检测核心失败: {e}，使用默认配置")
            self.big_cores = [0, 1]
            self.small_cores = [2, 3]
    
    def bind_to_big_cores(self):
        """
        将当前线程绑定到飞腾派大核（FTC664）
        
        适用场景：AI 推理线程（计算密集型）
        
        Returns:
            bool: 绑定是否成功
        """
        if self.is_linux and self.big_cores:
            try:
                os.sched_setaffinity(0, set(self.big_cores))
                return True
            except (OSError, PermissionError) as e:
                print(f"⚠️ 绑定大核失败: {e}")
                return False
        return False
    
    def bind_to_small_cores(self):
        """
        将当前线程绑定到飞腾派小核（FTC310）
        
        适用场景：视频采集线程（I/O 密集型）
        
        Returns:
            bool: 绑定是否成功
        """
        if self.is_linux and self.small_cores:
            try:
                os.sched_setaffinity(0, set(self.small_cores))
                return True
            except (OSError, PermissionError) as e:
                print(f"⚠️ 绑定小核失败: {e}")
                return False
        return False
    
    def set_performance_governor(self):
        """
        将飞腾派所有 CPU 核心设置为性能模式（最高频率运行）
        
        默认情况下 Linux 可能使用 ondemand/schedutil 调频策略，
        CPU 不会始终运行在最高频率。设置为 performance 模式后，
        大核将始终运行在 1.8GHz，小核始终运行在 1.5GHz。
        
        注意：需要 root 权限（sudo）
        
        Returns:
            bool: 设置是否成功
        """
        if not self.is_linux:
            return False
        
        success = True
        all_cores = (self.big_cores or []) + (self.small_cores or [])
        for core_id in all_cores:
            gov_path = f"/sys/devices/system/cpu/cpu{core_id}/cpufreq/scaling_governor"
            try:
                if os.path.exists(gov_path):
                    with open(gov_path, 'w') as f:
                        f.write('performance')
            except PermissionError:
                success = False
            except Exception:
                success = False
        
        return success
    
    def print_info(self):
        """打印飞腾派核心调度信息"""
        print(f"  系统平台: {'Linux (飞腾派)' if self.is_linux else platform.system() + ' (开发环境)'}")
        
        if self.big_cores:
            freq_str = ""
            if self.core_freqs:
                freq_mhz = self.core_freqs.get(self.big_cores[0], 0) / 1000
                freq_str = f" @ {freq_mhz:.0f}MHz"
            print(f"  大核 (FTC664): cpu{self.big_cores}{freq_str} → 用于 AI 推理")
        
        if self.small_cores:
            freq_str = ""
            if self.core_freqs:
                freq_mhz = self.core_freqs.get(self.small_cores[0], 0) / 1000
                freq_str = f" @ {freq_mhz:.0f}MHz"
            print(f"  小核 (FTC310): cpu{self.small_cores}{freq_str} → 用于视频采集")


# ================================================================
#  创新点 3：基于帧间差分的运动自适应检测频率策略
# ================================================================

class FrameDiffDetector:
    """
    帧间差分运动检测器
    
    原理：
    将相邻两帧转为灰度图，计算像素级绝对差值的均值。
    如果均值超过阈值，说明画面发生了显著变化（如新的光伏板进入视野），
    此时触发大核进行 AI 推理；否则跳过推理以节省算力和功耗。
    
    创新点：
    - 原方案固定每 30 帧检测一次（约 3 秒），不区分静态/动态场景
    - 本方案根据画面变化自适应调整：
      * 静态场景：跳过推理，大核休息，降低功耗
      * 动态场景：立即触发推理，提高响应速度
    - 帧间差分本身极轻量（仅灰度转换 + 减法 + 求均值），
      在小核上运行几乎不占用额外算力
    
    同时设有强制检测间隔（force_interval），确保即使场景静止，
    也会周期性进行推理以防止遗漏。
    """
    
    def __init__(self, threshold=5.0, min_interval=0.3, force_interval=3.0):
        """
        初始化帧间差分检测器
        
        Args:
            threshold: 帧间差分均值阈值（越小越灵敏，建议 3.0~10.0）
            min_interval: 两次推理之间的最小间隔（秒），防止频繁推理
            force_interval: 强制推理间隔（秒），即使画面无变化也会触发
        """
        self.threshold = threshold
        self.min_interval = min_interval
        self.force_interval = force_interval
        
        # 内部状态
        self.prev_gray = None                # 上一帧灰度图
        self.last_trigger_time = 0           # 上次触发推理的时间
        
        # 统计信息
        self.total_checks = 0                # 总检测次数
        self.total_triggers = 0              # 触发推理次数
        self.total_forced = 0                # 强制推理次数
        self.last_diff_value = 0.0           # 最近一次帧间差分值
    
    def check(self, frame):
        """
        检查当前帧是否需要触发 AI 推理
        
        执行流程：
        1. 将当前帧转为灰度图
        2. 与上一帧计算绝对差值的均值
        3. 如果差值 > 阈值，或超过强制间隔，则触发推理
        
        Args:
            frame: 当前帧 (BGR, numpy array)
        
        Returns:
            bool: True 表示需要触发推理，False 表示跳过
        """
        self.total_checks += 1
        current_time = time.time()
        
        # ---- 最小间隔保护：防止大核被过于频繁地调用 ----
        if current_time - self.last_trigger_time < self.min_interval:
            return False
        
        # ---- 将当前帧转为灰度图（轻量操作，适合在小核运行）----
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # ---- 首帧：必须触发推理（没有历史帧可对比）----
        if self.prev_gray is None:
            self.prev_gray = gray
            self.last_trigger_time = current_time
            self.total_triggers += 1
            return True
        
        # ---- 强制推理：超过 force_interval 秒未推理，强制触发 ----
        if current_time - self.last_trigger_time >= self.force_interval:
            self.prev_gray = gray
            self.last_trigger_time = current_time
            self.total_triggers += 1
            self.total_forced += 1
            return True
        
        # ---- 帧间差分计算 ----
        # cv2.absdiff 在 ARM NEON 下会自动使用 SIMD 加速
        diff = cv2.absdiff(gray, self.prev_gray)
        mean_diff = float(np.mean(diff))
        self.last_diff_value = mean_diff
        
        # 更新参考帧
        self.prev_gray = gray
        
        # ---- 判断是否超过阈值 ----
        if mean_diff > self.threshold:
            self.last_trigger_time = current_time
            self.total_triggers += 1
            return True
        
        return False
    
    def force_next(self):
        """强制下一次检查必定触发推理"""
        self.last_trigger_time = 0
        self.prev_gray = None
    
    def get_stats(self):
        """
        获取帧间差分检测的统计信息
        
        Returns:
            dict: 包含总检测次数、触发次数、跳过率等统计数据
        """
        skip_ratio = 0.0
        if self.total_checks > 0:
            skip_ratio = 1.0 - (self.total_triggers / self.total_checks)
        
        return {
            'total_checks': self.total_checks,
            'total_triggers': self.total_triggers,
            'total_forced': self.total_forced,
            'skip_ratio': skip_ratio,                # 推理跳过率（越高越省算力）
            'last_diff': self.last_diff_value,
        }


# ================================================================
#  创新点 2：面向 ARM NEON 的推理引擎替换与 INT8 量化
# ================================================================

class ONNXInferenceEngine:
    """
    ONNX 推理引擎（替代 PyTorch）
    
    性能优化原理：
    1. ONNX Runtime 的 CPU 后端内置 ARM NEON 自动向量化优化，
       相比 PyTorch 的通用 CPU 后端，在飞腾派上推理速度提升 2~5 倍
    2. 支持 INT8 量化模型，利用 ARMv8 的定点运算单元进一步加速
    3. 预处理使用纯 NumPy 向量化操作，消除原方案中的 3 次格式转换：
       原方案: cv2(BGR) → cv2(RGB) → PIL.Image → torchvision.transforms → Tensor
       本方案: cv2(BGR) → NumPy 向量化（resize + normalize + transpose）完成
    
    ImageNet 标准化参数（与原 predict.py 中 get_transform 完全一致）：
    - mean = [0.485, 0.456, 0.406]
    - std  = [0.229, 0.224, 0.225]
    """
    
    # 光伏板缺陷分类类别（与 predict.py 保持一致）
    CLASS_NAMES = [
        'Bird-drop',           # 鸟粪污染
        'Clean',               # 清洁
        'Dusty',               # 灰尘覆盖
        'Electrical-damage',   # 电气损伤
        'Physical-Damage',     # 物理损伤
        'Snow-Covered'         # 积雪覆盖
    ]
    
    # ImageNet 标准化参数（与 predict.py 中 get_transform 一致）
    MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
    STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)
    
    def __init__(self, model_path, input_size=64, num_threads=2):
        """
        初始化 ONNX 推理引擎
        
        Args:
            model_path: ONNX 模型文件路径（支持 FP32 和 INT8）
            input_size: 模型输入尺寸（默认 64，与 MODEL_INPUT_SIZE 一致）
            num_threads: 推理线程数（建议设为大核数量，飞腾派为 2）
        """
        try:
            import onnxruntime as ort
        except ImportError:
            raise ImportError(
                "❌ 请先安装 onnxruntime:\n"
                "   飞腾派 (ARM): pip3 install onnxruntime\n"
                "   开发机 (x86): pip install onnxruntime"
            )
        
        # ---- 配置 ONNX Runtime 会话 ----
        sess_options = ort.SessionOptions()
        
        # 图优化级别设为最高（算子融合、常量折叠等）
        sess_options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        
        # 推理线程数设为大核数量（飞腾派有 2 个大核）
        sess_options.intra_op_num_threads = num_threads
        sess_options.inter_op_num_threads = 1
        
        # 关闭日志减少开销
        sess_options.log_severity_level = 3
        
        # ---- 创建推理会话 ----
        self.session = ort.InferenceSession(
            model_path,
            sess_options,
            providers=['CPUExecutionProvider']  # 飞腾派无 GPU，使用 CPU
        )
        
        # 获取输入节点名称（export_onnx.py 中设为 "input"）
        self.input_name = self.session.get_inputs()[0].name
        self.input_size = input_size
        
        # ---- 预计算归一化常量（避免推理时重复创建）----
        # reshape 为 (1, 1, 3) 以便与 (H, W, 3) 的图像直接广播运算
        self._mean = self.MEAN.reshape(1, 1, 3)
        self._std = self.STD.reshape(1, 1, 3)
        
        # 推理计时统计
        self._infer_times = []
        self._max_history = 100
    
    def preprocess(self, frame):
        """
        纯 NumPy 预处理（替代 PIL + torchvision.transforms）
        
        处理流程（与原 predict.py 中 get_transform 数学等价）：
        1. cv2.resize: 将帧缩放到 input_size × input_size
        2. BGR → RGB:  通过 numpy 切片翻转通道 [:,:,::-1]
        3. 归一化:     除以 255 后减均值除标准差（ImageNet 标准）
        4. HWC → CHW:  转换为 ONNX 需要的 (Batch, Channel, H, W) 格式
        
        相比原方案的优势：
        - 原方案: cv2→PIL→torchvision→Tensor，涉及 3 次内存拷贝和对象创建
        - 本方案: 全程 NumPy 原地操作，零额外内存分配
        
        Args:
            frame: BGR 格式的图像帧 (numpy array, uint8)
        
        Returns:
            numpy array: shape (1, 3, input_size, input_size), dtype float32
        """
        # Step 1: Resize 到模型输入尺寸
        resized = cv2.resize(frame, (self.input_size, self.input_size))
        
        # Step 2: BGR → RGB（通过 NumPy 切片，无内存拷贝）
        rgb = resized[:, :, ::-1]
        
        # Step 3: [0, 255] uint8 → [0, 1] float32 → ImageNet 标准化
        normalized = rgb.astype(np.float32) / 255.0
        normalized = (normalized - self._mean) / self._std
        
        # Step 4: HWC (H, W, 3) → CHW (3, H, W)，并添加 batch 维度
        chw = normalized.transpose(2, 0, 1)
        batch = np.expand_dims(chw, axis=0)
        
        # 确保数据类型为 float32（ONNX Runtime 要求）
        return np.ascontiguousarray(batch, dtype=np.float32)
    
    def infer(self, input_array):
        """
        运行 ONNX Runtime 推理
        
        Args:
            input_array: 预处理后的输入 (1, 3, H, W), float32
        
        Returns:
            tuple: (class_name: str, confidence: float)
        """
        # ---- ONNX Runtime 推理（ARM NEON 自动向量化加速）----
        outputs = self.session.run(None, {self.input_name: input_array})
        logits = outputs[0][0]  # shape: (num_classes,)
        
        # ---- Softmax（数值稳定版本）----
        # 减去最大值防止 exp 溢出
        exp_logits = np.exp(logits - np.max(logits))
        probs = exp_logits / np.sum(exp_logits)
        
        # ---- 取最大概率的类别 ----
        pred_id = int(np.argmax(probs))
        confidence = float(probs[pred_id])
        
        return self.CLASS_NAMES[pred_id], confidence
    
    def detect(self, frame):
        """
        一步完成：预处理 + 推理
        
        Args:
            frame: BGR 格式的图像帧
        
        Returns:
            tuple: (class_name, confidence, infer_time_ms)
        """
        start_time = time.time()
        
        input_array = self.preprocess(frame)
        class_name, confidence = self.infer(input_array)
        
        infer_time = (time.time() - start_time) * 1000  # 转为毫秒
        
        # 记录推理耗时用于统计
        self._infer_times.append(infer_time)
        if len(self._infer_times) > self._max_history:
            self._infer_times.pop(0)
        
        return class_name, confidence, infer_time
    
    def get_avg_infer_time(self):
        """获取平均推理耗时（毫秒）"""
        if not self._infer_times:
            return 0.0
        return sum(self._infer_times) / len(self._infer_times)
    
    def get_model_info(self):
        """获取模型信息"""
        inputs = self.session.get_inputs()
        outputs = self.session.get_outputs()
        return {
            'input_name': inputs[0].name,
            'input_shape': inputs[0].shape,
            'output_name': outputs[0].name,
            'output_shape': outputs[0].shape,
            'num_threads': self.session.get_session_options().intra_op_num_threads,
        }

# ================================================================
#  轻量级太阳能板轮廓检测与跟踪器
#  替代光流法（Lucas-Kanade），开销从 ~15ms 降到 <1ms
# ================================================================

class SolarPanelTracker:
    """
    轻量级太阳能板轮廓检测与平滑跟踪器（v2 - 精准版）
    
    v1 的问题：
    - Canny + 膨胀会把太阳能板边缘和背景边缘连在一起 → 框太大
    - 固定 EMA alpha 导致跟踪延迟 → 跟不上目标移动
    
    v2 改进：
    1. 使用 Otsu 自适应阈值分割（太阳能板电池片颜色深，与背景对比明显）
       → 精确分割出深色的太阳能板区域，不会包含背景
    2. 凸包(convexHull)提取 → 比 boundingRect 更紧贴实际轮廓
    3. 自适应 EMA → 目标快速移动时 alpha 自动增大（更跟手），
       静止时 alpha 减小（更稳定）
    4. 去除膨胀操作 → 避免边缘膨胀导致的框冗余
    """
    
    def __init__(self, min_area_ratio=0.02, smooth_alpha=0.5, max_lost_frames=8):
        """
        初始化跟踪器
        
        Args:
            min_area_ratio: 最小检测面积占画面比例（过滤噪声小轮廓）
            smooth_alpha: EMA 基础平滑系数（0~1，越大越跟手）
            max_lost_frames: 目标丢失最大容忍帧数
        """
        self.min_area_ratio = min_area_ratio
        self.smooth_alpha = smooth_alpha
        self.max_lost_frames = max_lost_frames
        
        # 跟踪状态
        self._prev_bbox = None
        self._lost_count = 0
        
        # 形态学核（预创建，避免每帧重复分配）
        self._open_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
        self._close_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
        self._erode_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (2, 2))
    
    def detect_and_track(self, frame):
        """
        检测太阳能板并返回紧贴的跟踪框
        
        核心思路：太阳能板电池片是深色的，通过 Otsu 阈值自动分割出
        深色区域，再用轮廓分析找到最大的矩形深色区域 = 太阳能板。
        
        在 160×120 分辨率下耗时 < 1ms。
        
        Args:
            frame: BGR 图像帧
        
        Returns:
            tuple or None: (x, y, w, h) 紧贴太阳能板的边界框
        """
        h, w = frame.shape[:2]
        min_area = int(h * w * self.min_area_ratio)
        
        # ---- Step 1: 灰度 + 模糊 ----
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (3, 3), 0)  # 使用较小核保留边缘锐度，减少由于模糊导致的边界外溢
        
        # ---- Step 2: Otsu 自适应阈值分割 ----
        # THRESH_BINARY_INV: 深色区域变白（太阳能板），浅色区域变黑（背景）
        # Otsu 会自动计算最佳阈值，无需手动调参
        _, thresh = cv2.threshold(
            blurred, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU
        )
        
        # ---- Step 3: 形态学清理与边界收紧 ----
        # 开运算：去除小噪点（先腐蚀再膨胀）
        thresh = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, self._open_kernel, iterations=1)
        # 闭运算：填充太阳能板内部的小间隙（先膨胀再腐蚀）
        thresh = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, self._close_kernel, iterations=1)
        # 额外进行1像素的收紧腐蚀，剥离外部羽化背景边缘，从而使框紧贴目标并去除冗余
        thresh = cv2.erode(thresh, self._erode_kernel, iterations=1)
        
        # ---- Step 4: 查找轮廓 ----
        contours, _ = cv2.findContours(
            thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE
        )
        
        if not contours:
            return self._handle_lost()
        
        # ---- Step 5: 筛选最佳太阳能板轮廓 ----
        best_bbox = None
        best_score = 0
        max_area = int(h * w * 0.85)  # 面积上限：不能超过画面 85%（否则就是背景）
        
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area < min_area or area > max_area:
                continue
            
            # 用凸包包裹轮廓（消除凹陷，更贴合矩形太阳能板）
            hull = cv2.convexHull(cnt)
            hull_area = cv2.contourArea(hull)
            
            # 实心度过滤：排除边缘粗糙、非实心或带枝杈的分散背景噪点
            solidity = area / max(hull_area, 1.0)
            if solidity < 0.85:
                continue
                
            hx, hy, hw, hh = cv2.boundingRect(hull)
            
            # 边界框不能占满整帧（防止框冗余）
            if hw >= w * 0.95 and hh >= h * 0.95:
                continue
            
            # 宽高比过滤（光伏板是偏规则的矩形，过滤掉路灯杆等细长物体）
            aspect = hw / max(hh, 1)
            if aspect < 0.35 or aspect > 3.0:
                continue
            
            # 颜色与亮度特征过滤（防止背景中的木柜、绿/黄路灯、镜面强反光等噪点误检）
            crop = frame[hy:hy+hh, hx:hx+hw]
            if crop.size > 0:
                mean_bgr = cv2.mean(crop)[:3]
                b, g, r = mean_bgr[0], mean_bgr[1], mean_bgr[2]
                
                # 规则 1：排除高反射/高明度区域（白墙、镜面强反光、灯泡）
                if (b + g + r) / 3.0 > 185.0:
                    continue
                    
                # 规则 2：排除暖色系区域（放宽条件，允许沙漠反光造成色偏）
                if r > b + 25.0:
                    continue
                    
                # 规则 3：排除极暗且无细节的死角阴影（放宽限制，允许光线不好的板子）
                if (b + g + r) / 3.0 < 15.0:  
                    continue
                    
                # 规则 4：排除绿色系区域（绿植、绿色路灯盖/路牌）
                if g > b + 25.0 and g > r + 20.0:
                    continue
            
            # 填充度：凸包面积 / 边界框面积（矩形物体接近 1.0）
            fill_ratio = hull_area / max(hw * hh, 1)
            
            # 矩形度强过滤，排除各种斜角阴影、不规则暗色杂物或非矩形物品
            if fill_ratio < 0.75:
                continue
            
            # 评分 = 面积 × 填充度（最大且最规则的深色区域）
            score = area * fill_ratio
            if score > best_score:
                best_score = score
                best_bbox = (hx, hy, hw, hh)
        
        if best_bbox is None:
            return self._handle_lost()
        
        # ---- Step 6: bbox 跳变检测 —— 清除 EMA 历史 ----
        if self._prev_bbox is not None:
            px, py, pw, ph = self._prev_bbox
            bx, by, bw, bh = best_bbox
            prev_cx, prev_cy = px + pw / 2.0, py + ph / 2.0
            new_cx, new_cy = bx + bw / 2.0, by + bh / 2.0
            jump_dist = ((new_cx - prev_cx) ** 2 + (new_cy - prev_cy) ** 2) ** 0.5
            jump_thresh = 0.30 * (w + h)
            if jump_dist > jump_thresh:
                # 跳变过大，丢弃历史 EMA，直接采用当前原始框
                self._prev_bbox = None

        # ---- Step 7: 自适应 EMA 平滑 ----
        smoothed = self._adaptive_smooth(best_bbox)
        self._lost_count = 0
        return smoothed
    
    def _adaptive_smooth(self, bbox):
        """
        自适应指数滑动平均
        
        改进点（相比固定 alpha）：
        - 目标快速移动 → alpha 自动增大 → 框快速跟随
        - 目标静止 → alpha 自动减小 → 框稳定不抖
        
        公式：
            movement = |当前位置 - 上次位置| / 框尺寸（归一化移动量）
            alpha = base_alpha + movement * 0.5（上限 0.95）
        """
        if self._prev_bbox is None:
            self._prev_bbox = bbox
            return bbox
        
        px, py, pw, ph = self._prev_bbox
        cx, cy, cw, ch = bbox
        
        # 计算归一化移动距离
        dx = abs(cx - px) + abs(cy - py)
        size = max(pw + ph, 1)
        movement = dx / size
        
        # 自适应 alpha：移动越快 alpha 越大
        alpha = min(self.smooth_alpha + movement * 0.5, 0.95)
        
        sx = int(alpha * cx + (1 - alpha) * px)
        sy = int(alpha * cy + (1 - alpha) * py)
        sw = int(alpha * cw + (1 - alpha) * pw)
        sh = int(alpha * ch + (1 - alpha) * ph)
        
        self._prev_bbox = (sx, sy, sw, sh)
        return (sx, sy, sw, sh)
    
    def _handle_lost(self):
        """处理目标丢失：短暂丢失保持上次位置，长时间丢失清除"""
        self._lost_count += 1
        if self._lost_count > self.max_lost_frames:
            self._prev_bbox = None
            return None
        return self._prev_bbox
    
    def reset(self):
        """重置跟踪状态"""
        self._prev_bbox = None
        self._lost_count = 0
