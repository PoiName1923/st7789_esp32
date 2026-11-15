# Hướng dẫn sử dụng — Desktop Robot (ESP32-S3)

Tài liệu này hướng dẫn từng bước: lắp mạch → cài môi trường → cấu hình → nạp code → thay animation → tùy biến hành vi, giao diện → kiểm thử → xử lý sự cố.
Bản tiếng Anh đầy đủ hơn về kiến trúc nằm trong [README.md](README.md).

## Điều khiển bằng điện thoại

Trước khi nạp lần đầu, chạy `idf.py menuconfig`, vào **Desktop Robot → Wi-Fi** và đặt hai mật khẩu khác nhau: mật khẩu quản trị web và mật khẩu Wi-Fi dự phòng (ASCII, 8–63 ký tự). Không để trống và không dùng lại cùng một mật khẩu.

Kết nối điện thoại vào Wi-Fi dự phòng của robot rồi mở `https://192.168.4.1/`. Chứng chỉ đi kèm là chứng chỉ tự ký nên trình duyệt sẽ yêu cầu xác nhận. Trước khi dùng thật, thay `components/web/assets/server.crt` và `server.key` bằng chứng chỉ riêng của từng robot. Nếu cần chống đọc trộm flash khi có truy cập vật lý, bật NVS encryption và flash encryption.

Khi đổi Wi-Fi nhà hoặc mật khẩu Wi-Fi dự phòng, kết nối có thể bị ngắt; hãy kết nối lại bằng thông tin mới. Các thay đổi hướng xoay, RGB/BGR, đảo màu và offset có hiệu lực sau khi khởi động lại. Độ sáng không chỉnh được khi chân BL đang nối trực tiếp 3V3.

**Mục lục**

