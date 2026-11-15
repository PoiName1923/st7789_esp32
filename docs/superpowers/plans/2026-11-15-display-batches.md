# Display Batches Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Làm batch Khuôn mặt tự luân phiên biểu cảm, trong khi GIF, Thời tiết và Cảm biến vẫn là các batch cố định.

**Architecture:** `app_controller` dùng lại `robot_fsm` chỉ khi `s_mode == ROBOT_SCREEN_FACE`. Lệnh chọn GIF dừng FSM, lặp đúng animation đã chọn và lưu `ROBOT_SCREEN_FACE`; các mode thông tin dừng FSM và animation như hiện tại.

**Tech Stack:** ESP-IDF 5.5, C, LVGL, host C tests, HTML/JavaScript thuần.

**Spec:** `docs/superpowers/specs/2026-11-15-display-batches-design.md`

## Global Constraints

- Không thêm enum batch, API USB mới hoặc thay đổi định dạng settings.
- Không tự chuyển giữa batch.
- Khuôn mặt chỉ luân phiên bảy biểu cảm được nêu trong spec.

## Review Focus

- Batch Khuôn mặt không được tạo state Thời tiết/Cảm biến.
- GIF được chọn không bị FSM ghi đè.
- Chuyển sang Thời tiết/Cảm biến dừng animation và FSM.
- Khởi động lại từ `ROBOT_SCREEN_FACE` khởi động lại batch biểu cảm, trừ khi animation đã chọn là GIF người dùng.
- Web mô tả đúng sự khác nhau giữa Biểu cảm liên tục và GIF.

---

### Task 1: FSM expression-only configuration

**Files:**
- Modify: `components/app_controller/robot_fsm.c`
- Test: `tests/host/test_robot_fsm.c`

- [ ] Write a failing host test showing a configuration with weather/sensor disabled only enters face-expression states.
- [ ] Run `sh tests/host/run.sh` and verify it fails.
- [ ] Make the existing FSM configuration explicit in `app_controller` so only its expression and blink paths are enabled for the face batch.
- [ ] Run `sh tests/host/run.sh` and verify it passes.

### Task 2: Controller batch switching

**Files:**
- Modify: `components/app_controller/app_controller.c`
- Modify: `components/app_controller/include/app_controller.h`
- Test: `tests/host/test_robot_fsm.c`

- [ ] Write failing tests or a host-testable helper for controller mode decisions: face starts FSM; GIF/weather/sensor stop it.
- [ ] Run the relevant host test and verify RED.
- [ ] Add a private face-FSM instance/timer path in `app_controller`; map FSM states to animation IDs; detect user GIF IDs (`>= 9`) so GIF playback remains fixed.
- [ ] Run host tests and `idf.py build`; verify GREEN.

### Task 3: Web wording and final verification

**Files:**
- Modify: `companion/page.py`
- Modify: `companion/test_robot_companion.py`

- [ ] Write a failing markup test for the “Biểu cảm liên tục” label and GIF batch copy.
- [ ] Implement the text-only UI clarification without changing endpoints.
- [ ] Run `python -m unittest discover -v -s companion`, `sh tests/host/run.sh`, and `idf.py build`.
