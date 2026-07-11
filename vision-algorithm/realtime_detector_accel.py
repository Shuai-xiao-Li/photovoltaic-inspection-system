#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@File: realtime_detector_accel.py
@Brief: 飞腾派上位机实时检测状态机，多线程并发调度摄像头读取、光伏板ROI截取、AI分类与下位机指令控制。
@Author: 李帅 赵禹博 吴坨鑫
@Date: 6月12号
@Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
"""

"""
================================================================
飞腾派加速版 — 沙漠光伏板异常检测系统
================================================================
基于飞腾派 V2.0 异构四核架构（FTC664大核 + FTC310小核）的
边缘端视觉检测加速方案。

与原版 realtime_detector.py 的核心区别：
┌──────────────┬───────────────────────┬─────────────────────────┐
│   对比项      │   原版 (PyTorch)      │   加速版 (本文件)        │
├──────────────┼───────────────────────┼─────────────────────────┤
│ 推理引擎     │ PyTorch model()       │ ONNX Runtime session()  │
│ 模型精度     │ FP32 (94MB)           │ INT8 (~24MB)            │
│ 线程模型     │ 单线程串行            │ 双线程流水线 (大小核)    │
│ 核心调度     │ OS 默认              │ 小核采集 + 大核推理      │
│ 预处理       │ PIL + torchvision     │ 纯 NumPy 向量化         │
│ 检测触发     │ 固定每 30 帧          │ 帧间差分自适应           │
│ 依赖库       │ torch, torchvision    │ onnxruntime, numpy      │
└──────────────┴───────────────────────┴─────────────────────────┘

使用方法：
  python3 realtime_detector_accel.py \\
      --model solar_panel_int8.onnx \\
      --camera 0 --no_save
