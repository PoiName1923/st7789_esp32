# Companion Tabs and Button Feedback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Chia trang điều khiển USB thành năm tab responsive và cung cấp phản hồi trực quan cho mọi nút thao tác.

**Architecture:** Giữ giao diện HTML/CSS/JavaScript thuần trong hằng `PAGE`, không thay đổi server hay API. Markup dùng tab/panel semantic; một hàm JavaScript dùng chung quản lý trạng thái pending/success/error của nút và vùng thông báo.

**Tech Stack:** Python 3.11, `http.server`, HTML5, CSS3, JavaScript thuần, `unittest`, PyInstaller.

**Spec:** `docs/superpowers/specs/2026-11-15-companion-tabs-feedback-design.md`

## Global Constraints

- Năm tab chính xác: Tổng quan, Màn hình, GIF, Thời tiết, Firmware.
- Không thay đổi API, giao thức USB, firmware hay quy trình nạp.
- Không thêm framework frontend, icon pack hoặc phụ thuộc mới.
- Hỗ trợ chiều rộng 360 px, bàn phím, ARIA và `prefers-reduced-motion`.
- Khóa nút trong request để ngăn gửi trùng.

## Review Focus

- URL hash không hợp lệ phải quay về tab Tổng quan, không để trang trống.
- Chuyển tab bằng bàn phím phải cập nhật focus, `aria-selected` và panel hiển thị đồng bộ.
- Request thất bại phải mở khóa nút và hiển thị lỗi, không kẹt ở trạng thái loading.
- Double-click trong khi request đang chờ chỉ được gửi một request.
- Khi firmware toolchain không có, tab Firmware phải hiển thị empty state thay vì các nút không hoạt động.

---

### Task 1: Accessible tab shell and responsive visual system

**Files:**
- Modify: `companion/robot_companion.py:PAGE`
- Modify: `companion/test_robot_companion.py`

**Interfaces:**
- Consumes: `PAGE` với placeholder `__TOKEN__` và `__FIRMWARE_AVAILABLE__`.
- Produces: năm phần tử `[role=tab]`, năm `[role=tabpanel]`, hàm JavaScript `activateTab(name, focus)` và URL hash `#<tab-name>`.

- [ ] **Step 1: Write the failing semantic-structure test**

Thêm `PageMarkupTests.test_tabs_have_matching_accessible_panels` dùng `html.parser.HTMLParser` trên `PAGE`; khẳng định có đúng các tab `overview`, `display`, `gifs`, `weather`, `firmware`, mỗi `aria-controls` trỏ tới một panel và chỉ tab đầu có `aria-selected="true"`.

- [ ] **Step 2: Run the test and verify RED**

Run: `python -m unittest test_robot_companion.PageMarkupTests.test_tabs_have_matching_accessible_panels -v`

Expected: FAIL vì `PAGE` chưa có tab semantic.

- [ ] **Step 3: Implement the tab shell and visual system**

Trong `PAGE`, thêm header trạng thái cố định, tablist cuộn ngang, năm panel phân nhóm các điều khiển hiện có, card layout responsive và `activateTab(name, focus)`. Xử lý click, ArrowLeft/ArrowRight/Home/End, `hashchange` và fallback `overview`.

- [ ] **Step 4: Add the narrow-screen and reduced-motion assertions**

Thêm `PageMarkupTests.test_page_declares_mobile_and_reduced_motion_rules`; kiểm tra output có media query `max-width: 600px`, `prefers-reduced-motion: reduce`, tablist overflow và vùng thông báo `aria-live="polite"`.

- [ ] **Step 5: Run Task 1 tests and verify GREEN**

Run: `python -m unittest test_robot_companion.PageMarkupTests -v`

Expected: PASS.

### Task 2: Shared button interaction feedback

**Files:**
- Modify: `companion/robot_companion.py:PAGE`
- Modify: `companion/test_robot_companion.py`

**Interfaces:**
- Consumes: các lời gọi `send(path, data, button)` và `upload(button)` từ markup.
- Produces: `runAction(button, pendingLabel, action) -> Promise`, các trạng thái DOM `is-pending`, `is-success`, `is-error`, và thông báo `data-kind`.

- [ ] **Step 1: Write the failing action-state contract test**

Thêm `PageMarkupTests.test_action_buttons_use_shared_feedback_contract`; khẳng định các nút API truyền `this`, hàm dùng chung có nhánh success/error/finally, đặt `disabled` khi pending và khôi phục nhãn ban đầu.

- [ ] **Step 2: Run the test and verify RED**

Run: `python -m unittest test_robot_companion.PageMarkupTests.test_action_buttons_use_shared_feedback_contract -v`

Expected: FAIL vì `runAction` chưa tồn tại.

- [ ] **Step 3: Implement shared feedback and button motion**

Triển khai `runAction`, chuyển `send` và `upload` sang dùng wrapper này, thêm spinner CSS, press transform, success/error flash và khôi phục nút trong `finally`. Giữ nút bị `disabled` suốt request và bỏ qua lời gọi nếu đang pending.

- [ ] **Step 4: Implement firmware empty state**

Khi `__FIRMWARE_AVAILABLE__` là false, thay nội dung panel Firmware bằng hướng dẫn ngắn; không để lại nút Build/Nạp.

- [ ] **Step 5: Run Task 2 tests and full Python suite**

Run: `python -m unittest discover -v`

Expected: tất cả test PASS, không có warning/error.

### Task 3: Visual verification and Windows package

**Files:**
- Modify: `companion/dist/DesktopRobotUSB.exe` (generated, ignored by Git)

**Interfaces:**
- Consumes: `companion/robot_companion.py` đã hoàn thành Task 1–2.
- Produces: EXE mới chứa giao diện tab và button feedback.

- [ ] **Step 1: Run the local companion without opening the browser automatically**

Khởi động `run_server(..., open_browser=False)` trên cổng tạm, mở URL có token trong trình duyệt và kiểm tra một lần ở viewport desktop cùng mobile 360 px.

- [ ] **Step 2: Verify interaction states manually in the bounded visual pass**

Kiểm tra: chuyển cả năm tab; hash không hợp lệ; bàn phím; nút pending/success/error; double-click; firmware empty state; không overflow ngang ngoài tablist.

- [ ] **Step 3: Apply one batched visual correction if needed**

Chỉnh toàn bộ lỗi phát hiện trong một patch, sau đó xác nhận lại một lần; không lặp polish không giới hạn.

- [ ] **Step 4: Build the EXE**

Run from `companion/`:

`python -m PyInstaller --noconfirm --clean --onefile --name DesktopRobotUSB --distpath dist --workpath .build --specpath .build --paths .vendor robot_companion.py`

Expected: exit code 0 and `dist/DesktopRobotUSB.exe` has a fresh timestamp.

- [ ] **Step 5: Run final verification**

Run: `python -m unittest discover -v`

Expected: all tests PASS. Record EXE SHA-256 and location in the handoff.
