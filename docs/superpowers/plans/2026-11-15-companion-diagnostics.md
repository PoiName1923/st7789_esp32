# Companion Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hiển thị nhật ký chẩn đoán ở web và bridge, giữ kết quả lệnh độc lập với trạng thái nền, và làm lỗi GIF có thể truy vết.

**Architecture:** `robot_companion.py` có một event buffer bị giới hạn và endpoint chỉ đọc `/api/log`; mọi API ghi event an toàn. `page.py` thêm vùng log và thay đổi `send` để refresh hậu lệnh không làm fail thao tác. Kiểm thử HTTP chạy bridge thật với robot giả.

**Tech Stack:** Python 3, `http.server`, `unittest`, HTML/CSS/JavaScript thuần.

**Spec:** `docs/superpowers/specs/2026-11-15-companion-diagnostics-design.md`

## Global Constraints

- Không log token, mật khẩu Wi-Fi hay nội dung GIF.
- Browser log tối đa 100 sự kiện; bridge log tối đa 200 sự kiện trong bộ nhớ.
- Không đổi giao thức firmware.
- Lệnh thành công không được báo thất bại chỉ vì refresh trạng thái thất bại.

## Review Focus

- Upload GIF gặp `SerialException` phải trả JSON rõ ràng; kiểm thử ở Task 1.
- Mật khẩu Wi-Fi không xuất hiện trong `/api/log`; kiểm thử ở Task 1.
- Refresh lỗi sau lệnh thành công không đổi nút sang error; kiểm thử contract ở Task 2.
- Log dài phải bỏ sự kiện cũ nhất; kiểm thử ở Task 1.
- Poll log lỗi không được phá chức năng điều khiển; kiểm thử contract ở Task 2.

---

### Task 1: Event log và lỗi bridge có cấu trúc

**Files:**
- Modify: `companion/robot_companion.py`
- Modify: `companion/test_robot_companion.py`

**Interfaces:**
- Produces: `EventLog.add(source, action, outcome, detail)` và `EventLog.snapshot()`; `GET /api/log` trả `{events: [...]}`.

- [ ] **Step 1: Write failing bridge tests**

Thêm test dùng `run_server` với robot giả: endpoint `/api/log` có event upload lỗi SerialException và không chứa chuỗi mật khẩu; EventLog giới hạn số sự kiện.

- [ ] **Step 2: Run tests to verify RED**

Run: `python -m unittest test_robot_companion -v`

Expected: FAIL vì EventLog/API log chưa tồn tại hoặc upload làm đứt response.

- [ ] **Step 3: Implement EventLog and server instrumentation**

Thêm EventLog thread-safe, endpoint `/api/log`, logging thành công/lỗi của execute, connect, build/flash và GIF. Bắt `SerialException` trong các đường API để phản hồi JSON.

- [ ] **Step 4: Run Task 1 tests to verify GREEN**

Run: `python -m unittest test_robot_companion -v`

Expected: PASS.

### Task 2: Trạng thái UI, log web và tiến trình GIF

**Files:**
- Modify: `companion/page.py`
- Modify: `companion/test_robot_companion.py`

**Interfaces:**
- Consumes: `GET /api/log` response `{events: Event[]}`.
- Produces: `recordClientEvent`, `refreshLogs`, and non-blocking post-command status refresh.

- [ ] **Step 1: Write failing page contract tests**

Thêm test xác nhận `send` gọi `refresh` tách khỏi `runAction`, markup có vùng log client/server, và upload có event cho các giai đoạn.

- [ ] **Step 2: Run test to verify RED**

Run: `python -m unittest test_robot_companion.PageMarkupTests -v`

Expected: FAIL vì contract mới chưa có.

- [ ] **Step 3: Implement UI diagnostics**

Thêm hai danh sách log có aria-live, vòng đệm client tối đa 100, poll `/api/log`, và thông báo trạng thái phân biệt lỗi đọc nền. Upload phải ghi các phase trước/sau request, còn action thành công chỉ phụ thuộc response lệnh.

- [ ] **Step 4: Run all companion tests to verify GREEN**

Run: `python -m unittest discover -v`

Expected: PASS.

### Task 3: Hướng dẫn và xác minh

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update Vietnamese operating guide**

Ghi cách chạy web, build/nạp firmware và chẩn đoán: chọn cổng USB native, đóng Serial Monitor/bridge trước khi flash, và đọc log khi nạp GIF.

- [ ] **Step 2: Run the full Python suite**

Run: `python -m unittest discover -v`

Expected: PASS.