================================================================
"""

import cv2
import time
import threading
import queue
import argparse
import os
import json
import tempfile
import numpy as np
from datetime import datetime

from phytium_accelerator import (
    PhytiumCoreScheduler,
    FrameDiffDetector,
    ONNXInferenceEngine,
    SolarPanelTracker
)
from serial_bridge import SmartRoverController



# ==================== 可配置参数（与原版保持一致）====================
CENTER_X = 0                        # 全局中心坐标 X（供外部读取）
CENTER_Y = 0                        # 全局中心坐标 Y（供外部读取）
FLAG = 0                            # 全局标志位，1表示工作被截图动作暂停中，0表示正常检测中
PRINT_INTERVAL = 2.0                # 检测结果打印间隔（秒）
CENTER_PRINT_INTERVAL = 0.5         # 中心坐标打印间隔（秒）
MODEL_INPUT_SIZE = 64               # 模型输入尺寸（与训练时一致）
CAMERA_WIDTH = 640                  # 摄像头采集宽度
CAMERA_HEIGHT = 480                 # 摄像头采集高度
CAMERA_FPS = 15                     # 摄像头帧率
CAMERA_RECONNECT_DELAY = 2.0        # 摄像头重连等待时间（秒）
WINDOW_SCALE = 2.0                  # 显示窗口缩放比例
AUTO_CAPTURE_THRESHOLD = 50         # 自动截图中心区域阈值（像素）
AUTO_CAPTURE_CONSECUTIVE = 4        # 连续 N 次满足条件自动截图
AUTO_CAPTURE_SLEEP = 5              # 截图后休息时间（秒）
CONFIDENCE_THRESHOLD = 0.70         # 分类置信度阈值

# ==================== 加速版新增参数 ====================
FRAME_DIFF_THRESHOLD = 5.0          # 帧间差分阈值（越小越灵敏）
FRAME_DIFF_MIN_INTERVAL = 0.3       # 帧间差分最小触发间隔（秒）
FORCE_DETECT_INTERVAL = 3.0         # 强制检测间隔（秒），即使画面无变化
PERF_PRINT_INTERVAL = 10.0          # 性能统计打印间隔（秒）
MIN_LAPLACIAN_VAR = 0.0             # 设为0以关闭模糊检测（刹车抖动会导致误判目标丢失）

# ==================== IPC 输出参数（与 Qt UI 通信）====================
IPC_JSON_PATH = "/tmp/phytium_detection.json"     # 检测结果 JSON
IPC_FRAME_PATH = "/tmp/phytium_frame.bmp"          # 带标注的实时帧 BMP
IPC_WRITE_INTERVAL = 0.02                          # IPC 写入间隔（秒，极速响应)
# 异常分类映射
OCCLUSION_CLASSES = {'Bird-drop', 'Dusty', 'Snow-Covered'}
DAMAGE_CLASSES = {'Electrical-damage', 'Physical-Damage'}
# ==============================================================


class AccelSolarPanelDetector:
    """
    飞腾派加速版光伏板异常检测器
    
    架构设计（异构流水线）：
    
    ┌─────────────────────────────┐    ┌─────────────────────────────┐
    │  采集线程 (FTC310 小核)      │    │  推理线程 (FTC664 大核)      │
    │                             │    │                             │
    │  ① 摄像头读帧               │    │  ④ 纯 NumPy 预处理          │
    │  ② 通道修复                 │──→ │  ⑤ ONNX Runtime 推理        │
    │  ③ 帧间差分判断             │队列 │  ⑥ Softmax 后处理           │
    │                             │    │                             │
    └─────────────────────────────┘    └─────────────────────────────┘
                    │                              │
                    ↓                              ↓
            最新帧缓冲区                     检测结果缓冲区
                    │                              │
                    └──────────┐  ┌────────────────┘
                               ↓  ↓
                    ┌─────────────────────┐
                    │  主线程 (显示+控制)   │
                    │  显示画面 / 按键处理  │
                    │  自动截图 / 录像     │
                    │  性能统计打印        │
                    └─────────────────────┘
    """
    
    def __init__(self, engine, scheduler, diff_detector, tracker,
                 save_video=True, confidence_threshold=0.5,
                 camera_width=320, camera_height=240,
                 min_laplacian_var=60.0, no_show=False, track_only=False):
        """
        初始化加速版检测器
        
        Args:
            engine: ONNXInferenceEngine 推理引擎实例
            scheduler: PhytiumCoreScheduler 大小核调度器实例
            diff_detector: FrameDiffDetector 帧间差分检测器实例
            tracker: SolarPanelTracker 太阳能板轮廓跟踪器实例
            save_video: 是否保存录像
            confidence_threshold: 分类置信度阈值（低于此值的检测结果被忽略）
            camera_width: 摄像头采集宽度
            camera_height: 摄像头采集高度
            min_laplacian_var: 最小拉普拉斯方差阈值
        """
        self.engine = engine
        self.scheduler = scheduler
        self.diff_detector = diff_detector
        self.tracker = tracker
        self.save_video = save_video
        self.confidence_threshold = confidence_threshold
        self.camera_width = camera_width
        self.camera_height = camera_height
        self.min_laplacian_var = min_laplacian_var
        self.no_show = no_show
        
        self.track_only = track_only
        
        # 写入系统加载画面与状态数据，通知 Qt UI 正在加载中
        self.write_loading_screen("系统加载中: 等待下位机启动...", 10)
        
        # 兼容不同版本的 serial_bridge.py 并进行警告提示
        import inspect
        sig = inspect.signature(SmartRoverController.__init__)
        
        def update_init_status(status_text, progress):
            self.write_loading_screen(status_text, progress)
            
        rover_kwargs = {'track_only': self.track_only}
        if 'camera_width' in sig.parameters:
            rover_kwargs['camera_width'] = self.camera_width
        if 'camera_height' in sig.parameters:
            rover_kwargs['camera_height'] = self.camera_height
        if 'status_cb' in sig.parameters:
            rover_kwargs['status_cb'] = update_init_status
            
        self.rover = SmartRoverController(**rover_kwargs)
        self.panel_id = 1
        
        # ---- 新增中心坐标与暂停标志位属性 ----
        self.CENTER_X = 0
        self.CENTER_Y = 0

        self.FLAG = 0
        self.focus_stable_count = 0
        self._focus_is_blurry = False
        
        # ---- 历史预测队列（时间滑动窗口滤波，降低错检率）----
        self.prediction_history = []
        self.history_size = 5
        
        # ---- 运行状态 ----
        self.running = False
        self.frame_count = 0
        self.detection_count = 0
        self.start_time = None
        self.img_center_x = None
        self.img_center_y = None
        self.screenshot_count = 0
        
        # ---- 最近一次检测结果（与原版数据结构一致）----
        self.last_detection = {
            'bbox': None,           # (x, y, w, h) 检测框
            'class_name': None,     # 分类名称
            'confidence': 0.0,      # 置信度
            'timestamp': 0          # 检测时间戳
        }
        
        # ---- 自动截图相关 ----
        self.center_stable_count = 0
        self.after_capture_sleep_until = 0
        
        # ---- 打印计时 ----
        self.last_print_time = 0
        self.last_center_print_time = 0
        self.last_perf_print_time = 0
        
        # ---- 线程间通信 ----
        # 最新帧缓冲区（采集线程写入，主线程读取）
        self._latest_frame = None
        self._frame_lock = threading.Lock()
        
        # 推理任务队列（采集线程提交，推理线程消费）
        # maxsize=1: 只保留最新帧，丢弃旧帧，避免推理延迟累积
        self._inference_queue = queue.Queue(maxsize=1)
        
        # 推理结果缓冲区（推理线程写入，主线程读取）
        self._latest_result = None
        self._result_lock = threading.Lock()
        self._result_counter = 0    # 结果计数器，用于检测是否有新结果
        self._processed_counter = 0  # 主线程已处理的结果计数
        
        # 强制检测标志
        self._force_detect = True   # 启动时强制第一次检测
        
        # 摄像头状态
        self._camera_ok = False
        self._camera_width = 0
        self._camera_height = 0
        
        # 跟踪框缓冲区（采集线程写入，主线程读取）
        self._tracked_bbox = None  # (x, y, w, h) 紧贴太阳能板的边界框
        self._bbox_lock = threading.Lock()
        
        # ---- IPC 输出状态（与 Qt UI 共享数据）----
        self._last_ipc_write_time = 0
        self._prev_ipc_class = None         # 上一次上报的分类（用于变化检测）
        self._occlusion_count = 0           # 累计遮挡异常数
        self._damage_count = 0              # 累计损伤异常数
    
    def write_loading_screen(self, status_text="STATUS: INITIALIZING...", progress=50):
        """写入初始化加载画面与状态数据，通知 Qt UI 系统正在加载"""
        try:
            import cv2
            import numpy as np
            import json
            import os
            # 创建深蓝色背景图像 (480x640x3)
            img = np.zeros((480, 640, 3), dtype=np.uint8)
            img[:] = (40, 24, 16)  # BGR 格式的深蓝色/灰蓝色
            
            # 绘制精致的科技感加载提示界面
            cv2.putText(img, "PHYT-CAR INSPECTION SYSTEM", (60, 150), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (240, 240, 240), 2, cv2.LINE_AA)
            cv2.putText(img, "==============================", (60, 180), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (100, 100, 100), 2, cv2.LINE_AA)
            cv2.putText(img, status_text, (60, 240), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (52, 211, 153), 2, cv2.LINE_AA)
            cv2.putText(img, "Lower controller (STM32) boot wait (3.5s)", (60, 280), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (180, 180, 180), 1, cv2.LINE_AA)
            cv2.putText(img, "Gyro DMP Calibration in progress...", (60, 310), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (180, 180, 180), 1, cv2.LINE_AA)
            
            # 进度条
            bar_width = int((580 - 60) * (progress / 100.0))
            cv2.rectangle(img, (60, 360), (580, 380), (50, 50, 50), -1)
            cv2.rectangle(img, (60, 360), (60 + bar_width, 380), (52, 211, 153), -1)
            
            # 原子写入 BMP 帧画面
            tmp_frame = IPC_FRAME_PATH + ".tmp"
            ret, buf = cv2.imencode('.bmp', img)
            if ret:
                with open(tmp_frame, 'wb') as f:
                    f.write(buf.tobytes())
                os.replace(tmp_frame, IPC_FRAME_PATH)
                
            # 原子写入 JSON 状态文件 (class_name 设为 '系统加载中...')
            tmp_json = IPC_JSON_PATH + ".tmp"
            json_data = {
                "timestamp": time.time(),
                "class_name": "系统加载中...",
                "confidence": 0.0,
                "center_x": 0,
                "center_y": 0,
                "flag": 1,
                "fps": 0.0,
                "frame_count": 0,
                "detection_count": 0,
                "occlusion_count": 0,
                "damage_count": 0,
                "infer_time_ms": 0.0,
                "camera_ok": False,
                "uptime_seconds": 0,
                "bbox": []
            }
            with open(tmp_json, 'w') as f:
                f.write(json.dumps(json_data))
            os.replace(tmp_json, IPC_JSON_PATH)
            print("[System] 已写入初始化加载画面与状态数据。")
        except Exception as e:
            print(f"[System] 写入加载画面失败: {e}")
    
    def _pixel_to_center_coords(self, px, py):
        """
        将像素坐标转换为以图像中心为原点的坐标系
        （与原版 realtime_detector.py 完全一致）
        """
        coord_x = px - self.img_center_x
        coord_y = self.img_center_y - py
        return coord_x, coord_y
    
    def _open_camera(self, camera_id):
        """打开摄像头并设置参数"""
        cap = cv2.VideoCapture(camera_id)
        if not cap.isOpened():
            return None, 0, 0
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.camera_width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.camera_height)
        cap.set(cv2.CAP_PROP_FPS, CAMERA_FPS)
        ret, frame = cap.read()
        if not ret or frame is None:
            cap.release()
            return None, 0, 0
        height, width = frame.shape[:2]
        return cap, width, height
    
    def _fix_frame_channels(self, frame):
        """
        修复帧的通道数（某些摄像头可能输出非标准通道数）
        （与原版 realtime_detector.py 完全一致）
        """
        if frame is None:
            return None
        if len(frame.shape) == 2:
            return cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
        elif frame.shape[2] == 2:
            return cv2.merge([frame[:, :, 0], frame[:, :, 0], frame[:, :, 0]])
        elif frame.shape[2] == 4:
            return cv2.cvtColor(frame, cv2.COLOR_BGRA2BGR)
        else:
            return frame
    
    # ================================================================
    #  采集线程（绑定到飞腾派 FTC310 小核）
    # ================================================================
    
    def _capture_worker(self, camera_id):
        """
        【采集线程】运行在飞腾派 FTC310 小核上
        
        职责：
        1. 从摄像头持续读取帧
        2. 修复通道格式
        3. 运行帧间差分，判断是否需要触发推理
        4. 将需要推理的帧提交到推理队列
        5. 将最新帧存入缓冲区供主线程显示
        
        创新点体现：
        - 采集和预判断在小核上完成，不占用大核算力
        - 帧间差分极轻量（灰度转换 + 减法），适合小核
        - 通过队列异步提交，采集不会被推理阻塞
        """
        # ---- 绑定到飞腾派小核 ----
        bound = self.scheduler.bind_to_small_cores()
        if bound:
            print(f"  📌 采集线程已绑定到小核 cpu{self.scheduler.small_cores}")
        
        # ---- 打开摄像头 ----
        cap, width, height = self._open_camera(camera_id)
        if cap is not None:
            self._camera_ok = True
            self._camera_width = width
            self._camera_height = height
            self.img_center_x = width // 2
            self.img_center_y = height // 2
            # 丢弃前几帧（等待摄像头稳定）
            for _ in range(5):
                cap.read()
        
        reconnect_attempt = False
        
        while self.running:
            # ---- 摄像头断开重连逻辑 ----
            if cap is None or not cap.isOpened():
                self._camera_ok = False
                if not reconnect_attempt:
                    print("\n⚠️ [采集线程] 摄像头已断开，尝试重连...")
                    reconnect_attempt = True
                if cap is not None:
                    cap.release()
                time.sleep(CAMERA_RECONNECT_DELAY)
                cap, width, height = self._open_camera(camera_id)
                if cap is not None:
                    print("✅ [采集线程] 摄像头重连成功！")
                    self._camera_ok = True
                    self._camera_width = width
                    self._camera_height = height
                    self.img_center_x = width // 2
                    self.img_center_y = height // 2
                    reconnect_attempt = False
                    self.diff_detector.force_next()  # 重连后强制检测
                    self._force_detect = True
                    for _ in range(5):
                        cap.read()
                continue
            
            # ---- 读取帧 ----
            ret, frame = cap.read()
            if not ret or frame is None:
                print("\n⚠️ [采集线程] 读帧失败，准备重连...")
                cap.release()
                cap = None
                continue
            
            # ---- 通道修复 ----
            frame = self._fix_frame_channels(frame)
            if frame is None:
                continue
            
            # ---- 更新最新帧缓冲区（供主线程显示）----
            with self._frame_lock:
                self._latest_frame = frame.copy()
                self.frame_count += 1
            
            # ---- 暂停检测状态判断：如果处于截图暂停期，则不执行任何检测与跟踪 ----
            if self.FLAG == 1:
                with self._bbox_lock:
                    self._tracked_bbox = None
                continue
                
            # ---- 模糊对焦检测（防对焦抖动错检）----
            if self.min_laplacian_var > 0:
                # 缩放图像至 80x60 进行拉普拉斯计算，大幅降低 CPU 开销，适配飞腾派
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                small_gray = cv2.resize(gray, (80, 60))
                lap_var = cv2.Laplacian(small_gray, cv2.CV_64F).var()
                
                if lap_var < self.min_laplacian_var:
                    # 处于对焦模糊状态，仅阻止推理，不打断跟踪和已有的识别结果
                    self.focus_stable_count = 0
                    self._focus_is_blurry = True
                else:
                    self.focus_stable_count += 1
                    # 连续 3 帧（0.2秒）保持清晰即恢复推理，不要等太久
                    if self.focus_stable_count >= 3:
                        self._focus_is_blurry = False
                    else:
                        self._focus_is_blurry = True
            else:
                self._focus_is_blurry = False
                
            # ---- 轮廓跟踪：在小核上每帧运行，耗时 <1ms ----
            # 替代光流法（耗时 ~15ms），使用 Canny + 轮廓 + EMA 平滑
            tracked = self.tracker.detect_and_track(frame)
            with self._bbox_lock:
                self._tracked_bbox = tracked
            
            # ---- 休眠模式：自动截图后暂停推理 ----
            if self.after_capture_sleep_until > time.time():
                continue
            
            # ---- 帧间差分：判断是否需要触发推理 ----
            # 这是创新点 3 的核心：在小核上运行轻量判断
            current_time = time.time()
            if tracked is not None:
                if current_time - self.diff_detector.last_trigger_time >= 0.05:
                    need_infer = True
                    self.diff_detector.last_trigger_time = current_time
                    self.diff_detector.prev_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                else:
                    need_infer = False
            else:
                need_infer = self.diff_detector.check(frame)
            
            # 强制检测（首帧 / 用户按 r 重置 / 重连后）
            if self._force_detect:
                need_infer = True
                self._force_detect = False
                
            # 模糊时强制跳过推理
            if self._focus_is_blurry:
                need_infer = False
            
            if need_infer:
                # ---- 提交帧到推理队列 ----
                # 如果队列已满（推理线程正忙），替换为最新帧
                try:
                    # 尝试清空旧帧
                    try:
                        self._inference_queue.get_nowait()
                    except queue.Empty:
                        pass
                    self._inference_queue.put_nowait(frame.copy())
                except queue.Full:
                    pass  # 极端情况下丢弃
        
        # ---- 线程退出，释放摄像头 ----
        if cap is not None:
            cap.release()
    
    # ================================================================
    #  推理线程（绑定到飞腾派 FTC664 大核）
    # ================================================================
    
    def _inference_worker(self):
        """
        【推理线程】运行在飞腾派 FTC664 大核上
        
        职责：
        1. 从推理队列获取待推理的帧
        2. 执行纯 NumPy 预处理（替代 PIL + torchvision）
        3. 运行 ONNX Runtime 推理（ARM NEON 加速）
        4. 将结果存入结果缓冲区
        
        创新点体现：
        - AI 推理独占大核，不受采集和显示任务干扰
        - ONNX Runtime 替代 PyTorch，利用 ARM NEON 向量化
        - INT8 量化模型利用 ARMv8 定点运算单元
        """
        # ---- 绑定到飞腾派大核 ----
        bound = self.scheduler.bind_to_big_cores()
        if bound:
            print(f"  📌 推理线程已绑定到大核 cpu{self.scheduler.big_cores}")
        
        while self.running:
            # ---- 从队列获取帧（阻塞等待，超时 100ms）----
            try:
                frame = self._inference_queue.get(timeout=0.1)
            except queue.Empty:
                continue
            
            # ---- 裁剪到跟踪框区域再推理（去除背景噪点）----
            # 关键优化：只将太阳能板区域送入分类器，
            # 避免背景中的其他物体干扰分类结果
            infer_frame = frame  # 默认用全帧
            with self._bbox_lock:
                bbox = self._tracked_bbox
            
            if bbox is not None:
                bx, by, bw, bh = bbox
                fh, fw = frame.shape[:2]
                # 加小量 margin 避免裁剪过紧
                margin = max(bw, bh) // 10
                x1 = max(0, bx - margin)
                y1 = max(0, by - margin)
                x2 = min(fw, bx + bw + margin)
                y2 = min(fh, by + bh + margin)
                crop = frame[y1:y2, x1:x2]
                if crop.size > 0 and crop.shape[0] > 5 and crop.shape[1] > 5:
                    infer_frame = crop
            
            # ---- 模糊检测已在采集线程进行，此处直接执行推理 ----
            
            # ---- 执行推理（预处理 + ONNX 推理 + Softmax）----
            class_name, confidence, infer_time = self.engine.detect(infer_frame)
            
            h, w = frame.shape[:2]
            
            # ---- 将结果存入缓冲区 ----
            result = {
                'class_name': class_name,
                'confidence': confidence,
                'bbox': (0, 0, w, h),
                'infer_time': infer_time,
                'timestamp': time.time()
            }
            
            with self._result_lock:
                self._latest_result = result
                self._result_counter += 1
    
    # ================================================================
    #  主线程（显示 + 控制 + 自动截图）
    # ================================================================
    
    def start(self, camera_id=0, output_dir="./recordings"):
        """
        启动加速版检测系统
        
        主线程负责：
        - 显示画面
        - 键盘交互（q退出 / s截图 / r重置）
        - 自动截图逻辑
        - 录像保存
        - 性能统计打印
        
        Args:
            camera_id: 摄像头设备号（默认 0）
            output_dir: 录像输出目录
        """
        global CENTER_X, CENTER_Y, FLAG
        if self.save_video and not os.path.exists(output_dir):

            os.makedirs(output_dir)
        
        # ---- 打印系统信息 ----
        print(f"\n{'=' * 60}")
        print(f"🤖 沙漠光伏板异常检测系统 — 飞腾派加速版")
        print(f"{'=' * 60}")
        
        # 打印飞腾派核心调度信息
        print(f"\n📊 飞腾派异构核心调度:")
        self.scheduler.print_info()
        
        # 尝试设置性能模式
        gov_ok = self.scheduler.set_performance_governor()
        if gov_ok:
            print(f"  ⚡ CPU 调频模式: performance (最高频率)")
        else:
            print(f"  ⚡ CPU 调频模式: 默认 (建议使用 sudo 运行以启用性能模式)")
        
        # 打印推理引擎信息
        model_info = self.engine.get_model_info()
        print(f"\n📦 推理引擎:")
        print(f"  引擎: ONNX Runtime (ARM NEON 优化)")
        print(f"  输入: {model_info['input_name']} {model_info['input_shape']}")
        print(f"  线程: {model_info['num_threads']} (绑定大核)")
        
        print(f"\n📹 摄像头配置:")
        print(f"  目标分辨率: {CAMERA_WIDTH}x{CAMERA_HEIGHT} @ {CAMERA_FPS}fps")
        
        print(f"\n🎯 检测策略:")
        print(f"  帧间差分阈值: {self.diff_detector.threshold}")
        print(f"  强制检测间隔: {self.diff_detector.force_interval}s")
        print(f"  置信度阈值: {self.confidence_threshold:.0%}")
        
        print(f"\n💾 录像: {'是' if self.save_video else '否'} (建议 --no_save)")
        print(f"⌨️  q退出 | s手动截图 | r重置")
        print(f"{'=' * 60}")
        
        # ---- 启动采集线程（小核）----
        self.running = True
        self.start_time = time.time()
        
        capture_thread = threading.Thread(
            target=self._capture_worker,
            args=(camera_id,),
            name="CaptureThread-SmallCore",
            daemon=True
        )
        capture_thread.start()
        
        # ---- 等待摄像头就绪 ----
        print(f"\n⏳ 等待摄像头就绪...")
        wait_start = time.time()
        while not self._camera_ok and time.time() - wait_start < 10:
            time.sleep(0.1)
        
        if not self._camera_ok:
            print("❌ 错误：无法打开摄像头（等待超时）")
            self.running = False
            return
        
        print(f"📹 摄像头就绪: {self._camera_width}x{self._camera_height}")
        
        # ---- 启动推理线程（大核）----
        inference_thread = threading.Thread(
            target=self._inference_worker,
            name="InferenceThread-BigCore",
            daemon=True
        )
        inference_thread.start()
        
        print(f"\n🚀 系统启动完成，开始检测...\n")
        
        # ---- 设置显示窗口 ----
        if not self.no_show:
            display_width = int(self._camera_width * WINDOW_SCALE)
            display_height = int(self._camera_height * WINDOW_SCALE)
            cv2.namedWindow('Solar Panel Detection [Phytium Accel]', cv2.WINDOW_NORMAL)
            cv2.resizeWindow('Solar Panel Detection [Phytium Accel]', display_width, display_height)
        
        # ---- 初始化录像 ----
        video_writer = None
        if self.save_video:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            video_path = os.path.join(output_dir, f"detection_accel_{timestamp}.mp4")
            video_writer = cv2.VideoWriter(
                video_path, cv2.VideoWriter_fourcc(*'mp4v'),
                CAMERA_FPS, (self._camera_width, self._camera_height)
            )
            print(f"💾 录像保存至: {video_path}\n")
        
        # ---- 初始化截图目录 ----
        script_dir = os.path.dirname(os.path.abspath(__file__))
        screenshot_dir = os.path.join(script_dir, "screenshots")
        if not os.path.exists(screenshot_dir):
            os.makedirs(screenshot_dir)
        
        show_counter = 0
        
        # ================================================================
        #  主循环
        # ================================================================
        try:
            while self.running:
                # ---- 获取最新帧 ----
                frame = None
                with self._frame_lock:
                    if self._latest_frame is not None:
                        frame = self._latest_frame.copy()
                
                if frame is None:
                    time.sleep(0.01)
                    continue
                
                current_time = time.time()
                
                # ---- 暂停检测状态自动恢复判定 ----
                if self.FLAG == 1:
                    if current_time >= self.after_capture_sleep_until:
                        self.FLAG = 0
                        FLAG = 0
                        self._force_detect = True
                        self.diff_detector.force_next()
                        self.tracker.reset()
                        print("\n▶️ 冷却期已满，已将 FLAG 设置为 0，重启检测")
                
                # ---- 获取并更新实时跟踪框与几何中心 ----
                with self._bbox_lock:
                    live_bbox = self._tracked_bbox
                
                self.last_detection['bbox'] = live_bbox
                
                # 追踪框已缓存，供后续中心偏离计算使用
                
                global CENTER_X, CENTER_Y
                if live_bbox is not None and self.FLAG == 0:
                    x, y, w, h = live_bbox
                    center_px_x = x + w // 2
                    center_px_y = y + h // 2
                    center_x, center_y = self._pixel_to_center_coords(center_px_x, center_px_y)
                    CENTER_X = center_x
                    CENTER_Y = center_y
                    self.CENTER_X = center_x
                    self.CENTER_Y = center_y
                else:
                    CENTER_X = 0
                    CENTER_Y = 0
                    self.CENTER_X = 0
                    self.CENTER_Y = 0
                
                # ---- 检查是否有新的推理结果 ----
                with self._result_lock:
                    if self._result_counter > self._processed_counter:
                        result = self._latest_result
                        self._processed_counter = self._result_counter
                    else:
                        result = None
                
                # ---- 处理新的推理结果 ----
                if result is not None and self.FLAG == 0:
                    if live_bbox is None:
                        self.prediction_history.clear()
                        class_name = None
                        confidence = 0.0
                        self.last_detection['class_name'] = None
                        self.last_detection['confidence'] = 0.0
                    else:
                        raw_class_name = result['class_name']
                        raw_confidence = result['confidence']
                        infer_time = result['infer_time']
                        
                        # 将单帧预测结果推入历史队列进行投票
                        if raw_confidence >= self.confidence_threshold:
                            self.prediction_history.append((raw_class_name, raw_confidence))
                        else:
                            # 低于置信度阈值推入 None，代表没有检测到有效的太阳能板
                            self.prediction_history.append((None, raw_confidence))
                            
                        if len(self.prediction_history) > self.history_size:
                            self.prediction_history.pop(0)
                        
                        # 滑动窗口多数投票，过滤瞬时噪点和错检
                        votes = {}
                        for c_name, conf in self.prediction_history:
                            votes[c_name] = votes.get(c_name, 0) + 1
                        
                        majority_class = max(votes, key=votes.get)
                        majority_votes = votes[majority_class]
                        
                        # 只有当非 None 类别的投票数达到多数（5帧中至少3帧），才更新状态为检测到太阳能板
                        if majority_class is not None and majority_votes >= (self.history_size // 2 + 1):
                            matched_confs = [conf for c_name, conf in self.prediction_history if c_name == majority_class]
                            class_name = majority_class
                            confidence = sum(matched_confs) / len(matched_confs)
                        else:
                            # 没有达到多数共识，或者多数票为 None (表示视野中没有太阳能板)
                            class_name = None
                            confidence = 0.0
                        
                        self.last_detection['class_name'] = class_name
                        self.last_detection['confidence'] = confidence
                    
                    if class_name is not None:
                        self.last_detection['timestamp'] = current_time
                        self.detection_count += 1
                        
                        # ---- IPC: 分类变化时更新异常计数 ----
                        if class_name != self._prev_ipc_class:
                            self._prev_ipc_class = class_name
                            if class_name in OCCLUSION_CLASSES:
                                self._occlusion_count += 1
                            elif class_name in DAMAGE_CLASSES:
                                self._damage_count += 1
                        
                        # 获取当前帧对应的中心坐标，用于立即打印
                        if live_bbox is not None:
                            x, y, w, h = live_bbox
                            center_px_x = x + w // 2
                            center_px_y = y + h // 2
                            center_x, center_y = self._pixel_to_center_coords(center_px_x, center_px_y)
                        else:
                            center_x, center_y = 0, 0
                        
                        # 重置稳定计数
                        self.center_stable_count = 0
                        
                        # 打印检测结果
                        if current_time - self.last_print_time >= PRINT_INTERVAL:
                            self.last_print_time = current_time
                            elapsed = current_time - self.start_time
                            print(f"[{elapsed:5.1f}s] {class_name:12} | "
                                  f"置信度:{confidence:.1%} | "
                                  f"X={center_x:d}, Y={center_y:d}")
                        else:
                            status = (f"最近检测: {class_name} | "
                                      f"置信度:{confidence:.1%} | "
                                      f"X={center_x:d}, Y={center_y:d} | "
                                      f"次数:{self.detection_count}")
                            print(f"\r{status}", end="", flush=True)
                
                # ---- 串口控制与状态机更新 ----
                should_capture = False
                if self.FLAG == 0 and self._camera_ok:
                    has_solar_panel = (self.last_detection['bbox'] is not None and 
                                       self.last_detection['class_name'] is not None)
                    
                    if has_solar_panel:
                        x, y, w, h = self.last_detection['bbox']
                        center_px_x = x + w // 2
                        center_px_y = y + h // 2
                        center_x, center_y = self._pixel_to_center_coords(center_px_x, center_px_y)
                        
                        # 更新全局中心坐标
                        self.CENTER_X = center_x
                        self.CENTER_Y = center_y
                        CENTER_X = center_x
                        CENTER_Y = center_y
                        
                        box_area = w * h
                        should_capture = self.rover.update(True, center_x, center_y, box_area)
                        
                        # 中心坐标周期打印
                        if current_time - self.last_center_print_time >= CENTER_PRINT_INTERVAL:
                            self.last_center_print_time = current_time
                            elapsed = current_time - self.start_time
                            print(f"[{elapsed:5.1f}s] X={center_x:d}, Y={center_y:d} | 类别: {self.last_detection['class_name']}")
                    else:
                        self.CENTER_X = 0
                        self.CENTER_Y = 0
                        CENTER_X = 0
                        CENTER_Y = 0
                        should_capture = self.rover.update(False, 0, 0, 0)

                # ---- 任务完成自动退出判断 ----
                if hasattr(self.rover, 'state') and self.rover.state == "FINISHED":
                    print("\n🎉 [系统提示] 所有逻辑执行完毕，准备自动结束进程...")
                    self.running = False
                    break

                # ---- 自动截图 (在 FLAG=0 且收到 capture 信号时，不受当前帧是否检测到限制) ----
                if should_capture and self.FLAG == 0 and not self.track_only:
                    frame_with_box = frame.copy()
                    
                    # 尽可能绘制边框
                    bbox = self.last_detection.get('bbox')
                    class_name_str = self.last_detection.get('class_name') or "SolarPanel"
                    if bbox is not None:
                        bx, by, bw, bh = bbox
                        cv2.rectangle(frame_with_box, (bx, by), (bx + bw, by + bh), (0, 255, 0), 2)
                        cv2.putText(
                            frame_with_box,
                            class_name_str,
                            (bx + 2, by - 5),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2
                        )
                    else:
                        # 如果没有当前框，则在画面中心绘制提示
                        cv2.putText(
                            frame_with_box,
                            f"Captured: {class_name_str}",
                            (50, 50),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2
                        )
                    
                    status_map = {"Clean": "Normal"}
                    status = status_map.get(class_name_str, class_name_str)
                    
                    auto_ts = datetime.now()
                    ts_str = auto_ts.strftime("%Y-%m-%d_%H-%M-%S")
                    filename = f"{ts_str}_{status}.jpg"
                    auto_path = os.path.join(screenshot_dir, filename)
                    self.panel_id += 1
                    
                    # 定义安全的保存方法并打印绝对路径
                    def safe_save_image(path, img):
                        try:
                            success = cv2.imwrite(path, img)
                            if success:
                                print(f"\n📸 [Success] 自动截图已成功保存到: {os.path.abspath(path)}")
                            else:
                                print(f"\n❌ [Error] 自动截图保存失败 (cv2.imwrite 返回 False)，路径: {os.path.abspath(path)}")
                        except Exception as ex:
                            print(f"\n❌ [Exception] 自动截图保存发生异常: {ex}")

                    save_thread = threading.Thread(
                        target=safe_save_image,
                        args=(auto_path, frame_with_box),
                        daemon=True
                    )
                    save_thread.start()
                    
                    # 根据下位机控制器的状态决定拍照后冷却/休眠时间
                    if self.rover.state == "UTURN":
                        print(f"⏸️ 正在执行转弯，冷却检测 7.85 秒...")
                        self.after_capture_sleep_until = time.time() + 7.85
                    elif self.rover.state == "FINISHED":
                        print(f"⏸️ 任务已完成，永久关闭检测...")
                        self.after_capture_sleep_until = time.time() + 999999.0
                    else:
                        print(f"⏸️ 进入规避冷却 5 秒...")
                        self.after_capture_sleep_until = time.time() + 5.0
                    
                    self.FLAG = 1
                    FLAG = 1
                    
                    self.last_detection = {
                        'bbox': None, 'class_name': None,
                        'confidence': 0.0, 'timestamp': 0
                    }
                    with self._bbox_lock:
                        self._tracked_bbox = None



                
                # ---- 性能统计周期打印 (已取消) ----
                
                # ---- 在画面上绘制检测框和标注信息 ----
                display_frame = self._draw_overlay(frame)
                
                # ---- IPC: 写出检测数据和标注帧给 Qt UI ----
                self._write_ipc_output(display_frame, current_time)
                
                # ---- 显示画面（每 3 帧一次，降低显示开销）----
                if not self.no_show:
                    show_counter += 1
                    if show_counter % 3 == 0:
                        cv2.imshow('Solar Panel Detection [Phytium Accel]', display_frame)
                
                # ---- 录像 ----
                if video_writer:
                    video_writer.write(frame)
                
                # ---- 键盘交互 ----
                if not self.no_show:
                    key = cv2.waitKey(1)
                    if key == ord('q') or key == ord('Q') or key == 27:
                        break
                    elif key == ord('s') or key == ord('S'):
                        self.screenshot_count += 1
                        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
                        path = os.path.join(
                            screenshot_dir,
                            f"manual_{ts}_{self.screenshot_count}.jpg"
                        )
                        cv2.imwrite(path, frame)
                        print(f"\n📸 手动截图已保存: {path}")
                    elif key == ord('r') or key == ord('R'):
                        self.last_detection = {
                            'bbox': None, 'class_name': None,
                            'confidence': 0.0, 'timestamp': 0
                        }
                        self.center_stable_count = 0
                        self._force_detect = True
                        self.diff_detector.force_next()
                        self.tracker.reset()
                        print("\n🔄 已重置检测状态")
                else:
                    # 无窗口模式下，主线程稍微睡眠避免 CPU 空转
                    time.sleep(0.01)
        
        except KeyboardInterrupt:
            print("\n\n⌨️ 用户中断")
        
        finally:
            # ---- 清理资源 ----
            self.running = False
            
            # ---- 性能统计打印已取消 ----
            
            # 等待线程退出
            capture_thread.join(timeout=3)
            inference_thread.join(timeout=3)
            
            if video_writer:
                video_writer.release()
            cv2.destroyAllWindows()
            
            print("✅ 系统已安全退出")
    
    # ================================================================
    #  IPC 输出（与 Qt UI 共享数据）
    # ================================================================
    
    def _write_ipc_output(self, display_frame, current_time):
        """
        将检测结果和标注帧写入 /tmp 文件，供 Qt UI 轮询读取。
        
        使用原子写入（先写临时文件再 rename）防止 UI 读到不完整数据。
        写入间隔由 IPC_WRITE_INTERVAL 控制（默认 100ms），避免过频磁盘 IO。
        
        Args:
            display_frame: 已绘制检测框和标注信息的帧 (BGR numpy array)
            current_time: 当前时间戳（秒）
        """
        # ---- 间隔控制：避免过频 IO ----
        if current_time - self._last_ipc_write_time < IPC_WRITE_INTERVAL:
            return
        self._last_ipc_write_time = current_time
        
        # ---- 计算实时 FPS ----
        elapsed = current_time - self.start_time if self.start_time else 0
        fps = self.frame_count / elapsed if elapsed > 1.0 else 0.0
        
        # ---- 组装 JSON 数据 ----
        ipc_data = {
            "timestamp": current_time,
            "class_name": self.last_detection.get('class_name') or "",
            "confidence": self.last_detection.get('confidence', 0.0),
            "center_x": self.CENTER_X,
            "center_y": self.CENTER_Y,
            "flag": self.FLAG,
            "fps": round(fps, 1),
            "frame_count": self.frame_count,
            "detection_count": self.detection_count,
            "occlusion_count": self._occlusion_count,
            "damage_count": self._damage_count,
            "infer_time_ms": round(self.engine.get_avg_infer_time(), 1),
            "camera_ok": self._camera_ok,
            "uptime_seconds": int(elapsed)
        }
        
        # ---- 添加 bbox 信息 ----
        bbox = self.last_detection.get('bbox')
        if bbox is not None:
            ipc_data["bbox"] = list(bbox)
        else:
            ipc_data["bbox"] = []
        
        # ---- 原子写入 JSON ----
        try:
            json_str = json.dumps(ipc_data, ensure_ascii=False)
            tmp_json = IPC_JSON_PATH + ".tmp"
            with open(tmp_json, 'w') as f:
                f.write(json_str)
            os.replace(tmp_json, IPC_JSON_PATH)
        except Exception:
            pass  # 写入失败不影响主流程
        
        # ---- 原子写入标注帧 BMP ----
        try:
            ret, buf = cv2.imencode('.bmp', display_frame)
            if ret:
                tmp_frame = IPC_FRAME_PATH + ".tmp"
                with open(tmp_frame, 'wb') as f:
                    f.write(buf.tobytes())
                os.replace(tmp_frame, IPC_FRAME_PATH)
        except Exception:
            pass  # 写入失败不影响主流程
    
    def _draw_overlay(self, frame):
        """
        在画面上绘制检测框、分类标签和性能信息
        
        绘制内容：
        - 绿色矩形框标记检测区域
        - 左上角显示分类名称和置信度
        - 右上角显示实时 FPS 和推理耗时
        - 十字线标记画面中心
        
        Args:
            frame: 原始帧 (BGR numpy array)
        
        Returns:
            绘制后的帧（副本，不修改原帧）
        """
        display = frame.copy()
        h, w = display.shape[:2]
        
        # ---- 绘制画面中心十字线（辅助对准）----
        cx, cy = w // 2, h // 2
        cross_color = (0, 255, 255)  # 黄色
        cv2.line(display, (cx - 8, cy), (cx + 8, cy), cross_color, 1)
        cv2.line(display, (cx, cy - 8), (cx, cy + 8), cross_color, 1)
        
        # ---- 绘制跟踪框和标注信息（完全基于实时 live_bbox，流畅无抖动）----
        with self._bbox_lock:
            live_bbox = self._tracked_bbox
        
        if live_bbox is not None:
            x, y, bw, bh = live_bbox
            class_name = self.last_detection['class_name']
            confidence = self.last_detection['confidence']
            
            if class_name is None:
                # 有跟踪框但尚未分类：用白色虚线/细线显示跟踪中
                cv2.rectangle(display, (x, y), (x + bw, y + bh), (200, 200, 200), 1)
                cv2.putText(display, "Tracking...", (x + 2, y - 4),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.35, (200, 200, 200), 1)
            else:
                # 根据分类结果选择颜色
                if class_name == 'Clean':
                    box_color = (0, 255, 0)      # 绿色 - 正常
                else:
                    box_color = (0, 0, 255)      # 红色 - 异常
                
                # 绘制检测边框（带圆角效果：4 个角画短线段）
                thickness = 2
                corner_len = min(bw, bh) // 4  # 角线长度
                # 左上角
                cv2.line(display, (x, y), (x + corner_len, y), box_color, thickness)
                cv2.line(display, (x, y), (x, y + corner_len), box_color, thickness)
                # 右上角
                cv2.line(display, (x + bw, y), (x + bw - corner_len, y), box_color, thickness)
                cv2.line(display, (x + bw, y), (x + bw, y + corner_len), box_color, thickness)
                # 左下角
                cv2.line(display, (x, y + bh), (x + corner_len, y + bh), box_color, thickness)
                cv2.line(display, (x, y + bh), (x, y + bh - corner_len), box_color, thickness)
                # 右下角
                cv2.line(display, (x + bw, y + bh), (x + bw - corner_len, y + bh), box_color, thickness)
                cv2.line(display, (x + bw, y + bh), (x + bw, y + bh - corner_len), box_color, thickness)
                
                # 绘制分类标签背景条
                label = f"{class_name} {confidence:.0%}"
                font = cv2.FONT_HERSHEY_SIMPLEX
                font_scale = 0.45
                (tw, th), _ = cv2.getTextSize(label, font, font_scale, 1)
                cv2.rectangle(display, (x, y - th - 6), (x + tw + 4, y), box_color, -1)
                cv2.putText(display, label, (x + 2, y - 4), font, font_scale, (255, 255, 255), 1)
                
                pass
        
        # ---- 性能指标屏幕显示已取消 ----
        
        return display
    
    def _print_performance_stats(self):
        """打印性能统计信息"""
        elapsed = time.time() - self.start_time if self.start_time else 0
        fps = self.frame_count / elapsed if elapsed > 0 else 0
        avg_infer = self.engine.get_avg_infer_time()
        diff_stats = self.diff_detector.get_stats()
        
        print(f"\n  ┌──────────── 性能统计 ────────────┐")
        print(f"  │ 运行时间:   {elapsed:>8.1f} s           │")
        print(f"  │ 采集帧率:   {fps:>8.1f} FPS         │")
        print(f"  │ 平均推理:   {avg_infer:>8.1f} ms          │")
        print(f"  │ 推理次数:   {diff_stats['total_triggers']:>8d}            │")
        print(f"  │ 跳过率:     {diff_stats['skip_ratio']:>7.1%}             │")
        print(f"  │ 强制推理:   {diff_stats['total_forced']:>8d}            │")
        print(f"  │ 检测次数:   {self.detection_count:>8d}            │")
        print(f"  └──────────────────────────────────┘")


# ================================================================
#  主函数
# ================================================================

def main():
    parser = argparse.ArgumentParser(
        description='飞腾派加速版 — 沙漠光伏板异常检测系统',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 基础运行（推荐加 --no_save 提升性能）
  python3 realtime_detector_accel.py --model solar_panel_int8.onnx --no_save

  # 指定摄像头和大小核
  python3 realtime_detector_accel.py --model solar_panel_int8.onnx \\
      --camera 0 --big_cores 0,1 --small_cores 2,3 --no_save

  # 调整帧间差分灵敏度
  python3 realtime_detector_accel.py --model solar_panel_int8.onnx \\
      --diff_threshold 3.0 --force_interval 2.0 --no_save
        """
    )
    
    # ---- 基础参数（与原版兼容）----
    parser.add_argument(
        '--model', type=str, default="solar_panel_int8.onnx",
        help='ONNX 模型文件路径（如 solar_panel_int8.onnx）'
    )
    parser.add_argument(
        '--camera', type=int, default=0,
        help='摄像头设备号（默认 0）'
    )
    parser.add_argument(
        '--no_save', action='store_true',
        help='不保存录像（推荐，可提升性能）'
    )
    parser.add_argument(
        '--no_show', action='store_true',
        help='不打开 OpenCV 独立显示窗口'
    )
    parser.add_argument(
        '--track_only', action='store_true',
        help='仅进行目标跟踪，不进行拍照截图和冷却'
    )

    
    # ---- 摄像头参数 ----
    parser.add_argument(
        '--width', type=int, default=CAMERA_WIDTH,
        help=f'摄像头采集宽度（默认 {CAMERA_WIDTH}，越高检测越准但越占算力）'
    )
    parser.add_argument(
        '--height', type=int, default=CAMERA_HEIGHT,
        help=f'摄像头采集高度（默认 {CAMERA_HEIGHT}）'
    )
    
    # ---- 飞腾派架构参数 ----
    parser.add_argument(
        '--big_cores', type=str, default=None,
        help='大核 CPU 编号，逗号分隔（如 0,1）。默认自动检测。'
    )
    parser.add_argument(
        '--small_cores', type=str, default=None,
        help='小核 CPU 编号，逗号分隔（如 2,3）。默认自动检测。'
    )
    parser.add_argument(
        '--infer_threads', type=int, default=2,
        help='ONNX Runtime 推理线程数（建议等于大核数，默认 2）'
    )
    
    # ---- 检测参数 ----
    parser.add_argument(
        '--confidence', type=float, default=CONFIDENCE_THRESHOLD,
        help=f'分类置信度阈值（默认 {CONFIDENCE_THRESHOLD}，低于此值的检测结果被忽略）'
    )
    parser.add_argument(
        '--min_laplacian_var', type=float, default=MIN_LAPLACIAN_VAR,
        help=f'最小拉普拉斯方差阈值（默认 {MIN_LAPLACIAN_VAR}，低于此值认为模糊/对焦中，跳过推理以防止错检；设为0则禁用）'
    )
    
    # ---- 跟踪参数 ----
    parser.add_argument(
        '--smooth_alpha', type=float, default=0.5,
        help='跟踪框平滑系数（0~1，越大越跟手，越小越平滑，默认 0.5）'
    )
    parser.add_argument(
        '--min_area', type=float, default=0.05,
        help='最小检测面积占比（0~1，过滤小轮廓，默认 0.05）'
    )
    
    # ---- 帧间差分参数 ----
    parser.add_argument(
        '--diff_threshold', type=float, default=FRAME_DIFF_THRESHOLD,
        help=f'帧间差分阈值（默认 {FRAME_DIFF_THRESHOLD}，越小越灵敏）'
    )
    parser.add_argument(
        '--force_interval', type=float, default=FORCE_DETECT_INTERVAL,
        help=f'强制检测间隔秒数（默认 {FORCE_DETECT_INTERVAL}）'
    )
    
    args = parser.parse_args()
    
    # ---- 解析大小核参数 ----
    big_cores = None
    small_cores = None
    if args.big_cores:
        big_cores = [int(c.strip()) for c in args.big_cores.split(',')]
    if args.small_cores:
        small_cores = [int(c.strip()) for c in args.small_cores.split(',')]
    
    # ---- 检查模型文件 ----
    if not os.path.exists(args.model):
        print(f"❌ 错误: 模型文件不存在: {args.model}")
        print(f"   请先运行 export_onnx.py 导出 ONNX 模型")
        return
    
    model_size = os.path.getsize(args.model) / (1024 * 1024)
    print(f"\n🚀 飞腾派加速版启动")
    print(f"   模型: {args.model} ({model_size:.1f} MB)")
    
    # ---- 初始化三大核心组件 ----
    
    # 组件 1: 飞腾派大小核调度器
    scheduler = PhytiumCoreScheduler(
        big_cores=big_cores,
        small_cores=small_cores
    )
    
    # 组件 2: 帧间差分检测器
    diff_detector = FrameDiffDetector(
        threshold=args.diff_threshold,
        min_interval=FRAME_DIFF_MIN_INTERVAL,
        force_interval=args.force_interval
    )
    
    # 组件 3: ONNX 推理引擎
    engine = ONNXInferenceEngine(
        model_path=args.model,
        input_size=MODEL_INPUT_SIZE,
        num_threads=args.infer_threads
    )
    
    # 组件 4: 太阳能板轮廓跟踪器（替代光流法，耗时 <1ms）
    tracker = SolarPanelTracker(
        min_area_ratio=args.min_area,
        smooth_alpha=args.smooth_alpha
    )
    
    # ---- 创建并启动检测器 ----
    detector = AccelSolarPanelDetector(
        engine=engine,
        scheduler=scheduler,
        diff_detector=diff_detector,
        tracker=tracker,
        save_video=not args.no_save,
        confidence_threshold=args.confidence,
        camera_width=args.width,
        camera_height=args.height,
        min_laplacian_var=args.min_laplacian_var,
        no_show=args.no_show,
        track_only=args.track_only
    )

    
    detector.start(camera_id=args.camera)


if __name__ == '__main__':
    main()