1. [Chuẩn bị phần cứng](#1-chuẩn-bị-phần-cứng)
2. [Cài môi trường lập trình](#2-cài-môi-trường-lập-trình)
3. [Cấu hình project](#3-cấu-hình-project)
4. [Build và nạp code](#4-build-và-nạp-code)
5. [Lần chạy đầu tiên: căn chỉnh màn hình](#5-lần-chạy-đầu-tiên-căn-chỉnh-màn-hình)
6. [Thay đổi animation](#6-thay-đổi-animation)
7. [Tùy chỉnh hành vi robot](#7-tùy-chỉnh-hành-vi-robot)
8. [Tùy chỉnh giao diện](#8-tùy-chỉnh-giao-diện)
9. [Thời tiết và Wi-Fi](#9-thời-tiết-và-wi-fi)
10. [Kiểm thử](#10-kiểm-thử)
11. [Xử lý sự cố](#11-xử-lý-sự-cố)
12. [Bảng tra cứu nhanh](#12-bảng-tra-cứu-nhanh)

---

## 1. Chuẩn bị phần cứng

### Linh kiện

| Linh kiện | Ghi chú |
|---|---|
| ESP32-S3 **N16R8** | 16 MB flash, 8 MB PSRAM (octal). Board khác loại cần sửa cấu hình (xem §11). |
| Màn hình TFT ST7789 240×240 SPI | Có hoặc không có chân CS đều được |
| Cảm biến DHT11 | Loại module 3 chân thường đã có điện trở kéo lên |
| Pin LiPo 1 cell (3.7V) + module sạc/xả 5V | Module (IP5306, TP4056+boost…) nâng lên 5V, cấp vào chân 5V/VIN của board |
| Cáp USB-C **có truyền dữ liệu** | Cáp chỉ sạc sẽ không nạp được code |

### Sơ đồ đấu dây (mặc định)

| Màn hình ST7789 | ESP32-S3 |
|---|---|
| SCL / SCLK | GPIO12 |
| SDA / MOSI | GPIO11 |
| CS | GPIO10 |
| DC | GPIO14 |
| RES / RST | GPIO13 |
| BL | 3V3 (hoặc một GPIO, xem §3) |
| VCC | 3V3 |
| GND | GND |

| DHT11 | ESP32-S3 |
|---|---|
| DATA | GPIO4 |
| VCC | 3V3 |
| GND | GND |

| Module sạc/xả 5V | Nối tới |
|---|---|
| B+ / B− | Cực + / − của pin LiPo |
| OUT+ (5V) | 5V/VIN của ESP32-S3 |
| OUT− | GND |
| IN (USB) | Sạc pin |

### Kiểm tra trước khi cấp điện

- [ ] OUT+ của module nối vào chân **5V/VIN**, **không** nối vào 3V3. Đúng cực +/−. Đo OUT bằng đồng hồ trước, phải ra khoảng 5.0–5.2V.
- [ ] **Không cắm USB vào ESP32 khi module đang cấp 5V**, trừ khi board có diode giữa VBUS của USB và chân 5V. Nếu không có diode, hai nguồn 5V sẽ chập vào nhau. Lúc nạp code, tắt module hoặc rút OUT+ ra.
- [ ] Gắn tụ **470–1000 µF** giữa 5V và GND, đặt gần board. Khi cắm hoặc rút sạc, nhiều module bị sụt áp trong giây lát, có thể làm ESP32 reset.
- [ ] Module IP5306 **tự ngắt** khi tải nhỏ hơn khoảng 50 mA. Robot thường kéo hơn mức đó khi bật Wi-Fi và màn hình, nhưng nếu thấy nó tự tắt sau khoảng 30 s thì đó là nguyên nhân.
- [ ] Màn hình dùng logic 3.3V. Không cấp 5V vào chân tín hiệu.
- [ ] Màn hình **không có chân CS**: ghi nhớ để đặt `PIN_LCD_CS = -1` và `SPI mode = 3` ở bước 3.
- [ ] Chân **BL** (đèn nền) phải được nối lên 3V3 hoặc nối vào một GPIO, nếu không màn hình sẽ tối đen.
- [ ] **Không dùng GPIO26–37** cho bất kỳ tín hiệu nào. Trên N16R8 các chân này dành cho flash/PSRAM. Firmware sẽ báo lỗi và dừng nếu bạn dùng.
- [ ] Hạn chế dùng GPIO0, 3, 45, 46 (chân strapping), 19, 20 (USB) và 43, 44 (UART log).
- [ ] DHT11 loại 4 chân (không module) cần điện trở 4.7–10 kΩ nối từ DATA lên 3V3.

---

## 2. Cài môi trường lập trình

Project dùng **ESP-IDF v5.5.x** (không dùng Arduino). **Không chọn bản 6.x**: project được khóa ở `<6.0` vì bản 6.x thay đổi API.

### Cách 1 — VS Code + ESP-IDF Extension (khuyên dùng)

1. Cài [VS Code](https://code.visualstudio.com/).
2. Vào Extensions (`Ctrl+Shift+X`), tìm và cài **ESP-IDF** (nhà phát hành: Espressif Systems).
3. Mở Command Palette (`Ctrl+Shift+P`) → **ESP-IDF: Configure ESP-IDF Extension**.
4. Chọn **Express**, chọn phiên bản **v5.5.x**, chọn thư mục cài (đường dẫn **không có dấu cách và không có tiếng Việt có dấu**, ví dụ `C:\Espressif`).
5. Chờ cài xong (tải khoảng 1–2 GB, mất 10–30 phút tùy mạng).

### Cách 2 — ESP-IDF Installation Manager (EIM)

Chạy `eim-gui-windows-x64-*.exe`, chọn **v5.5.x**, target **esp32s3**, rồi cài. Sau đó trong VS Code vẫn dùng extension ESP-IDF như cách 1 để mở terminal.

### Driver USB

- Cổng **USB** (native) của ESP32-S3: Windows 10/11 tự nhận, không cần driver.
- Cổng **COM/UART**: nếu Device Manager hiện thiết bị lạ, cài driver **CP210x** (Silicon Labs) hoặc **CH343** (WCH) tùy chip trên board.

### Mở project

1. VS Code → **File → Open Folder** → chọn thư mục `desktop_robot`.
2. Mở terminal có sẵn môi trường IDF: `Ctrl+Shift+P` → **ESP-IDF: Open ESP-IDF Terminal**.
3. Kiểm tra bằng lệnh:

```bash
idf.py --version
```

Kết quả phải là `ESP-IDF v5.5.x`.

> Mọi lệnh `idf.py` trong tài liệu này đều chạy trong **ESP-IDF Terminal**, tại thư mục `desktop_robot`.

---

## 3. Cấu hình project

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

Trong menuconfig, dùng phím mũi tên để di chuyển, `Enter` để vào hoặc sửa, `Space` để bật/tắt, `S` để lưu, `Q` để thoát. Vào mục **Desktop Robot**.

### Bắt buộc: Wi-Fi

**Desktop Robot → Wi-Fi**
- `SSID`: tên mạng Wi-Fi (chỉ hỗ trợ 2.4 GHz).
- `Password`: mật khẩu.

Để trống SSID nếu muốn robot chạy offline. Khi đó vẫn có mặt robot và cảm biến, chỉ không có thời tiết.

> Mật khẩu được lưu trong file `sdkconfig`, file này **đã được loại khỏi git**. **Không** ghi mật khẩu vào `sdkconfig.defaults` hay vào mã nguồn.

### Nếu đấu dây khác mặc định: Pins

**Desktop Robot → Pins** — đặt số GPIO cho từng tín hiệu. Dùng `-1` cho chân không nối (CS, RST, BL).

### Các mục còn lại

| Menu | Nội dung |
|---|---|
| Display (ST7789) | Độ phân giải, tốc độ SPI, xoay màn hình, thứ tự màu RGB/BGR, đảo màu, độ lệch (offset) |
| Features | Bật/tắt thời tiết, cảm biến, GIF; chọn nguồn GIF |
| Weather (Open-Meteo) | Vĩ độ, kinh độ, múi giờ, chu kỳ cập nhật |
| Sensor (DHT11) | Chu kỳ đọc (2–5 s), thời gian coi là dữ liệu cũ |
| Animation & screens | FPS, thời gian chớp mắt, biểu cảm, chuyển màn hình, hiệu ứng |
| Diagnostics | Log heap/FPS, self-test |

> **Lưu ý:** `sdkconfig.defaults` chỉ được áp dụng khi **chưa có** file `sdkconfig`. Nếu bạn sửa `sdkconfig.defaults`, hãy xóa `sdkconfig` rồi chạy lại `idf.py reconfigure`. Cách thứ hai là sửa trực tiếp trong menuconfig.

---

## 4. Build và nạp code

### 4.1. Build

```bash
idf.py reconfigure     # lần đầu: tải thư viện LVGL + esp_lvgl_port (cần Internet)
idf.py build
```

Build thành công khi cuối log có dòng `Project build complete`.

Trong VS Code bạn cũng có thể bấm các nút trên thanh trạng thái phía dưới: **Build** (biểu tượng cờ lê), **Flash** (tia sét), **Monitor** (màn hình).

### 4.2. Tìm cổng COM

1. Mở **Device Manager** → **Ports (COM & LPT)**.
2. Cắm board vào, xem dòng nào mới xuất hiện, ví dụ `USB JTAG/serial debug unit (COM5)` hoặc `Silicon Labs CP210x (COM7)`.
3. Ghi lại số COM.

Board có 2 cổng USB-C: nếu một cổng không nhận, thử cổng còn lại.

### 4.3. Nạp code và xem log

```bash
idf.py -p COM5 flash monitor
```

(thay `COM5` bằng cổng của bạn)

- Thoát monitor: `Ctrl+]`
- Chỉ xem log, không nạp lại: `idf.py -p COM5 monitor`

### 4.4. Khi không nạp được: vào chế độ Download

Nếu báo `Failed to connect to ESP32-S3` hoặc `Wrong boot mode`:

1. **Nhấn giữ** nút **BOOT**.
2. **Nhấn và thả** nút **RST** (hoặc EN).
3. **Thả** nút **BOOT**.
4. Chạy lại lệnh `flash`.
5. Nạp xong, nhấn **RST** một lần để chạy chương trình mới.

### 4.5. Log khởi động bình thường

```
I (...) main: Desktop robot starting (ESP-IDF v5.5.x)
I (...) display: ST7789 240x240 rot=0 swap=0 mirror=0/0 gap=0,0 @40 MHz
I (...) animation: 0/9 animations from GIF, the rest use the built-in face
I (...) wifi: connecting to "TenWiFi"
I (...) wifi: connected, ip 192.168.1.23
I (...) weather: 31.4 C, 66%, code 80 (Rain showers), wind 9.7 km/h, 2026-11-12T14:30
I (...) app: 24.8 fps (avg render ...) | int ... KB | psram ... KB | IDLE / robot_idle [drawn]
```

Dòng `app: ... fps` được in 10 giây một lần. Đây là số đo **thực tế** trên board của bạn.

---

## 5. Lần chạy đầu tiên: căn chỉnh màn hình

Khi khởi động, màn hình hiện **4 dải màu trong 1.5 giây**, từ trên xuống: **đỏ – xanh lá – xanh dương – trắng**. Dựa vào đó để chỉnh trong **menuconfig → Desktop Robot → Display**:

| Bạn thấy | Cần chỉnh |
|---|---|
| Đúng thứ tự đỏ – xanh lá – xanh dương – trắng | Đã đúng, không cần chỉnh |
| Xanh dương – xanh lá – đỏ – trắng | Bật `DISPLAY_COLOR_ORDER: panel is BGR` |
| Màu bị âm bản (nền trắng, chữ đen…) | Đổi `Invert colors` |
| Hình bị lệch, có dải rác ở mép | Kiểm tra `Controller RAM rows` = 320, chỉnh `Extra X/Y offset` |
| Màn hình đen nhưng log vẫn chạy | Kiểm tra chân BL và VCC. Nếu màn không có CS thì đặt `SPI mode = 3`. Hạ `SPI clock` xuống 20 |
| Hình bị nhiễu, lấm tấm | Dây ngắn hơn, hoặc hạ `SPI clock` |
| Hình bị xoay | `DISPLAY_ROTATION`: 0 / 90 / 180 / 270 |

Khi màn hình đã hiển thị đúng, đặt `Boot color-bar test pattern duration` = **0** để bỏ bước hiện dải màu.

---

## 6. Thay đổi animation

Robot có **9 "khe" animation**. Mỗi khe dùng **file GIF** nếu có. Nếu thiếu file hoặc file hỏng, khe đó tự động dùng **khuôn mặt vẽ sẵn** (hai mắt màu xanh). Vì vậy project **luôn build được**, kể cả khi chưa có GIF nào.

| Khe (tên file) | Khi nào được phát |
|---|---|
| `robot_idle` | Trạng thái nghỉ (lặp liên tục) |
| `robot_blink` | Chớp mắt (phát 1 lần) |
| `robot_happy` | Biểu cảm vui |
| `robot_sleepy` | Buồn ngủ |
| `robot_surprised` | Ngạc nhiên |
| `robot_look_left` | Nhìn trái |
| `robot_look_right` | Nhìn phải |
| `robot_weather_sunny` | Vừa xem thời tiết xong và trời nắng (ban ngày) |
| `robot_weather_rain` | Vừa xem thời tiết xong và trời mưa / dông |

### 6.1. Thêm hoặc thay GIF (cách thông dụng nhất)

**Bước 1.** Cài Pillow (chỉ cần làm một lần):

```bash
pip install pillow
```

**Bước 2.** Chuyển đổi GIF nguồn vào đúng khe:

```bash
python tools/convert_gif.py D:\gif\mat_cuoi.gif --name robot_happy
python tools/convert_gif.py D:\gif\chop_mat.gif --name robot_blink --fps 20 --colors 64
```

Công cụ sẽ tự động:
- Thu về 240×240, giữ tỉ lệ, phần thừa tô đen.
- Giới hạn FPS nhưng giữ nguyên tổng thời lượng.
- Giảm số màu để file nhẹ và giải mã nhanh hơn.
- Ghi file vào `assets/gifs/robot_happy.gif`.

| Tham số | Mặc định | Ý nghĩa |
|---|---|---|
| `--name` | tên file nguồn | Khe animation (bắt buộc đúng 1 trong 9 tên ở trên) |
| `--fps` | 20 | FPS tối đa. Nên để 15–25 |
| `--colors` | 64 | Số màu (2–256). Ít màu thì nhẹ hơn và nhanh hơn |
| `--size` | 240 | Kích thước màn hình |
| `--bg` | `#000000` | Màu nền cho vùng trong suốt hoặc vùng đệm |

**Bước 3.** Kiểm tra các khe:

```bash
python tools/convert_gif.py --check
```

```
  robot_idle             240x240  24 frames     38 KB
  robot_blink            240x240   6 frames     11 KB
  robot_happy            missing  -> built-in drawn face
  ...
```

**Bước 4.** Build và nạp lại:

```bash
idf.py build
idf.py -p COM5 flash monitor
```

Trong log khởi động sẽ có dòng `animation: robot_happy: GIF loaded, ... KB` và `N/9 animations from GIF`.

> Nếu file GIF mới không được nhận, chạy `idf.py reconfigure` rồi build lại.

**Muốn xóa một GIF:** xóa file trong `assets/gifs/` rồi build lại. Khe đó sẽ quay về khuôn mặt vẽ sẵn.

**Lời khuyên khi chọn GIF:**
- Nền đen, nét đơn giản, ít màu: đẹp trên màn nhỏ và chạy mượt.
- `robot_idle` nên là vòng lặp liền mạch (khung cuối nối được với khung đầu).
- `robot_blink` nên ngắn (0.3–0.6 s). Robot sẽ chờ GIF chớp mắt chạy xong (tối đa 2 s) rồi mới chuyển trạng thái.
- Mỗi GIF chiếm khoảng **169 KB PSRAM** khi chạy. 9 GIF vẫn dư sức trong 8 MB.
- **Bản quyền:** chỉ dùng GIF do bạn tự làm hoặc có giấy phép cho phép phân phối lại.

### 6.2. Nhúng GIF vào firmware hay lưu trên SPIFFS?

**menuconfig → Desktop Robot → Features → GIF source**

| Lựa chọn | Đặc điểm |
|---|---|
| **Embedded** (mặc định) | GIF nằm trong file firmware (phân vùng app 7 MB). Đơn giản, chỉ cần build và flash |
| **SPIFFS** | GIF nằm ở phân vùng `storage` 8.9 MB. Lệnh `idf.py flash` tự đóng gói thư mục `assets/gifs` và nạp lên. Hợp khi có nhiều GIF lớn |

Cả hai chế độ đều dùng chung thư mục `assets/gifs/` và quy tắc đặt tên.

### 6.3. Tắt hẳn GIF, chỉ dùng khuôn mặt vẽ

**Features → ENABLE_GIF_ANIMATIONS** = tắt.

### 6.4. Sửa khuôn mặt vẽ sẵn (không cần GIF)

Mở [components/animation/face.c](components/animation/face.c), tìm bảng `SHAPES`:

```c
static const eye_shape_t SHAPES[ANIMATION_COUNT] = {
    //                       rộng cao  lệch_x lệch_y bo_góc  màu
    [ROBOT_IDLE]          = { 56, 72,   0,   0, 18, 0x00E5FF },
    [ROBOT_HAPPY]         = { 64, 26,   0, -10, 13, 0x00E5FF },
    [ROBOT_SLEEPY]        = { 60, 10,   0,  18,  5, 0x0088AA },
    ...
};
```

| Cột | Ý nghĩa |
|---|---|
| rộng, cao | Kích thước mỗi mắt (pixel) |
| lệch_x | Mắt dịch trái (âm) hoặc phải (dương). Dùng cho nhìn trái/phải |
| lệch_y | Mắt dịch lên (âm) hoặc xuống (dương) |
| bo_góc | Độ bo góc. Bằng một nửa cạnh nhỏ thì thành hình tròn hoặc viên thuốc |
| màu | Mã màu hex `0xRRGGBB` |

Các hằng số ở đầu file:

| Hằng số | Ý nghĩa |
|---|---|
| `EYE_GAP` | Khoảng cách từ tâm màn hình tới tâm mỗi mắt |
| `EYE_Y` | Độ cao của đôi mắt |
| `MORPH_MS` | Thời gian chuyển giữa hai biểu cảm |
| `BLINK_MS` | Tốc độ chớp mắt |
| `FLOAT_PX`, `FLOAT_MS` | Biên độ và chu kỳ "nhịp thở" (mắt nhấp nhô khi nghỉ) |

Ví dụ: làm mắt vui màu hồng thì đổi dòng `ROBOT_HAPPY` thành `{ 64, 26, 0, -10, 13, 0xFF6EC7 }`.

### 6.5. Thêm một biểu cảm hoàn toàn mới (nâng cao)

Ví dụ thêm biểu cảm `ROBOT_ANGRY`. Cần sửa đúng **6 chỗ**:

| # | File | Việc cần làm |
|---|---|---|
| 1 | [components/animation/include/animation.h](components/animation/include/animation.h) | Thêm `ROBOT_ANGRY,` vào `animation_id_t`, **trước** `ANIMATION_COUNT` |
| 2 | [components/animation/animation.c](components/animation/animation.c) | Thêm `"robot_angry"` vào mảng `NAMES`, **đúng vị trí tương ứng** với enum |
| 3 | [components/animation/face.c](components/animation/face.c) | Thêm dòng `[ROBOT_ANGRY] = { ... }` vào `SHAPES` (hình dạng dự phòng) |
| 4 | [components/animation/CMakeLists.txt](components/animation/CMakeLists.txt) | Thêm `robot_angry` vào danh sách `gif_names` |
| 5 | [tools/convert_gif.py](tools/convert_gif.py) | Thêm `"robot_angry"` vào danh sách `NAMES` |
| 6 | [components/app_controller/include/robot_fsm.h](components/app_controller/include/robot_fsm.h) và [robot_fsm.c](components/app_controller/robot_fsm.c), [app_controller.c](components/app_controller/app_controller.c) | Thêm trạng thái `RS_ANGRY` vào enum (**trước** `RS_COUNT`), thêm `"ANGRY"` vào mảng tên trong `robot_state_name()`, thêm `RS_ANGRY` vào mảng `pool[]` (các biểu cảm ngẫu nhiên). Trong `STATE_ANIM` của `app_controller.c` thêm `[RS_ANGRY] = ROBOT_ANGRY` |

Sau đó chạy `idf.py reconfigure && idf.py build`, rồi chạy host test (§10) để chắc chắn máy trạng thái vẫn đúng.

---

## 7. Tùy chỉnh hành vi robot

Tất cả đều nằm trong **menuconfig → Desktop Robot → Animation & screens**, không cần sửa code:

| Tùy chọn | Mặc định | Ý nghĩa |
|---|---|---|
| `ANIMATION_DEFAULT_FPS` | 25 | Giới hạn tốc độ làm mới màn hình |
| `ANIMATION_IDLE_DURATION` | 8000 ms | Nghỉ bao lâu thì đổi sang một biểu cảm ngẫu nhiên |
| `Expression duration` | 2500 ms | Mỗi biểu cảm kéo dài bao lâu |
| `Min / Max time between blinks` | 2000 / 6000 ms | Khoảng cách ngẫu nhiên giữa hai lần chớp mắt |
| `SCREEN_SWITCH_INTERVAL` | 30 s | Sau bao lâu thì chuyển sang màn thời tiết hoặc cảm biến |
| `How long weather/sensor screens stay` | 6 s | Màn thông tin hiển thị bao lâu |
| `Screen transition` | Fade | Hiệu ứng chuyển màn: mờ dần (Fade) hoặc trượt (Slide) |
| `Transition time` | 400 ms | Thời gian chuyển màn |

**Chu trình mặc định:**
1. Nghỉ (idle), thỉnh thoảng chớp mắt.
2. Sau 8 s nghỉ thì làm một biểu cảm ngẫu nhiên.
3. Cứ mỗi 30 s, hiện màn **thời tiết** hoặc **trong nhà** (luân phiên) trong 6 s.
4. Quay về mặt robot. Nếu vừa xem thời tiết, robot "phản ứng" bằng `robot_weather_sunny` hoặc `robot_weather_rain`.

**Ví dụ chỉnh nhanh:**
- Robot "lười" hơn: tăng `ANIMATION_IDLE_DURATION` lên 20000 và `Max time between blinks` lên 8000.
- Chỉ hiện mặt robot: tắt cả `ENABLE_WEATHER` và `ENABLE_SENSOR` (mục Features).

Muốn đổi logic sâu hơn (ví dụ biểu cảm theo nhiệt độ phòng), sửa hàm `robot_fsm_tick()` trong [robot_fsm.c](components/app_controller/robot_fsm.c) hoặc hàm `on_state()` trong [app_controller.c](components/app_controller/app_controller.c).

---

## 8. Tùy chỉnh giao diện

File [components/ui/ui.c](components/ui/ui.c):

- **Màu sắc:** các hằng số đầu file.

| Hằng số | Dùng cho |
|---|---|
| `COL_PANEL` | Nền màn thông tin |
| `COL_TEXT` | Chữ chính |
| `COL_DIM` | Chữ phụ |
| `COL_WARN` | Cảnh báo |
| `COL_TEMP` | Nhiệt độ |
| `COL_HUM` | Độ ẩm |

- **Vị trí chữ:** trong `ui_init()`, mỗi lệnh `make_label(..., LV_ALIGN_TOP_MID, x, y, ...)` có tọa độ `x, y` (pixel) tính từ điểm căn lề.
- **Font:** đang dùng Montserrat cỡ 14, 20 và 40. Muốn dùng cỡ khác (ví dụ 28):
  1. Bật trong `menuconfig → Component config → LVGL configuration → Font usage → Enable Montserrat 28`.
  2. Trong code, đổi thành `&lv_font_montserrat_28`.
- **Chữ tiếng Việt có dấu:** font Montserrat có sẵn **không có** các ký tự này. Cần tạo font riêng bằng [LVGL Font Converter](https://lvgl.io/tools/fontconverter), chọn dải ký tự tiếng Việt. Thêm file `.c` vào component `ui`, khai báo `LV_FONT_DECLARE(ten_font)` rồi dùng như font thường.
- **Biểu tượng thời tiết:** được vẽ bằng hình khối trong hàm `icon_set()` (mặt trời, mặt trăng, mây, giọt mưa, tuyết, sương mù).

---

## 9. Thời tiết và Wi-Fi

- **Đổi vị trí:** `menuconfig → Desktop Robot → Weather`.
  - `LATITUDE` / `LONGITUDE`: tọa độ dạng thập phân. Mặc định là TP.HCM (10.8231, 106.6297). Hà Nội là 21.0285, 105.8542.
  - Lấy tọa độ: trên Google Maps, nhấp chuột phải vào vị trí, dòng đầu tiên là tọa độ.
  - `Timezone`: để `auto`, hoặc ghi rõ `Asia/Ho_Chi_Minh`.
- **Chu kỳ cập nhật:** mặc định 15 phút. Open-Meteo miễn phí, không cần API key. Đừng đặt quá dày.
- **Mất mạng:**
  - Robot vẫn chạy bình thường. Màn thời tiết giữ số liệu cũ và ghi `- stale` khi dữ liệu quá 2 chu kỳ.
  - Robot tự kết nối lại với thời gian chờ tăng dần: 1, 2, 4, … đến tối đa 300 s (chỉnh ở `Max reconnect backoff`).
- **Chỉ hỗ trợ Wi-Fi 2.4 GHz.** Mạng 5 GHz sẽ không kết nối được.

---

## 10. Kiểm thử

### 10.1. Test logic trên máy tính (không cần board)

Kiểm tra các phần không phụ thuộc phần cứng:
- Xoay màn hình và offset
- Quy tắc chân GPIO
- Giải mã DHT11
- Đọc JSON thời tiết và bảng mã thời tiết
- Thời gian chờ khi kết nối lại
- Máy trạng thái hành vi
- Tra cứu file GIF

Cần một trình biên dịch C (gcc hoặc clang), ví dụ trong WSL hoặc Git Bash có gcc:

```bash
sh tests/host/run.sh
# hoặc chỉ định trình biên dịch:
CC=clang sh tests/host/run.sh
```

Kết quả mong đợi: `... passed, 0 failed`.

### 10.2. Self-test trên board

1. Bật `menuconfig → Desktop Robot → Diagnostics → Run on-target self-test`.
2. Build, nạp, mở monitor.
3. Sau khoảng 3 s, tìm các dòng `SELFTEST`:

```
SELFTEST switching: 180 switches, heap delta ... B -> PASS
SELFTEST invalid id rejected: PASS
SELFTEST wifi reconnect: PASS in ... ms
SELFTEST sensor: PASS (...)
```

4. Test xong thì **tắt lại** tùy chọn này.

### 10.3. Kiểm tra rò rỉ bộ nhớ khi chạy lâu

Để robot chạy vài giờ, theo dõi dòng log `app: ... int X KB (min Y, ...)`. Giá trị `min` phải **đứng yên**. Nếu nó giảm dần theo thời gian thì có rò rỉ bộ nhớ.

---

## 11. Xử lý sự cố

| Hiện tượng | Cách xử lý |
|---|---|
| Lỗi Component Manager về phiên bản `idf` (`>=5.3,<6.0`) | Bạn đang dùng ESP-IDF 6.x. Cài bản 5.5.x |
| `reconfigure` không tải được thư viện | Cần Internet cho lần đầu. Mạng công ty có proxy thì đặt biến `HTTPS_PROXY` |
| Đường dẫn có dấu cách hoặc tiếng Việt gây lỗi build | Chuyển project sang đường dẫn ngắn, không dấu, ví dụ `D:\esp\desktop_robot` |
| `Failed to connect to ESP32-S3` | Sai cổng COM, cáp chỉ sạc, hoặc cần vào chế độ Download (§4.4). Thử cổng USB-C còn lại |
| Log báo `GPIOxx ... reserved for octal flash/PSRAM` | Chuyển tín hiệu đó sang chân khác, ngoài dải GPIO26–37 |
| Board khởi động lại liên tục, báo lỗi PSRAM | Board không phải N16R8. Với board PSRAM quad (N8R2…): menuconfig → `SPI RAM config → Mode = Quad`, chỉnh lại dung lượng flash |
| Màn hình đen, màu sai, hình lệch | Xem §5 |
| `animation: robot_x: corrupt or unsupported GIF` | Chạy lại `tools/convert_gif.py` với file gốc. Trong lúc đó khe này dùng mặt vẽ sẵn |
| GIF mới không hiện | `idf.py reconfigure` rồi `idf.py build`. Kiểm tra tên file đúng chính tả bằng `convert_gif.py --check` |
| `sensor: read failed: timeout` | Kiểm tra dây DATA, nguồn 3V3 và điện trở kéo lên |
| `sensor: read failed: checksum` thỉnh thoảng | Bình thường với DHT11, robot giữ giá trị đúng gần nhất. Nếu bị liên tục thì rút ngắn dây |
| Màn trong nhà báo `Sensor STALE` | Không đọc được cảm biến quá 30 s (chỉnh ở `Mark reading stale after`) |
| Wi-Fi không kết nối | Sai SSID/mật khẩu, hoặc mạng 5 GHz. Log hiện `disconnected (reason N)` kèm mã lỗi |
| Thời tiết báo `HTTP 400` | Sai vĩ độ, kinh độ hoặc múi giờ |
| Thời tiết báo `connect failed` | Mạng chặn `api.open-meteo.com:443`, hoặc mất Internet |
| FPS thấp | Dùng GIF ít màu hơn (`--colors 32`), FPS nguồn thấp hơn. Nếu dây ngắn và gọn có thể thử SPI 60–80 MHz |
| Muốn build lại từ đầu | `idf.py fullclean` rồi `idf.py build` |
| Muốn xóa sạch flash (kể cả Wi-Fi đã lưu) | `idf.py -p COM5 erase-flash` rồi nạp lại |

---

## 12. Bảng tra cứu nhanh

```bash
# Lần đầu
idf.py set-target esp32s3
idf.py menuconfig                    # điền Wi-Fi
idf.py reconfigure

# Hằng ngày
idf.py build
idf.py -p COM5 flash monitor         # Ctrl+] để thoát
idf.py -p COM5 monitor               # chỉ xem log

# Animation
python tools/convert_gif.py file.gif --name robot_idle
python tools/convert_gif.py --check

# Kiểm thử
sh tests/host/run.sh

# Dọn dẹp
idf.py fullclean
idf.py -p COM5 erase-flash
```

| Muốn thay đổi | Ở đâu |
|---|---|
| Wi-Fi, chân GPIO, màn hình, thời gian, vị trí thời tiết | `idf.py menuconfig` → Desktop Robot |
| GIF animation | `assets/gifs/` + `tools/convert_gif.py` |
| Hình dạng / màu mắt vẽ sẵn | `components/animation/face.c` → `SHAPES` |
| Logic hành vi | `components/app_controller/robot_fsm.c`, `app_controller.c` |
| Màu, bố cục, font giao diện | `components/ui/ui.c` |
| Phân vùng flash | `partitions.csv` |
| Cấu hình mặc định (không chứa mật khẩu!) | `sdkconfig.defaults` |
