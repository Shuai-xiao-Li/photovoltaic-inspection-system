# Project: Solar Panel Tracking and Detection System Optimization

## Architecture
- `phytium_accelerator.py`: Implements size/color-based contour filtering in `SolarPanelTracker`, frame difference, ONNX inference, and Big/Little core scheduling.
- `realtime_detector_accel.py`: Runs the main real-time loop, manages the tracker state machine, handles AI classification, outputs metadata/BMPs for the UI, and communicates via `serial_bridge.py`.
- `serial_bridge.py`: Defines `SmartRoverController` to interact with the STM32 crawler chassis (moving left/right/up/down/stop) and locking the gimbal.

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | Exploration | Analyze existing codebase and requirements | None | DONE |
| 2 | Implementation | Implement requirements R1, R2, R3, R4 in tracking & detection scripts | 1 | DONE |
| 3 | Verification | Run unit tests, simulate states, check limits & criteria | 2 | DONE |
| 4 | Forensic Audit | Perform integrity verification using forensic auditor | 3 | DONE |

## Interface Contracts
### `phytium_accelerator.py` ↔ `realtime_detector_accel.py`
- `SolarPanelTracker`: Main class for tracking. Needs updated contour filtering parameters.
- `FrameDiffDetector`, `ONNXInferenceEngine`: Inference pipelines.

### `realtime_detector_accel.py` ↔ `serial_bridge.py`
- `SmartRoverController` commands to control chassis (movement, stops) and gimbal (movement, limits, locking).

## Code Layout
- `phytium_accelerator.py` - Core hardware acceleration and tracker filtering
- `realtime_detector_accel.py` - Main execution loop and state machine
- `serial_bridge.py` - Serial communication bridge to STM32
- `test_phytium.py` - Testing tool
