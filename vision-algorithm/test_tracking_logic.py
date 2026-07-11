#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@File: test_tracking_logic.py
@Brief: 光伏板轮廓识别、运动PID响应及防抖触发策略的闭环控制仿真与诊断脚本。
@Author: 李帅 赵禹博 吴坨鑫
@Date: 6月12号
@Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
"""

"""
test_tracking_logic.py — Comprehensive unit tests for tracking, calibration, and stabilization state machine.
"""

import unittest
from unittest.mock import patch
import sys
import os
import time

# Ensure we can import from the current directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_bridge import SmartRoverController


class MockSerial:
    def __init__(self, *args, **kwargs):
        self.in_waiting = 1
        self.written_commands = []
        self.closed = False
        self.is_open = True

    def reset_input_buffer(self):
        pass

    def write(self, data):
        self.written_commands.append(data.decode('ascii'))

    def read(self, num_bytes):
        return b"stm32 ready\r\n"

    def close(self):
        self.closed = True
        self.is_open = False


class TestTrackingLogic(unittest.TestCase):
    def setUp(self):
        self.simulated_time = 1000.0
        
        # Mock serial.Serial and comports to avoid physical connections during instantiation
        self.mock_serial_instance = MockSerial()
        self.serial_patcher = patch('serial.Serial', return_value=self.mock_serial_instance)
        self.serial_patcher.start()
        
        class MockPort:
            def __init__(self, device):
                self.device = device
                
        self.comports_patcher = patch('serial.tools.list_ports.comports', return_value=[MockPort('/dev/ttyUSB0')])
        self.comports_patcher.start()
        
        # Patch sleep and time within serial_bridge to execute instantly
        self.sleep_patcher = patch('serial_bridge.time.sleep', side_effect=self.mock_sleep)
        self.sleep_patcher.start()
        self.time_patcher = patch('serial_bridge.time.time', side_effect=self.get_mock_time)
        self.time_patcher.start()
        
        # Instantiate controller
        self.controller = SmartRoverController(baudrate=9600, track_only=False, camera_width=640, camera_height=480)
        self.mock_serial_instance.written_commands.clear()

        # Override send_command to write synchronously to the mock serial instance for test stability
        def mock_send_command(cmd):
            sub_cmds = [c.strip() for c in cmd.split('\n') if c.strip()]
            for sub_cmd in sub_cmds:
                full_cmd = sub_cmd + "\r\n"
                self.mock_serial_instance.write(full_cmd.encode('ascii'))
        self.controller.send_command = mock_send_command

    def tearDown(self):
        self.serial_patcher.stop()
        self.comports_patcher.stop()
        self.sleep_patcher.stop()
        self.time_patcher.stop()

    def get_mock_time(self):
        return self.simulated_time

    def mock_sleep(self, seconds):
        self.simulated_time += seconds

    def test_scenario_a_searching_mode(self):
        """Scenario A: SEARCHING mode when no target is detected."""
        self.assertEqual(self.controller.state, "SEARCHING")
        
        # Call update with no target
        self.controller.s1_angle = 0.0
        self.controller.s1_target_angle = 0.0
        self.controller.sweep_direction = 1
        
        self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
        self.assertEqual(self.controller.state, "SEARCHING")
        self.assertEqual(self.controller.s1_target_angle, -45.0)  # should align to right side -45 degrees (Right 45)
        self.assertTrue(any("sdelta " in cmd for cmd in self.mock_serial_instance.written_commands))
        self.assertTrue(any("vel 180 0" in cmd for cmd in self.mock_serial_instance.written_commands))

    def test_scenario_b_transition_searching_to_tracking(self):
        """Scenario B: Transition from SEARCHING to TRACKING when a target is detected."""
        self.assertEqual(self.controller.state, "SEARCHING")
        
        # Provide a target
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=50, center_y=50, box_area=5000)
        self.assertEqual(self.controller.state, "TRACKING")
        self.assertTrue(any("vel 0 0" in cmd for cmd in self.mock_serial_instance.written_commands))

    def test_scenario_c_brief_target_loss_tolerance(self):
        """Scenario C: Brief target loss tolerance (< 30 frames) in TRACKING mode."""
        self.controller.state = "TRACKING"
        self.controller.last_state = "TRACKING"
        self.assertEqual(self.controller.target_lost_count, 0)
        
        # Target lost for 15 consecutive updates, should remain in TRACKING state
        for i in range(1, 16):
            self.simulated_time += 0.1
            self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
            self.assertEqual(self.controller.state, "TRACKING")
            self.assertEqual(self.controller.target_lost_count, i)

    def test_scenario_d_target_loss_exceeding_tolerance(self):
        """Scenario D: Target loss exceeding tolerance (>= 30 frames) transitions back to SEARCHING."""
        self.controller.state = "TRACKING"
        self.controller.last_state = "TRACKING"
        
        # Target lost for 30 frames
        for i in range(1, 31):
            self.simulated_time += 0.1
            self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
            self.assertEqual(self.controller.state, "TRACKING")
            self.assertEqual(self.controller.target_lost_count, i)
            
        # 31st frame (loss count exceeds tolerance of 30)
        self.simulated_time += 0.1
        self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
        self.assertEqual(self.controller.state, "SEARCHING")
        self.assertEqual(self.controller.target_lost_count, 0)

    def test_scenario_e_target_centered_triggers_calibrating(self):
        """Scenario E: Target is centered (within ±60 pixels) for 0.2 seconds, triggering CALIBRATING."""
        self.controller.state = "TRACKING"
        self.controller.last_state = "TRACKING"
        
        # First update initializes stable_since
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=10, center_y=10, box_area=5000)
        self.assertEqual(self.controller.state, "TRACKING")
        
        # Second update (0.1s elapsed)
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=10, center_y=10, box_area=5000)
        self.assertEqual(self.controller.state, "TRACKING")
        
        # Third update (0.2s elapsed) - triggers CALIBRATING
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=10, center_y=10, box_area=5000)
        self.assertEqual(self.controller.state, "CALIBRATING")

    def test_scenario_f_calibrating_transitions_and_clamps(self):
        """Scenario F: CALIBRATING state transitions (steps 1 through 6, verifying base/left/right/up/down/best movement logic and angle clamps)."""
        # --- Part 1: Normal Calibration transitions ---
        self.controller.s1_angle = 0.0
        self.controller.s1_target_angle = 0.0
        self.controller.s2_angle = -10.0
        self.controller.s2_target_angle = -10.0
        
        self.controller.state = "TRACKING"
        self.controller.last_state = "TRACKING"
        
        self.controller._stable_since = self.simulated_time - 2.5  # stable
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "CALIBRATING")
        self.assertEqual(self.controller.calib_step, 1)
        
        base_s1 = self.controller.calib_base_s1
        base_s2 = self.controller.calib_base_s2
        
        # Step 1: Left area collected, moves to Right (base_s1 + 10.0)
        self.simulated_time += 0.15
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4000)  # Left
        self.assertEqual(self.controller.calib_step, 2)
        self.assertAlmostEqual(self.controller.s1_angle, base_s1 + 10.0)
        
        # Step 2: Right area collected, moves to Up (base_s2 - 10.0)
        self.simulated_time += 0.15
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=5000)  # Right
        self.assertEqual(self.controller.calib_step, 3)
        self.assertAlmostEqual(self.controller.s2_angle, base_s2 - 10.0)
        
        # Step 3: Up area collected, moves to Down (base_s2 + 10.0)
        self.simulated_time += 0.15
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=3000)  # Up
        self.assertEqual(self.controller.calib_step, 4)
        self.assertAlmostEqual(self.controller.s2_angle, base_s2 + 10.0)
        
        # Step 4: Down area collected, moves to Best (Right is maximum, base_s1 + 10.0)
        self.simulated_time += 0.15
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=3500)  # Down
        self.assertEqual(self.controller.calib_step, 5)
        self.assertAlmostEqual(self.controller.s1_angle, base_s1 + 10.0)
        self.assertAlmostEqual(self.controller.s2_angle, base_s2)
        
        # At 0.15s: CALIBRATING -> STABILIZING (0.5s stop for screenshot)
        self.simulated_time += 0.16
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=5000)
        self.assertEqual(self.controller.calib_step, 6)
        self.assertEqual(self.controller.state, "STABILIZING")

        # --- Part 2: Angle clamps ---
        self.controller.state = "TRACKING"
        self.controller.s1_angle = 89.0
        self.controller.s2_angle = -25.0
        self.controller.s1_target_angle = 89.0
        self.controller.s2_target_angle = -25.0
        self.controller._stable_since = self.simulated_time - 2.5
        
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "CALIBRATING")
        
        # Right move: base_s1 + 10.0 = 99.0 -> clamped to 89.0
        self.simulated_time += 0.15
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4000)
        self.assertEqual(self.controller.calib_step, 2)
        self.assertEqual(self.controller.s1_angle, 89.0)
        
        # Up move: base_s2 - 10.0 = -35.0 -> clamped to -25.0
        self.simulated_time += 0.15
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=5000)
        self.assertEqual(self.controller.calib_step, 3)
        self.assertEqual(self.controller.s2_angle, -25.0)

    def test_scenario_g_calibration_timeout(self):
        """Scenario G: CALIBRATING timeout protection (if calibration takes > 5.0 seconds, it enters STABILIZING)."""
        self.controller.state = "TRACKING"
        self.controller.last_state = "TRACKING"
        self.controller._stable_since = self.simulated_time - 2.5
        
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "CALIBRATING")
        
        # Advance time by 2.6 seconds - should still be in CALIBRATING (timeout is 5.0s now)
        self.simulated_time += 2.6
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "CALIBRATING")
        
        # Advance time by another 2.5 seconds (total 5.1s from start of calibration)
        self.simulated_time += 2.5
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "STABILIZING")
        self.assertAlmostEqual(self.controller.stabilize_end_time, self.simulated_time + 0.5)

    def test_scenario_h_complete_state_transition_sequence(self):
        """Scenario H: Complete state transition sequence (TRACKING -> CALIBRATING -> STABILIZING -> COOLDOWN -> SEARCHING)."""
        self.controller.state = "TRACKING"
        self.controller.last_state = "TRACKING"
        self.controller.s1_angle = 0.0
        self.controller.s1_target_angle = 0.0
        self.controller.s2_angle = -10.0
        self.controller.s2_target_angle = -10.0
        self.controller.sweep_direction = 1
        
        # 1. TRACKING -> CALIBRATING (0.2s stability)
        self.controller._stable_since = self.simulated_time - 2.5
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "CALIBRATING")
        
        # 2. CALIBRATING -> STABILIZING (5 calibration steps)
        for _ in range(5):
            self.simulated_time += 0.15
            self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "STABILIZING")
        
        # 3. STABILIZING -> COOLDOWN (after 0.5s, triggers screenshot)
        self.simulated_time += 0.4
        capture_triggered = self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "STABILIZING")
        self.assertFalse(capture_triggered)
        
        self.simulated_time += 0.2
        self.mock_serial_instance.written_commands.clear()
        capture_triggered = self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "COOLDOWN")
        self.assertTrue(capture_triggered)  # returns True to trigger capture!
        
        # Execute COOLDOWN state behavior
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        
        # Verify cooldown actions: resumes forward straight, resets gimbal to right search yaw
        self.assertTrue(any("vel 180 0" in cmd for cmd in self.mock_serial_instance.written_commands))
        self.assertAlmostEqual(self.controller.cooldown_end_time, self.simulated_time + 1.4)
        
        # 4. COOLDOWN -> SEARCHING (after 1.5s total cooldown)
        self.simulated_time += 1.5
        self.controller.update(has_target=True, center_x=0, center_y=0, box_area=4500)
        self.assertEqual(self.controller.state, "TRACKING")

    def test_scenario_i_pi_deadband_and_damping(self):
        """Scenario I: Verify the new PI controller deadband (5 pixels) and damping coefficient features."""
        # --- Deadband ---
        self.controller.state = "TRACKING"
        self.controller.last_state = "TRACKING"
        
        self.controller.s1_angle = 0.0
        self.controller.s1_target_angle = 0.0
        self.controller.s2_angle = -10.0
        self.controller.s2_target_angle = -10.0
        self.controller.last_delta_angle_x = 0.0
        self.controller.last_delta_angle_y = 0.0
        
        # Update inside deadband (4 pixels)
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=4, center_y=4, box_area=5000)
        self.assertEqual(self.controller.s1_target_angle, 0.0)
        self.assertEqual(self.controller.s2_target_angle, -10.0)
        
        # Update outside deadband (6 pixels)
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=6, center_y=6, box_area=5000)
        self.assertNotEqual(self.controller.s1_target_angle, 0.0)
        self.assertNotEqual(self.controller.s2_target_angle, -10.0)
        
        # --- Damping Coefficient ---
        # Compare step size of two different controllers with different damping coefficients
        self.simulated_time += 0.1
        c1 = SmartRoverController(baudrate=9600, track_only=False, camera_width=640, camera_height=480)
        c1.alpha_x = 0.35
        c1.state = "TRACKING"
        c1.last_state = "TRACKING"
        c1.s1_angle = 0.0
        c1.s1_target_angle = 0.0
        c1.update(has_target=True, center_x=100, center_y=0, box_area=5000)
        target_delta_c1 = c1.s1_target_angle
        
        self.simulated_time += 0.1
        c2 = SmartRoverController(baudrate=9600, track_only=False, camera_width=640, camera_height=480)
        c2.alpha_x = 0.70
        c2.state = "TRACKING"
        c2.last_state = "TRACKING"
        c2.s1_angle = 0.0
        c2.s2_angle = 0.0
        c2.s1_target_angle = 0.0
        c2.s2_target_angle = 0.0
        c2.update(has_target=True, center_x=100, center_y=0, box_area=5000)
        target_delta_c2 = c2.s1_target_angle
        
        # The target delta should be proportional to alpha_x
        self.assertAlmostEqual(target_delta_c2, target_delta_c1 * 2.0)

    def test_scenario_j_uturn_and_finished_states(self):
        """Scenario J: Verify U-turn (triggered at 2 panels) and Finished (triggered at 4 panels) states."""
        self.controller.state = "STABILIZING"
        self.controller.inspected_count = 1
        self.controller.stabilize_end_time = self.simulated_time - 1.0 # expired
        
        # Expiration should trigger UTURN state, return True (for capture), and reset s1/s2 angle to 0 (scenter)
        self.simulated_time += 0.1
        self.mock_serial_instance.written_commands.clear()
        capture_triggered = self.controller.update(has_target=True, center_x=0, center_y=0, box_area=5000)
        
        self.assertTrue(capture_triggered)
        self.assertEqual(self.controller.inspected_count, 2)
        self.assertEqual(self.controller.state, "UTURN")
        self.assertEqual(self.controller.s1_angle, 0.0)
        self.assertEqual(self.controller.s2_angle, 0.0)
        self.assertTrue(any("scenter" in cmd for cmd in self.mock_serial_instance.written_commands))
        self.assertTrue(any("vel 0 1200" in cmd for cmd in self.mock_serial_instance.written_commands))
        
        # Update during UTURN:
        # At 1.0s (Phase 1): still turning, vel 0 1200, gimbal not turned
        self.simulated_time += 1.0
        self.mock_serial_instance.written_commands.clear()
        self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
        self.assertEqual(self.controller.state, "UTURN")
        self.assertTrue(any("vel 0 1200" in cmd for cmd in self.mock_serial_instance.written_commands))
        
        # At 2.5s (Phase 2): driving straight, vel 200 0
        self.simulated_time += 1.5
        self.mock_serial_instance.written_commands.clear()
        self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
        self.assertEqual(self.controller.state, "UTURN")
        self.assertTrue(any("vel 200 0" in cmd for cmd in self.mock_serial_instance.written_commands))
        
        # At 4.2s (Phase 3): spin in place, vel 0 1200, gimbal should turn left 45 degrees (45.0)
        self.simulated_time += 1.7
        self.mock_serial_instance.written_commands.clear()
        self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
        self.assertEqual(self.controller.state, "UTURN")
        self.assertEqual(self.controller.s1_angle, 45.0)
        self.assertTrue(any("sdelta 45" in cmd for cmd in self.mock_serial_instance.written_commands))
        self.assertTrue(any("vel 0 1200" in cmd for cmd in self.mock_serial_instance.written_commands))
        
        # At 5.1s (total): U-turn ends, transitions to COOLDOWN (with search_yaw = 45.0)
        self.simulated_time += 0.9
        self.mock_serial_instance.written_commands.clear()
        self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
        self.assertEqual(self.controller.state, "COOLDOWN")
        self.assertEqual(self.controller.search_yaw, 45.0)
        self.assertTrue(any("vel 180 0" in cmd for cmd in self.mock_serial_instance.written_commands))
        
        # Verify FINISHED state when inspected_count reaches 4:
        self.controller.state = "STABILIZING"
        self.controller.inspected_count = 3
        self.controller.stabilize_end_time = self.simulated_time - 1.0 # expired
        
        self.simulated_time += 0.1
        self.mock_serial_instance.written_commands.clear()
        capture_triggered = self.controller.update(has_target=True, center_x=0, center_y=0, box_area=5000)
        
        self.assertTrue(capture_triggered)
        self.assertEqual(self.controller.inspected_count, 4)
        self.assertEqual(self.controller.state, "FINAL_RUN")
        self.assertTrue(any("scenter" in cmd for cmd in self.mock_serial_instance.written_commands))
        self.assertTrue(any("vel 180 0" in cmd for cmd in self.mock_serial_instance.written_commands))
        
        # Advance simulated time by 5.1 seconds and check for FINISHED state
        self.simulated_time += 5.1
        self.mock_serial_instance.written_commands.clear()
        self.controller.update(has_target=False, center_x=0, center_y=0, box_area=0)
        
        self.assertEqual(self.controller.state, "FINISHED")
        self.assertTrue(any("vel 0 0" in cmd for cmd in self.mock_serial_instance.written_commands))

    def test_scenario_k_tracking_timeout(self):
        """Scenario K: TRACKING timeout protection (if tracking takes > 2.5 seconds without stabilizing, it forcefully enters CALIBRATING)."""
        self.controller.state = "SEARCHING"
        self.controller.last_state = "SEARCHING"
        
        # 1. SEARCHING -> TRACKING
        self.simulated_time += 0.1
        self.controller.update(has_target=True, center_x=100, center_y=100, box_area=5000)
        self.assertEqual(self.controller.state, "TRACKING")
        self.assertIsNotNone(self.controller._tracking_start_time)
        tracking_start = self.controller._tracking_start_time
        
        # 2. Advance time by 1.0s, keep unstable (center_x/y = 100, which is outside center_zone of 60)
        self.simulated_time += 1.0
        self.controller.update(has_target=True, center_x=100, center_y=100, box_area=5000)
        self.assertEqual(self.controller.state, "TRACKING")
        self.assertEqual(self.controller._tracking_start_time, tracking_start)
        
        # 3. Advance time by 1.6s (total 2.6s elapsed > 2.5s)
        self.simulated_time += 1.6
        self.controller.update(has_target=True, center_x=100, center_y=100, box_area=5000)
        
        # State should forcefully transition to CALIBRATING
        self.assertEqual(self.controller.state, "CALIBRATING")
        self.assertIsNone(self.controller._tracking_start_time)
        self.assertEqual(self.controller.calib_step, 1)
        self.assertAlmostEqual(self.controller.s1_angle, self.controller.calib_base_s1 - 10.0)
        self.assertEqual(self.controller.s2_angle, self.controller.calib_base_s2)
        self.assertAlmostEqual(self.controller.calib_timer, self.simulated_time + self.controller.calib_step_delay)
        self.assertAlmostEqual(self.controller._calib_start_time, self.simulated_time)


if __name__ == "__main__":
    unittest.main()

