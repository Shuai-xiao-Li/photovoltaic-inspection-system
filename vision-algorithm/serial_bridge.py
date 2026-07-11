#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@File: serial_bridge.py
@Brief: 飞腾派与底盘STM32下位机的串口命令通信接口，包含运动包构建、校验位计算及发送控制指令。
@Author: 李帅 赵禹博 吴坨鑫
@Date: 6月12号
@Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
"""

"""
serial_bridge.py — 飞腾派与STM32底盘的串口通信桥接模块 (v4 - 右侧单向循迹与3s冷却避锁)

本版本专门优化以下逻辑：
  1. 太阳能板单侧摆放：开机回正后云台自动偏向右侧 45° 并直行搜索。
  2. 发现目标立即刹车：直行中一旦识别到太阳能板，底盘立刻制动停车，开始 PI 追踪对准。
  3. 拍照微调与静态识别：云台稳定居中后执行 4 点式微调扫视，之后静态驻留 3s 供 AI 稳定截图。
  4. 3s 冷却直行跳板：截图后小车立即恢复直走，并完全关闭识别 3s，云台回到右侧 45°，防止二次触发。
"""

import serial
import time
import glob
import os
import threading
import queue

class SmartRoverController:
    def __init__(self, baudrate=115200, track_only=False, camera_width=640, camera_height=480, status_cb=None):
        self.baudrate = baudrate
        self.track_only = track_only
        self.camera_width = camera_width
        self.camera_height = camera_height
        
        self.ser = None
        self.state = "SEARCHING"

        # 冷却与防抖时间戳
        self.cooldown_end_time = 0
        self.stabilize_end_time = 0
        
        # 搜索与对准的目标绝对角度
        self.search_yaw = -45.0      # 太阳能板在右侧摆放，第一行开机转动到右侧 -45 度监控 (负值为右, 正值为左)
        
        # 巡检状态与计数变量
        self.inspected_count = 0     # 累计巡检完成的板子计数
        self.uturn_start_time = 0.0  # U-turn 开始的时间戳
        self.uturn_gimbal_turned = False # U-turn 过程中云台是否已提前偏转
        self.search_tilt = -10.0     # 垂直方向倾角 (-10度)

        # 发送频率限速
        self.last_send_time = 0
        self.min_send_interval = 1.0 / 15.0  # 最大发送频率 15Hz

        # 底盘速度控制限频
        self.last_vel_send_time = 0
        self.vel_send_interval = 0.1  # 底盘指令发送间隔 100ms

        # ---- 绝对角度与目标角度 (数字孪生) ----
        self.s1_angle = 0.0          # Python 估计的水平绝对角度 (范围: -83 ~ 89)
        self.s2_angle = 0.0          # Python 估计的垂直绝对角度 (范围: -25 ~ 0)
        
        self.s1_target_angle = 0.0   # 期望的绝对角度目标值
        self.s2_target_angle = 0.0   # 期望的绝对角度目标值

        # PI 参数 (使用视场角变换后的角度比例，alpha 为更新阻尼系数)
        self.alpha_x = 0.35          # 水平阻尼系数 (每次更新仅走剩余偏差的 35%)
        self.alpha_y = 0.35          # 垂直阻尼系数
        
        # 异步串口发送队列
        self.cmd_queue = queue.Queue(maxsize=10)

        # 自适应控制状态和历史偏差记录 (无扰切换与微分阻尼)
        self.last_state = "SEARCHING"
        self.last_delta_angle_x = 0.0
        self.last_delta_angle_y = 0.0

        # 目标丢失容忍计数器 (连续 N 帧丢失才切回 SEARCHING，防止闪烁丢锁)
        self.target_lost_count = 0
        self.target_lost_tolerance = 30  # 容忍连续 30 帧未检测到目标
        
        # 坐标稳定持续时间要求（秒）
        self.stable_required_duration = 0.2
        self._stable_since = None
        self._tracking_start_time = None
        
        # 舵机运动方向反向标志
        self.invert_x = True         # 向左为正，向右为负。画面右侧为正时需要向右看(负方向)，所以必须反转
        self.invert_y = True         # 根据物理连线：向下为正，向上为负。画面上方为正时需要向上看(负方向)，所以必须反转
        
        # 视场角估算值 (度)
        self.fov_x = 55.0
        self.fov_y = 43.0

        # 允许对准的死区
        self.center_zone = 60        # 像素偏离低于此值认为已对准，触发拍照流程

        # ---- 4-Point Micro-Scan Calibration (R1) ----
        self.calib_step = 0
        self.calib_timer = 0.0
        self.calib_base_s1 = 0.0
        self.calib_base_s2 = 0.0
        self.calib_best_s1 = 0.0
        self.calib_best_s2 = 0.0
        self.calib_areas = {}
        self.calib_step_delay = 0.8

        # 尝试连接
        self.connect()
        
        # 延时等待 3.5 秒以确保下位机 (STM32 DMP 陀螺仪姿态解算和屏幕 UI) 完全初始化就绪，串口中断就位
        print("[SerialBridge] 正在等待下位机 (STM32 DMP陀螺仪) 启动就绪 (等待3.5秒)...")
        if status_cb:
            status_cb("系统加载中: 等待下位机启动...", progress=20)
        time.sleep(3.5)
        
        # 启动时强制归中，并等待 0.5 秒以确保舵机物理复位到中心，防止因角度溢出导致机械卡死
        if self.ser:
            if status_cb:
                status_cb("系统加载中: 云台复位归中...", progress=50)
            self.send_command("scenter\r\n")
            time.sleep(0.5)
            
            # --- 启动自检扫视：左侧 45° -> 右侧 45° 扫视 ---
            print("[SerialBridge] 执行启动扫视自检: 左转45°...")
            if status_cb:
                status_cb("系统自检中: 启动云台自检扫描...", progress=70)
            # 左转45，垂直-10 (45 is Left in STM32 coordinate)
            self.send_command("sdelta 45 -10\r\n")
            self.s1_angle = 45.0
            self.s2_angle = -10.0
            time.sleep(1.0)
            
            print("[SerialBridge] 执行启动扫视自检: 扫视至右转45°...")
            if status_cb:
                status_cb("系统自检中: 云台自检扫描中...", progress=90)
            # 从左45扫动到右45 (-45 is Right in STM32 coordinate, delta is -90)
            self.send_command("sdelta -90 0\r\n")
            self.s1_angle = -45.0
            self.s2_angle = -10.0
            time.sleep(1.5)
            
            print("[SerialBridge] 自检扫视完毕，已锁定右侧45°，准备直行检索。")
            if status_cb:
                status_cb("加载完成: 开始直行检索", progress=100)
            self.s1_target_angle = -45.0
            self.s2_target_angle = -10.0

        # 启动异步串口发送 worker 线程
        self.worker_thread = threading.Thread(target=self._serial_worker, daemon=True)
        self.worker_thread.start()

    def connect(self):
        """自动扫描并连接USB串口，使用握手协议排除非底盘串口（如海康摄像头）"""
        import serial.tools.list_ports
        ports_list = list(serial.tools.list_ports.comports())
        
        sorted_ports = []
        for p in ports_list:
            sorted_ports.append(p.device)
            
        if not sorted_ports:
            sorted_ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
            
        if not sorted_ports:
            print("[SerialBridge] 未找到任何物理串口设备。请检查 CH340 是否已连接。")
            return False
            
        print(f"[SerialBridge] 扫描到可用串口列表: {sorted_ports}")
        
        for port in sorted_ports:
            if "video" in port:
                continue
            try:
                print(f"[SerialBridge] 正在尝试连接 {port}，波特率 {self.baudrate}...")
                ser = serial.Serial(port, self.baudrate, timeout=0.3)
                
                ser.reset_input_buffer()
                ser.write(b"\r\nhelp\r\n")
                time.sleep(0.15)
                
                if ser.in_waiting > 0:
                    resp = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                    if any(kw in resp.lower() for kw in ["stm32", "chassis", "ok", "err", "ready", "test", "status"]):
                        print(f"[SerialBridge] 成功连接到 {port}，握手成功！设备特征匹配。")
                        self.ser = ser
                        self.ser.write(b"scenter\r\n")
                        self.s1_angle = 0.0
                        self.s2_angle = 0.0
                        self.s1_target_angle = 0.0
                        self.s2_target_angle = 0.0
                        return True
                else:
                    ser.write(b"status\r\n")
                    time.sleep(0.15)
                    if ser.in_waiting > 0:
                        resp = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                        if any(kw in resp.lower() for kw in ["mode=", "ok", "err"]):
                            print(f"[SerialBridge] 成功连接到 {port}，握手成功！设备特征匹配。")
                            self.ser = ser
                            self.ser.write(b"scenter\r\n")
                            self.s1_angle = 0.0
                            self.s2_angle = 0.0
                            self.s1_target_angle = 0.0
                            self.s2_target_angle = 0.0
                            return True
                            
                if len(sorted_ports) == 1 or "USB" in port:
                    print(f"[SerialBridge] 端口 {port} 握手超时，但检测到其为唯一或USB类型设备，启动容错连接...")
                    self.ser = ser
                    self.ser.write(b"scenter\r\n")
                    self.s1_angle = 0.0
                    self.s2_angle = 0.0
                    self.s1_target_angle = 0.0
                    self.s2_target_angle = 0.0
                    return True
                
                ser.close()
                
            except Exception as e:
                print(f"[SerialBridge] 打开 {port} 失败: {e}")
                
        return False

    def _serial_worker(self):
        """后台异步发送串口指令的线程，确保带有延时而不阻塞主进程"""
        while True:
            cmd = self.cmd_queue.get()
            if cmd is None:
                break
            
            # 断线重连检测机制 (解决 U型弯大电流掉线导致的 Timeout fault)
            if not self.ser or not self.ser.is_open:
                print("[SerialBridge] ⚠️ 检测到串口断线！正在尝试重新连接恢复控制...")
                if not self.connect():
                    time.sleep(0.5)  # 连不上就稍微等一下再试
                    self.cmd_queue.task_done()
                    continue
                else:
                    print("[SerialBridge] ✅ 串口重连成功！恢复指令发送。")

            if self.ser and self.ser.is_open:
                try:
                    self.ser.write(cmd)
                    time.sleep(0.015)  # 发送后延时 15ms，避免STM32接收过载
                    if self.ser.in_waiting > 0:
                        resp = self.ser.read(self.ser.in_waiting).decode('ascii', errors='ignore').strip()
                        if resp and "ERR" in resp:
                            print(f"[STM32报错] {resp}")
                except Exception as e:
                    print(f"[SerialBridge] ❌ 串口发送异常(掉线了): {e}")
                    try:
                        self.ser.close()
                    except:
                        pass
                    self.ser = None
            self.cmd_queue.task_done()

    def send_command(self, cmd):
        """发送ASCII指令到STM32 (非阻塞方式)"""
        if not self.ser:
            if not self.connect():
                return
        try:
            sub_cmds = [c.strip() for c in cmd.split('\n') if c.strip()]
            for sub_cmd in sub_cmds:
                full_cmd = sub_cmd + "\r\n"
                if self.cmd_queue.full():
                    try:
                        self.cmd_queue.get_nowait()
                    except queue.Empty:
                        pass
                self.cmd_queue.put_nowait(full_cmd.encode('ascii'))
        except Exception as e:
            print(f"[SerialBridge] 放入指令队列异常: {e}")

    def _move_to_absolute(self, target_s1, target_s2):
        target_s1 = max(-83.0, min(89.0, target_s1))
        target_s2 = max(-25.0, min(0.0, target_s2))
        step_x = int(round(target_s1 - self.s1_angle))
        step_y = int(round(target_s2 - self.s2_angle))
        if step_x != 0 or step_y != 0:
            self.send_command(f"sdelta {step_x} {step_y}\r\n")
            self.s1_angle = max(-83.0, min(89.0, self.s1_angle + step_x))
            self.s2_angle = max(-25.0, min(0.0, self.s2_angle + step_y))
            self.s1_target_angle = self.s1_angle
            self.s2_target_angle = self.s2_angle

    def update(self, has_target, center_x, center_y, box_area):
        """
        核心状态机更新函数 (取消面积检测，右侧直走、锁定、3s冷却直行循环)
        
        Args:
            has_target: 是否检测到太阳能板
            center_x: 目标几何中心水平偏离
            center_y: 目标几何中心垂直偏离
            box_area: 检测框的像素面积
            
        Returns:
            bool: 是否应当触发拍照截图
        """
        current_time = time.time()

        # 串口控制限速 15Hz
        if current_time - self.last_send_time < self.min_send_interval:
            return False
        self.last_send_time = current_time

        # ---- 状态机逻辑 ----

        # 6. 最终冲刺状态（FINAL_RUN）
        if self.state == "FINAL_RUN":
            if current_time - self.last_vel_send_time >= self.vel_send_interval:
                self.send_command("vel 180 0\r\n")
                self.last_vel_send_time = current_time
                
            if current_time > self.final_run_end_time:
                print(f"[SerialBridge] 5 秒直行完毕！任务彻底完成，底盘刹车。")
                self.state = "FINISHED"
                self.send_command("vel 0 0\r\n")
                
            return False

        # 7. 任务完成状态（FINISHED）
        if self.state == "FINISHED":
            if current_time - self.last_vel_send_time >= self.vel_send_interval:
                self.send_command("vel 0 0\r\n")
                self.last_vel_send_time = current_time
            return False

        # 0.5. U-turn 转弯状态 (UTURN)
        if self.state == "UTURN":
            elapsed = current_time - self.state_start_time
            if elapsed < 1.5:
                # 使用 spd 命令绕过角速度上限，直接让两侧履带以直行同等速度 (20脉冲/10ms ≈ 180mm/s) 反转打转
                cmd_vel = "spd -20 20\r\n"
            elif elapsed < 3.5:
                # 直行跨排阶段
                cmd_vel = "spd 20 20\r\n"
            else:
                cmd_vel = "spd -20 20\r\n"

            if current_time - self.last_vel_send_time >= self.vel_send_interval:
                self.send_command(cmd_vel)
                self.last_vel_send_time = current_time
                
            if elapsed >= 4.0 and self.last_state != "PREPARE_S1":
                print("[SerialBridge] U-turn 接近尾声，云台左偏45度准备第二排巡检。")
                self.send_command("sdelta 45 0\r\n")
                self.s1_angle = 45.0
                self.search_yaw = 45.0  # 第二排：面板在左侧监控 (左偏45)
                self.s1_target_angle = 45.0
                self.last_state = "PREPARE_S1"
            
            # U-turn 总耗时: 5.0s
            if elapsed >= 5.0:
                # 开始第二行直行
                self.send_command("vel 180 0\r\n")
                print("[SerialBridge] U-turn 结束，转弯完毕，进入第二排巡检。")
                self.state = "SEARCHING"
                self.search_yaw = 45.0  # 第二排：面板在左侧监控 (左偏45)
                self.s1_target_angle = 45.0
                self.s2_target_angle = -10.0
                
                self.last_vel_send_time = current_time
                self.last_state = self.state
                
                # 冷却 1.0 秒，忽略一开始可能检测到的杂物
                self.cooldown_end_time = current_time + 1.0
                self.state = "COOLDOWN"
                
            return False

        # 1. 冷却直行状态（COOLDOWN）
        if self.state == "COOLDOWN":
            if current_time < self.cooldown_end_time:
                # 冷却期内：强制忽略目标，底盘持续直行，云台复位到右侧45度监控
                cmds = ""
                # 底盘直行指令 (前进 150 mm/s)
                if current_time - self.last_vel_send_time >= self.vel_send_interval:
                    cmds += "vel 180 0\r\n"
                    self.last_vel_send_time = current_time
                # 云台偏转到右侧 45 度监控
                self.s1_target_angle = self.search_yaw
                self.s2_target_angle = self.search_tilt
                step_x = int(round(self.s1_target_angle - self.s1_angle))
                step_y = int(round(self.s2_target_angle - self.s2_angle))
                if step_x != 0 or step_y != 0:
                    cmds += f"sdelta {step_x} {step_y}\r\n"
                    self.s1_angle = max(-83, min(89, self.s1_angle + step_x))
                    self.s2_angle = max(-25, min(0, self.s2_angle + step_y))
                if cmds:
                    self.send_command(cmds)
                self.last_state = self.state
                return False
            else:
                print("[SerialBridge] 冷却直行结束，开始重新检测太阳能板。")
                self.state = "SEARCHING"
                self.last_state = self.state

        # 2. 微调校准状态（CALIBRATING）
        if self.state == "CALIBRATING":
            # 校准超时保护（5.0秒）
            if not hasattr(self, '_calib_start_time') or self._calib_start_time is None:
                self._calib_start_time = current_time
            if current_time - self._calib_start_time > 5.0:
                print("[SerialBridge] 校准超时，直接进入稳定阶段")
                self._calib_start_time = None
                self.state = "STABILIZING"
                self.stabilize_end_time = current_time + 0.5
                self.send_command("vel 0 0\r\n")
                return False
            
            if current_time < self.calib_timer:
                # 保持底盘静止
                if current_time - self.last_vel_send_time >= self.vel_send_interval:
                    self.send_command("vel 0 0\r\n")
                    self.last_vel_send_time = current_time
                return False
            
            # 记录各个位置 of the area, to fine-tune and find the best angle
            if self.calib_step == 1:
                self.calib_areas['left'] = box_area
            elif self.calib_step == 2:
                self.calib_areas['right'] = box_area
            elif self.calib_step == 3:
                self.calib_areas['up'] = box_area
            elif self.calib_step == 4:
                self.calib_areas['down'] = box_area
            
            self.calib_step += 1
            
            if self.calib_step == 1:
                self._move_to_absolute(self.calib_base_s1 - 12.0, self.calib_base_s2)
                self.calib_timer = current_time + self.calib_step_delay
            elif self.calib_step == 2:
                self._move_to_absolute(self.calib_base_s1 + 12.0, self.calib_base_s2)
                self.calib_timer = current_time + self.calib_step_delay
            elif self.calib_step == 3:
                self._move_to_absolute(self.calib_base_s1, self.calib_base_s2 - 12.0)
                self.calib_timer = current_time + self.calib_step_delay
            elif self.calib_step == 4:
                self._move_to_absolute(self.calib_base_s1, self.calib_base_s2 + 12.0)
                self.calib_timer = current_time + self.calib_step_delay
            elif self.calib_step == 5:
                best_pos = 'base'
                max_area = self.calib_areas.get('base', 0.0)
                for pos in ['left', 'right', 'up', 'down']:
                    if self.calib_areas.get(pos, 0.0) > max_area:
                        max_area = self.calib_areas[pos]
                        best_pos = pos
                
                if best_pos == 'base':
                    self.calib_best_s1, self.calib_best_s2 = self.calib_base_s1, self.calib_base_s2
                elif best_pos == 'left':
                    self.calib_best_s1, self.calib_best_s2 = self.calib_base_s1 - 12.0, self.calib_base_s2
                elif best_pos == 'right':
                    self.calib_best_s1, self.calib_best_s2 = self.calib_base_s1 + 12.0, self.calib_base_s2
                elif best_pos == 'up':
                    self.calib_best_s1, self.calib_best_s2 = self.calib_base_s1, self.calib_base_s2 - 12.0
                elif best_pos == 'down':
                    self.calib_best_s1, self.calib_best_s2 = self.calib_base_s1, self.calib_base_s2 + 12.0
                
                self._move_to_absolute(self.calib_best_s1, self.calib_best_s2)
                self.calib_timer = current_time + self.calib_step_delay
            elif self.calib_step == 6:
                self.state = "STABILIZING"
                self.stabilize_end_time = current_time + 0.5  # 停留 0.5s 确认画面清晰并截屏，随后立刻运动
                self.send_command("vel 0 0\r\n")
                
            return False

        # 3. 截图稳定状态（STABILIZING）
        if self.state == "STABILIZING":
            # 保持底盘静止
            if current_time - self.last_vel_send_time >= self.vel_send_interval:
                self.send_command("vel 0 0\r\n")
                self.last_vel_send_time = current_time

            if current_time > self.stabilize_end_time:
                self.inspected_count += 1
                if self.inspected_count == 2:
                    print(f"[SerialBridge] 已成功巡检 {self.inspected_count} 个太阳能板！开始云台回正并执行 U-turn 转弯。")
                    self.state = "UTURN"
                    self.state_start_time = current_time
                    self.send_command("scenter\r\n")  # 云台强制物理归零
                    self.s1_angle = 0.0
                    self.s2_angle = 0.0
                    self.s1_target_angle = 0.0
                    self.s2_target_angle = 0.0
                    
                    # 发送 U-turn 转弯指令 (使用 spd 命令闭环控制，赋予和直行相同的强大动力)
                    self.send_command("spd -20 20\r\n")
                    self.last_vel_send_time = current_time
                    return True # 触发截图
                elif self.inspected_count >= 4:
                    print(f"[SerialBridge] 已成功巡检 {self.inspected_count} 个太阳能板！云台回正，继续直行 5 秒后停止。")
                    self.state = "FINAL_RUN"
                    self.final_run_end_time = current_time + 5.0
                    self.send_command("scenter\r\n")
                    self.send_command("vel 180 0\r\n")
                    self.last_vel_send_time = current_time
                    return True # 触发截图
                else:
                    print(f"[SerialBridge] 已成功巡检 {self.inspected_count} 个太阳能板！进入 COOLDOWN 并继续直行。")
                    self.state = "COOLDOWN"
                    self.cooldown_end_time = current_time + 1.5  # 1.5秒冷却直行，完全关闭检测
                    self.last_state = self.state
                    return True  # 触发截图
            
            self.last_state = self.state
            return False

        # 4. 寻觅直行状态（SEARCHING）
        if self.state == "SEARCHING":
            if not has_target:
                # 目标丢失容忍机制
                if self.last_state == "TRACKING":
                    self.target_lost_count += 1
                    if self.target_lost_count <= self.target_lost_tolerance:
                        return False
                
                # 无目标：底盘持续直行，云台指向右侧 45 度监控
                cmds = ""
                if current_time - self.last_vel_send_time >= self.vel_send_interval:
                    cmds += "vel 180 0\r\n"  # 直行 (180 mm/s)
                    self.last_vel_send_time = current_time
                
                self.s1_target_angle = self.search_yaw
                self.s2_target_angle = self.search_tilt
                step_x = int(round(self.s1_target_angle - self.s1_angle))
                step_y = int(round(self.s2_target_angle - self.s2_angle))
                if step_x != 0 or step_y != 0:
                    cmds += f"sdelta {step_x} {step_y}\r\n"
                    self.s1_angle = max(-83, min(89, self.s1_angle + step_x))
                    self.s2_angle = max(-25, min(0, self.s2_angle + step_y))
                if cmds:
                    self.send_command(cmds)
                
                self.target_lost_count = 0
                self.last_state = self.state
                return False
            else:
                # 发现目标！直接发送刹车指令交由 STM32 硬件防抱死处理，瞬间进入追踪状态消除迟滞
                print("[SerialBridge] [WARN] 直行中发现目标！触发底层硬件刹车，开始追踪...")
                self.send_command("vel 0 0\r\n")
                self.last_vel_send_time = current_time
                self.state = "TRACKING"
                self._tracking_start_time = current_time
                self.target_lost_count = 0
                self.last_state = self.state
                return False




        # 5. 动态追踪对准状态（TRACKING）
        if self.state == "TRACKING":
            if has_target:
                if not hasattr(self, '_tracking_start_time') or self._tracking_start_time is None:
                    self._tracking_start_time = current_time

                # 2.5s 追踪硬超时保护
                if current_time - self._tracking_start_time > 2.5:
                    print("[SerialBridge] 追踪超时，强制进入微调校准")
                    self._tracking_start_time = None
                    self.state = "CALIBRATING"
                    self.calib_step = 1
                    self.calib_areas = {'base': box_area}
                    self.calib_base_s1 = self.s1_angle
                    self.calib_base_s2 = self.s2_angle
                    self._move_to_absolute(self.calib_base_s1 - 12.0, self.calib_base_s2)
                    self.calib_timer = current_time + self.calib_step_delay
                    self._calib_start_time = current_time
                    self.last_state = self.state
                    return False
            else:
                self._tracking_start_time = None

            # 保持底盘完全停止
            if current_time - self.last_vel_send_time >= self.vel_send_interval:
                self.send_command("vel 0 0\r\n")
                self.last_vel_send_time = current_time

            if not has_target:
                # 追踪目标丢失，累积丢失帧数
                self.target_lost_count += 1
                if self.target_lost_count > self.target_lost_tolerance:
                    print(f"[SerialBridge] 目标连续丢失 {self.target_lost_count} 帧，切回直行搜索模式。")
                    self.state = "SEARCHING"
                    self._tracking_start_time = None
                    self.target_lost_count = 0
                    self.last_state = self.state
                return False

            # 有目标，重置丢失计数器，使用 PI 控制器精细调整对准中心
            self.target_lost_count = 0

            # 1. 转换像素偏差为角度差
            deg_per_pixel_x = self.fov_x / self.camera_width
            deg_per_pixel_y = self.fov_y / self.camera_height
            
            delta_angle_x = center_x * deg_per_pixel_x
            delta_angle_y = center_y * deg_per_pixel_y

            if self.invert_x:
                delta_angle_x = -delta_angle_x
            if self.invert_y:
                delta_angle_y = -delta_angle_y

            # 2. 自适应 PI 增益计算
            abs_err_x = abs(center_x)
            if abs_err_x > 200:
                ki_x = 0.80
                kp_x = 0.05
            elif abs_err_x > 100:
                ki_x = 0.60
                kp_x = 0.10
            elif abs_err_x > 40:
                ki_x = 0.40
                kp_x = 0.20
            else:
                ki_x = 0.20
                kp_x = 0.30

            abs_err_y = abs(center_y)
            if abs_err_y > 200:
                ki_y = 0.80
                kp_y = 0.05
            elif abs_err_y > 100:
                ki_y = 0.60
                kp_y = 0.10
            elif abs_err_y > 40:
                ki_y = 0.40
                kp_y = 0.20
            else:
                ki_y = 0.20
                kp_y = 0.30

            # 无扰切换
            if self.last_state != "TRACKING":
                self.last_delta_angle_x = delta_angle_x
                self.last_delta_angle_y = delta_angle_y

            delta_target_x = (ki_x * delta_angle_x + kp_x * (delta_angle_x - self.last_delta_angle_x)) * self.alpha_x
            delta_target_y = (ki_y * delta_angle_y + kp_y * (delta_angle_y - self.last_delta_angle_y)) * self.alpha_y

            self.last_delta_angle_x = delta_angle_x
            self.last_delta_angle_y = delta_angle_y

            # 5像素死区
            if abs(center_x) < 5:
                delta_target_x = 0.0
            if abs(center_y) < 5:
                delta_target_y = 0.0

            # 3. 累加并限制目标绝对角度
            self.s1_target_angle += delta_target_x
            self.s2_target_angle += delta_target_y
            self.s1_target_angle = max(-83.0, min(89.0, self.s1_target_angle))
            self.s2_target_angle = max(-25.0, min(0.0, self.s2_target_angle))

            # 4. 计算整数舵机步长
            step_x = int(round(self.s1_target_angle - self.s1_angle))
            step_y = int(round(self.s2_target_angle - self.s2_angle))

            if step_x != 0 or step_y != 0:
                self.send_command(f"sdelta {step_x} {step_y}\r\n")
                self.s1_angle = max(-83, min(89, self.s1_angle + step_x))
                self.s2_angle = max(-25, min(0, self.s2_angle + step_y))

            # 5. 居中稳定判断：如果X、Y轴居中（或已达物理极限无法继续居中），则认为稳定
            x_stable = abs(center_x) <= self.center_zone or (self.s1_target_angle >= 89.0 and center_x < 0) or (self.s1_target_angle <= -83.0 and center_x > 0)
            y_stable = abs(center_y) <= self.center_zone or (self.s2_target_angle <= -25.0 and center_y > 0) or (self.s2_target_angle >= 0.0 and center_y < 0)
            if x_stable and y_stable:
                if self._stable_since is None:
                    self._stable_since = current_time
                elif current_time - self._stable_since >= self.stable_required_duration:
                    self._stable_since = None
                    self._tracking_start_time = None
                    if self.track_only:
                        print(f"[SerialBridge] track_only: 对准成功，恢复 SEARCHING 直走")
                        self.state = "SEARCHING"
                    else:
                        print(f"[SerialBridge] 目标稳定对准 {self.stable_required_duration}s，开始微调校准")
                        self.state = "CALIBRATING"
                        self.calib_step = 1
                        self.calib_areas = {'base': box_area}
                        self.calib_base_s1 = self.s1_angle
                        self.calib_base_s2 = self.s2_angle
                        self._move_to_absolute(self.calib_base_s1 - 12.0, self.calib_base_s2)
                        self.calib_timer = current_time + self.calib_step_delay
                        self._calib_start_time = current_time
            else:
                self._stable_since = None

            self.last_state = self.state
            return False

        return False

