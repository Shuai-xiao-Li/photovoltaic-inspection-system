#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@File: test_gimbal_bounds.py
@Brief: 用于调试光伏板对准时云台舵机转动物理极限的辅助交互控制脚本。
@Author: 李帅 赵禹博 吴坨鑫
@Date: 6月12号
@Note: 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
"""

"""
test_gimbal_bounds.py — Gimbal bounds and boundary condition empirical test suite.
"""

import sys
import os
import time

# Ensure we can import from the current directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_bridge import SmartRoverController

class GimbalTestHarness:
    def __init__(self):
        # Create a controller but avoid real serial connection
        self.rover = SmartRoverController.__new__(SmartRoverController)
        self.rover.baudrate = 9600
        self.rover.track_only = False
        self.rover.camera_width = 640
        self.rover.camera_height = 480
        self.rover.ser = None
        self.rover.state = "SEARCHING"
        self.rover.cooldown_end_time = 0
        self.rover.stabilize_end_time = 0
        self.rover.sweep_direction = 1
        self.rover.sweep_angle = 0
        self.rover.last_send_time = 0
        self.rover.min_send_interval = 0  # Disable frequency limit for testing speed
        self.rover.last_vel_send_time = 0
        self.rover.vel_send_interval = 0  # Disable velocity interval limit for testing
        
        # v3 properties
        self.rover.s1_angle = 0.0          # Horizontal absolute angle (-83 ~ 89)
        self.rover.s2_angle = 0.0          # Vertical absolute angle (-25 ~ 0)
        self.rover.s1_target_angle = 0.0
        self.rover.s2_target_angle = 0.0
        self.rover.alpha_x = 0.35
        self.rover.alpha_y = 0.35
        self.rover.last_state = "SEARCHING"
        self.rover.last_delta_angle_x = 0.0
        self.rover.last_delta_angle_y = 0.0
        self.rover.invert_x = False
        self.rover.invert_y = True
        self.rover.fov_x = 55.0
        self.rover.fov_y = 43.0
        self.rover.center_zone = 15

        # Calibration
        self.rover.calib_step = 0
        self.rover.calib_timer = 0.0
        self.rover.calib_base_s1 = 0.0
        self.rover.calib_base_s2 = 0.0
        self.rover.calib_best_s1 = 0.0
        self.rover.calib_best_s2 = 0.0
        self.rover.calib_areas = {}
        self.rover.calib_step_delay = 0.3

        # Track the sent commands and the simulated physical gimbal angles
        self.sent_commands = []
        self.physical_s1 = 0.0
        self.physical_s2 = 0.0
        
        def mock_send(cmd):
            self.sent_commands.append(cmd)
            # Parse sdelta commands to update simulated physical gimbal position
            for line in cmd.strip().split('\n'):
                if line.startswith("sdelta"):
                    parts = line.split()
                    if len(parts) == 3:
                        dx = int(parts[1])
                        dy = int(parts[2])
                        self.physical_s1 += dx
                        self.physical_s2 += dy
                elif line.startswith("scenter"):
                    self.physical_s1 = 0.0
                    self.physical_s2 = 0.0
        
        self.rover.send_command = mock_send

    def reset_angles(self, s1=0.0, s2=0.0):
        self.rover.s1_angle = s1
        self.rover.s2_angle = s2
        self.rover.s1_target_angle = s1
        self.rover.s2_target_angle = s2
        self.physical_s1 = s1
        self.physical_s2 = s2
        self.sent_commands.clear()

def run_tests():
    print("==================================================")
    print("RUNNING: GIMBAL BOUNDS AND LIMITS EMPIRICAL VERIFICATION")
    print("==================================================")

    harness = GimbalTestHarness()
    
    # --------------------------------------------------
    # TEST 1: Pan Angle Search Sweep Limits (-80 to +80)
    # --------------------------------------------------
    print("\n[TEST 1] Verifying Pan Angle Search Sweep (-80 to +80)...")
    harness.rover.state = "SEARCHING"
    harness.reset_angles(0.0, 0.0)
    
    # Let's sweep in positive direction until it reverses
    angles_visited = []
    reversal_count = 0
    sweep_directions = []
    
    for i in range(150):
        harness.rover.update(has_target=False, center_x=0, center_y=0, box_area=0)
        angles_visited.append((harness.rover.s1_angle, harness.rover.s1_target_angle))
        sweep_directions.append(harness.rover.sweep_direction)
        
    # Check that angles visited are strictly within [-80.0, 80.0] during search,
    # or if target angle ever exceeded 80.
    max_target = max(a[1] for a in angles_visited)
    min_target = min(a[1] for a in angles_visited)
    max_angle = max(a[0] for a in angles_visited)
    min_angle = min(a[0] for a in angles_visited)
    
    print(f"  Sweep Range Target: [{min_target:.1f}, {max_target:.1f}]")
    print(f"  Sweep Range Actual: [{min_angle:.1f}, {max_angle:.1f}]")
    
    # Verify that target angle is clamped to 80.0 and -80.0
    assert max_target <= 80.0, f"Error: Search sweep target exceeded +80.0: {max_target}"
    assert min_target >= -80.0, f"Error: Search sweep target below -80.0: {min_target}"
    assert max_angle <= 80.0, f"Error: Search sweep actual angle exceeded +80.0: {max_angle}"
    assert min_angle >= -80.0, f"Error: Search sweep actual angle below -80.0: {min_angle}"
    
    # Ensure it successfully reverses direction
    assert -1 in sweep_directions and 1 in sweep_directions, "Error: Sweep direction did not reverse"
    print("PASS: TEST 1: Pan angle sweeps cleanly between -80 and +80.")

    # --------------------------------------------------
    # TEST 2: Tilt Angle Limits (0 to -25) under Extreme Inputs
    # --------------------------------------------------
    print("\n[TEST 2] Verifying Tilt Angle Limits (0 to -25) under Extreme Inputs...")
    
    # Test cases: (initial_s2, center_y_offset, steps)
    test_cases_tilt = [
        # Push to upper limit (0) with extreme positive offsets (invert_y = True, so positive center_y will move down, negative center_y will move up)
        # Wait, center_y = -500 (target is above, so gimbal needs to tilt up to 0)
        (0.0, -500, 50),
        (-15.0, -500, 50),
        (-25.0, -500, 50),
        # Push to lower limit (-25) with extreme negative offsets
        # center_y = 500 (target is below, so gimbal needs to tilt down to -25)
        (0.0, 500, 50),
        (-15.0, 500, 50),
        (-25.0, 500, 50),
    ]
    
    for init_s2, center_y, steps in test_cases_tilt:
        harness.rover.state = "TRACKING"
        harness.reset_angles(0.0, init_s2)
        
        for _ in range(steps):
            harness.rover.update(has_target=True, center_x=0, center_y=center_y, box_area=5000)
            
            # Verify software limits are respected at each step
            assert -25.0 <= harness.rover.s2_target_angle <= 0.0, \
                f"Error: s2_target_angle out of bounds: {harness.rover.s2_target_angle} at init_s2={init_s2}, center_y={center_y}"
            assert -25.0 <= harness.rover.s2_angle <= 0.0, \
                f"Error: s2_angle out of bounds: {harness.rover.s2_angle} at init_s2={init_s2}, center_y={center_y}"
            assert -25.0 <= harness.physical_s2 <= 0.0, \
                f"Error: physical_s2 out of bounds: {harness.physical_s2} at init_s2={init_s2}, center_y={center_y}"

    print("PASS: TEST 2: Tilt angle never exceeds 0 or drops below -25 under extreme offsets.")

    # --------------------------------------------------
    # TEST 3: Consecutive Relative Delta Movements Stability
    # --------------------------------------------------
    print("\n[TEST 3] Verifying stability under consecutive relative delta movements...")
    harness.rover.state = "TRACKING"
    harness.reset_angles(0.0, -12.0)
    
    # Send alternating extreme offsets to check for stability and limit recovery
    for cycle in range(5):
        # Move up to limit 0
        for _ in range(20):
            harness.rover.update(has_target=True, center_x=0, center_y=-500, box_area=500)
            assert -25.0 <= harness.rover.s2_angle <= 0.0, f"Error: s2_angle out of bounds during up-cycle: {harness.rover.s2_angle}"
            assert -25.0 <= harness.physical_s2 <= 0.0, f"Error: physical_s2 out of bounds during up-cycle: {harness.physical_s2}"
        # Move down to limit -25
        for _ in range(20):
            harness.rover.update(has_target=True, center_x=0, center_y=500, box_area=500)
            assert -25.0 <= harness.rover.s2_angle <= 0.0, f"Error: s2_angle out of bounds during down-cycle: {harness.rover.s2_angle}"
            assert -25.0 <= harness.physical_s2 <= 0.0, f"Error: physical_s2 out of bounds during down-cycle: {harness.physical_s2}"

    print("PASS: TEST 3: System remains stable under consecutive relative delta movements without exceeding bounds.")

    # --------------------------------------------------
    # TEST 4: Calibration Mode Boundary Check
    # --------------------------------------------------
    print("\n[TEST 4] Verifying Calibration boundary checks...")
    
    # We want to check what happens when calibration is triggered near or at the tilt/pan boundaries.
    # Calibration does relative offsets of +/- 2 degrees.
    # What if base position is s2_angle = -25?
    # Step 3 subtracts 2 degrees from s2_base (-27).
    # Since _move_to_absolute bounds s2 to [-25, 0], this should result in target_s2 = -25, and no command.
    
    # Let's test at lower boundary (s2 = -25)
    harness.reset_angles(0.0, -25.0)
    harness.rover.state = "TRACKING"
    # Trigger calibration by providing centered target
    harness.rover.update(has_target=True, center_x=5, center_y=5, box_area=5000)
    assert harness.rover.state == "CALIBRATING", f"Expected state to transition to CALIBRATING, got {harness.rover.state}"
    assert harness.rover.calib_step == 1, f"Expected step 1, got {harness.rover.calib_step}"
    
    # Run through the calibration steps and verify bounds
    for step in range(1, 6):
        harness.rover.calib_timer = 0.0 # bypass timer cooldown
        harness.rover.update(has_target=True, center_x=5, center_y=5, box_area=5000 + step*100)
        
        # Verify angles stay within bounds
        assert -83.0 <= harness.rover.s1_angle <= 89.0, f"Calib step {step} s1_angle out of bounds: {harness.rover.s1_angle}"
        assert -25.0 <= harness.rover.s2_angle <= 0.0, f"Calib step {step} s2_angle out of bounds: {harness.rover.s2_angle}"
        assert -83.0 <= harness.physical_s1 <= 89.0, f"Calib step {step} physical_s1 out of bounds: {harness.physical_s1}"
        assert -25.0 <= harness.physical_s2 <= 0.0, f"Calib step {step} physical_s2 out of bounds: {harness.physical_s2}"

    # Let's test at upper boundary (s2 = 0)
    harness.reset_angles(0.0, 0.0)
    harness.rover.state = "TRACKING"
    harness.rover.update(has_target=True, center_x=5, center_y=5, box_area=5000)
    assert harness.rover.state == "CALIBRATING", f"Expected state to transition to CALIBRATING, got {harness.rover.state}"
    
    for step in range(1, 6):
        harness.rover.calib_timer = 0.0
        harness.rover.update(has_target=True, center_x=5, center_y=5, box_area=5000 + step*100)
        
        assert -83.0 <= harness.rover.s1_angle <= 89.0, f"Calib step {step} s1_angle out of bounds: {harness.rover.s1_angle}"
        assert -25.0 <= harness.rover.s2_angle <= 0.0, f"Calib step {step} s2_angle out of bounds: {harness.rover.s2_angle}"
        assert -83.0 <= harness.physical_s1 <= 89.0, f"Calib step {step} physical_s1 out of bounds: {harness.physical_s1}"
        assert -25.0 <= harness.physical_s2 <= 0.0, f"Calib step {step} physical_s2 out of bounds: {harness.physical_s2}"

    print("PASS: TEST 4: Calibration boundary checks prevent out-of-bounds commands.")

    print("\nALL TESTS PASSED SUCCESSFULLY!")
    print("==================================================")

if __name__ == "__main__":
    run_tests()
