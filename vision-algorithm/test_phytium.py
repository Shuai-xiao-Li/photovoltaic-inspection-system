#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@File: test_phytium.py
@Brief: 专门针对飞腾派异构计算核心资源分配、推理引擎运行环境的基准自检程序。
@Author: 李帅 赵禹博 吴坨鑫
@Date: 6月12号
@Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
"""

"""
test_phytium.py — 飞腾派独立测试脚本

测试模式：
  python3 test_phytium.py --test state    纯软件测试状态机（不需要任何硬件）
  python3 test_phytium.py --test serial   测试串口连接（需要 CH340）
  python3 test_phytium.py --test full     完整串口指令测试（需要 CH340 + STM32）
  python3 test_phytium.py --test all      全部运行
"""

import sys, os, time, argparse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_bridge import SmartRoverController


def test_state_machine():
    print("=" * 50)
    print("🧠 测试：四阶段状态机逻辑模拟（包含 R1-R4 校验）")
    print("=" * 50)

    rover = SmartRoverController.__new__(SmartRoverController)
    rover.ser = None
    rover.state = "SEARCHING"
    rover.cooldown_end_time = 0
    rover.stabilize_end_time = 0
    rover.sweep_direction = 1
    rover.sweep_angle = 0
    rover.last_send_time = 0
    rover.min_send_interval = 0
    rover.last_vel_send_time = 0
    rover.vel_send_interval = 0.3
    
    # Initialize variables for 4-Point Micro-Scan Calibration (R1)
    rover.calib_step = 0
    rover.calib_timer = 0.0
    rover.calib_base_s1 = 0.0
    rover.calib_base_s2 = 0.0
    rover.calib_best_s1 = 0.0
    rover.calib_best_s2 = 0.0
    rover.calib_areas = {}
    rover.calib_step_delay = 0.3
    rover.s1_angle = 0.0
    rover.s2_angle = 0.0
    rover.s1_target_angle = 0.0
    rover.s2_target_angle = 0.0
    rover.fov_x = 55.0
    rover.fov_y = 43.0
    rover.camera_width = 640
    rover.camera_height = 480
    rover.kp_x = 0.05
    rover.kp_y = 0.05
    rover.kd_x = 0.05
    rover.kd_y = 0.05
    rover.prev_error_x = 0
    rover.prev_error_y = 0
    rover.dead_zone = 3
    rover.max_step = 15
    rover.center_zone = 40
    rover.baudrate = 9600
    rover.track_only = False

    # Mock variables for v3/v2 check
    rover.alpha_x = 0.35
    rover.alpha_y = 0.35
    rover.last_state = "SEARCHING"
    rover.last_delta_angle_x = 0.0
    rover.last_delta_angle_y = 0.0
    rover.invert_x = False
    rover.invert_y = True
    rover.center_zone = 15

    def mock_send(cmd):
        print(f"     📤 [模拟发送] {cmd.strip()}")
    rover.send_command = mock_send

    # 1. SEARCHING
    print("\n1. 场景: 无目标 → 搜索")
    res = rover.update(False, 0, 0, 0)
    assert rover.state == "SEARCHING", f"Expected SEARCHING, got {rover.state}"
    assert not res, "Expected False"
    print("  状态: SEARCHING ✅")

    # 2. TRACKING (not centered)
    print("\n2. 场景: 偏离中心 → 追踪")
    res = rover.update(True, 200, 100, 50000)
    assert rover.state == "TRACKING", f"Expected TRACKING, got {rover.state}"
    assert not res, "Expected False"
    print("  状态: TRACKING ✅")

    # 3. TRACKING -> CALIBRATING step 1
    print("\n3. 场景: 居中 → 触发校准 (CALIBRATING step 1)")
    res = rover.update(True, 10, -5, 50000)
    assert rover.state == "CALIBRATING", f"Expected CALIBRATING, got {rover.state}"
    assert rover.calib_step == 1, f"Expected step 1, got {rover.calib_step}"
    assert rover.calib_areas['base'] == 50000, f"Expected base area 50000, got {rover.calib_areas}"
    assert not res, "Expected False"
    print("  状态: CALIBRATING (step 1) ✅")

    # 4. CALIBRATING step 1 waiting
    print("\n4. 场景: 校准步骤 1 等待中")
    res = rover.update(True, 10, -5, 52000)
    assert rover.state == "CALIBRATING", f"Expected CALIBRATING, got {rover.state}"
    assert rover.calib_step == 1, f"Expected step 1, got {rover.calib_step}"
    assert 'left' not in rover.calib_areas, "Expected left area not recorded yet"
    assert not res, "Expected False"
    print("  状态: CALIBRATING (step 1 waiting) ✅")

    # 5. CALIBRATING step 1 -> step 2
    print("\n5. 场景: 校准步骤 1 完成 → 进入步骤 2 (Base -> Left completed)")
    rover.calib_timer = time.time() - 1
    res = rover.update(True, 10, -5, 52000)
    assert rover.state == "CALIBRATING", f"Expected CALIBRATING, got {rover.state}"
    assert rover.calib_step == 2, f"Expected step 2, got {rover.calib_step}"
    assert rover.calib_areas['left'] == 52000, f"Expected left area 52000, got {rover.calib_areas}"
    assert not res, "Expected False"
    print("  状态: CALIBRATING (step 2) ✅")

    # 6. CALIBRATING step 2 -> step 3
    print("\n6. 场景: 校准步骤 2 完成 → 进入步骤 3 (Left -> Right completed)")
    rover.calib_timer = time.time() - 1
    res = rover.update(True, 10, -5, 53000)
    assert rover.state == "CALIBRATING", f"Expected CALIBRATING, got {rover.state}"
    assert rover.calib_step == 3, f"Expected step 3, got {rover.calib_step}"
    assert rover.calib_areas['right'] == 53000, f"Expected right area 53000, got {rover.calib_areas}"
    assert not res, "Expected False"
    print("  状态: CALIBRATING (step 3) ✅")

    # 7. CALIBRATING step 3 -> step 4
    print("\n7. 场景: 校准步骤 3 完成 → 进入步骤 4 (Right -> Up completed)")
    rover.calib_timer = time.time() - 1
    res = rover.update(True, 10, -5, 54000)
    assert rover.state == "CALIBRATING", f"Expected CALIBRATING, got {rover.state}"
    assert rover.calib_step == 4, f"Expected step 4, got {rover.calib_step}"
    assert rover.calib_areas['up'] == 54000, f"Expected up area 54000, got {rover.calib_areas}"
    assert not res, "Expected False"
    print("  状态: CALIBRATING (step 4) ✅")

    # 8. CALIBRATING step 4 -> step 5
    print("\n8. 场景: 校准步骤 4 完成 → 进入步骤 5 (Up -> Down completed)")
    rover.calib_timer = time.time() - 1
    res = rover.update(True, 10, -5, 55000)
    assert rover.state == "CALIBRATING", f"Expected CALIBRATING, got {rover.state}"
    assert rover.calib_step == 5, f"Expected step 5, got {rover.calib_step}"
    assert rover.calib_areas['down'] == 55000, f"Expected down area 55000, got {rover.calib_areas}"
    assert not res, "Expected False"
    print("  状态: CALIBRATING (step 5) ✅")

    # 9. CALIBRATING step 5 -> step 6 (Decision & move to best completed, transition to STABILIZING)
    print("\n9. 场景: 校准步骤 5 完成 → 决策并移动到最佳位置 (Down has max area 55000) 并进入 STABILIZING")
    rover.calib_timer = time.time() - 1
    res = rover.update(True, 10, -5, 55000)
    assert rover.state == "STABILIZING", f"Expected STABILIZING, got {rover.state}"
    assert rover.calib_step == 6, f"Expected step 6, got {rover.calib_step}"
    assert rover.calib_best_s1 == rover.calib_base_s1, f"Expected best_s1 to match best target s1"
    assert rover.calib_best_s2 == rover.calib_base_s2 + 2.0, f"Expected best_s2 to match best target s2"
    assert not res, "Expected False"
    stabilize_diff = rover.stabilize_end_time - time.time()
    assert 4.8 <= stabilize_diff <= 5.2, f"Expected ~5.0s delay, got {stabilize_diff}"
    print("  状态: STABILIZING ✅")

    # 10. STABILIZING (waiting)
    print("\n10. 场景: 最佳位置防抖稳定中")
    res = rover.update(True, 10, -5, 55000)
    assert rover.state == "STABILIZING", f"Expected STABILIZING, got {rover.state}"
    assert not res, "Expected False"
    print("  状态: STABILIZING (waiting) ✅")

    # 11. STABILIZING completed -> COOLDOWN (4s delay, return True)
    print("\n11. 场景: 防抖结束 → 触发截图并进入 COOLDOWN")
    rover.stabilize_end_time = time.time() - 1
    res = rover.update(True, 10, -5, 55000)
    assert rover.state == "COOLDOWN", f"Expected COOLDOWN, got {rover.state}"
    cooldown_diff = rover.cooldown_end_time - time.time()
    assert 3.8 <= cooldown_diff <= 4.2, f"Expected ~4.0s delay, got {cooldown_diff}"
    assert res, "Expected True (capture)"
    print("  状态: COOLDOWN (4s delay, capture=True) ✅")

    # 12. COOLDOWN waiting
    print("\n12. 场景: 冷却期间 → 忽略")
    res = rover.update(True, 0, 0, 50000)
    assert rover.state == "COOLDOWN", f"Expected COOLDOWN, got {rover.state}"
    assert not res, "Expected False"
    print("  状态: COOLDOWN (waiting) ✅")

    # 13. COOLDOWN completed -> SEARCHING
    print("\n13. 场景: 冷却结束 → 恢复搜索")
    rover.cooldown_end_time = time.time() - 1
    res = rover.update(False, 0, 0, 0)
    assert rover.state == "SEARCHING", f"Expected SEARCHING, got {rover.state}"
    assert not res, "Expected False"
    print("  状态: SEARCHING ✅")

    print(f"\n{'='*50}")
    print("✅ R1, R2, R3, R4 状态机与校准测试全部通过！")
    print(f"{'='*50}\n")


def test_serial():
    print("=" * 50)
    print("🔌 测试：串口连接")
    print("=" * 50)
    rover = SmartRoverController()
    if rover.ser and rover.ser.is_open:
        print(f"✅ 连接成功: {rover.ser.port}")
        rover.ser.write(b"help\r\n")
        time.sleep(1.0)
        if rover.ser.in_waiting > 0:
            print(f"  ← STM32 回复:\n{rover.ser.read(rover.ser.in_waiting).decode('ascii', errors='ignore')}")
            print("✅ 双向通信正常！")
        else:
            print("⚠️ 未收到回复，检查 TX/RX 是否接反")
        rover.ser.close()
    else:
        print("❌ 串口未找到")
    print()


def test_full():
    print("=" * 50)
    print("🔗 测试：完整串口指令")
    print("=" * 50)
    rover = SmartRoverController()
    if not rover.ser or not rover.ser.is_open:
        print("❌ 串口未连接")
        return
    for cmd, desc, wait in [
        ("scenter",      "云台归中",     1.5),
        ("sdelta 10 0",  "云台右转",     1.0),
        ("sdelta -20 0", "云台左转",     1.0),
        ("scenter",      "云台复位",     1.5),
        ("status",       "读取状态",     1.5),
    ]:
        print(f"  → {desc}: {cmd}")
        rover.ser.write(f"{cmd}\r\n".encode('ascii'))
        time.sleep(wait)
        if rover.ser.in_waiting > 0:
            print(f"    ← {rover.ser.read(rover.ser.in_waiting).decode('ascii', errors='ignore').strip()}")
    rover.ser.close()
    print(f"\n✅ 完整测试完成！\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--test', default='all', choices=['state','serial','full','all'])
    args = parser.parse_args()
    if args.test in ('state','all'): test_state_machine()
    if args.test in ('serial','all'): test_serial()
    if args.test in ('full','all'): test_full()

