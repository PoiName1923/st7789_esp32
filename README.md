# Desktop Robot — ESP32-S3

Firmware ESP-IDF cho robot để bàn dùng ESP32-S3, màn hình ST7789 và cảm biến DHT11. Robot giữ một màn hình đã chọn cho đến khi bạn đổi bằng trang web điều khiển cục bộ qua USB.

## Chức năng

- Điều khiển qua web cục bộ, chỉ mở tại `127.0.0.1` trên máy tính đang cắm USB.
- Chọn màn hình: khuôn mặt, thời tiết hoặc cảm biến.
- Chọn biểu cảm, nhập/xóa GIF và phát GIF trực tiếp qua USB.
- Lưu Wi-Fi và vị trí để cập nhật thời tiết Open-Meteo.
- Build và nạp firmware từ ứng dụng desktop khi dùng bản phân phối đầy đủ.

Wi-Fi chỉ phục vụ dữ liệu thời tiết. Trang điều khiển không được mở qua Wi-Fi.

## Phần cứng và chân mặc định

| Thiết bị | ESP32-S3 |
|---|---:|
| ST7789 SCLK | GPIO12 |
| ST7789 MOSI | GPIO11 |
| ST7789 CS | GPIO10 |
| ST7789 DC | GPIO14 |
| ST7789 RST | GPIO13 |
| DHT11 DATA | GPIO4 |
| Nguồn màn hình/cảm biến | 3V3 và GND |

Bo mạch mục tiêu là **ESP32-S3 N16R8** (16 MB flash, 8 MB octal PSRAM). Không dùng GPIO26–37 vì chúng dành cho flash/PSRAM.

### DHT11 module 3 chân (`+`, `-`, `OUT`)

Sơ đồ mạch dùng **DHT11 loại module 3 chân**, không phải cảm biến DHT11 rời 4 chân. Đấu dây theo đúng nhãn in trên module:

| Chân DHT11 | Nối vào ESP32-S3 | Ghi chú |
|---|---|---|
| `+` | `3V3` | Không cấp 5V vào chân tín hiệu. |
| `-` | `GND` | Phải chung mass với ESP32-S3. |
| `OUT` | `GPIO4` | Dây dữ liệu một dây. |

Module 3 chân thông thường đã có điện trở kéo lên cho `OUT`. Nếu dùng DHT11 rời 4 chân, thêm điện trở kéo lên 4.7–10 kΩ từ chân DATA sang 3V3.

## Khởi động web điều khiển

### 1. Cắm đúng cổng USB

Để điều khiển, cắm cổng USB native của ESP32-S3. Trong Device Manager, cổng đúng thường là **Espressif USB Serial/JTAG** (`VID:PID 303A:1001`).

Cổng **CH340/CH343/CP210x** là UART: dùng để xem log hoặc cứu hộ khi nạp firmware, không phải cổng điều khiển web native.

### 2. Chạy ứng dụng companion

Nếu Windows không chặn EXE:

```powershell
.\companion\dist\DesktopRobotUSB.exe
```

Nếu Smart App Control chặn EXE, chạy trực tiếp từ mã nguồn:

```powershell
python -m pip install -r companion/requirements.txt
powershell -ExecutionPolicy Bypass -File companion/run_companion.ps1
```

Khi đã biết cổng native, chỉ định rõ cổng:

```powershell
powershell -ExecutionPolicy Bypass -File companion/run_companion.ps1 -Port COM8
```

Ứng dụng sẽ mở trình duyệt tại địa chỉ dạng `http://127.0.0.1:<cổng>/?token=...`. Không chia sẻ URL có token này.

### 3. Sử dụng giao diện web

- **Tổng quan:** kết nối lại robot sau khi rút/cắm USB.
- **Màn hình:** chọn màn hình hiển thị và biểu cảm.
- **GIF:** nạp GIF vào một trong 32 ô GIF hoặc xóa GIF đã nạp.
- **Thời tiết:** lưu Wi-Fi 2.4 GHz và tọa độ/thời gian cập nhật.
- **Firmware:** build hoặc build+nạp, nếu bản companion có `toolchain` và `firmware`.

Mỗi nút đổi màu và hiển thị trạng thái đang xử lý trong lúc gửi lệnh. Không đóng companion hoặc rút cáp khi đang nạp firmware.

## Nạp firmware

### Cách A — từ web companion

1. Mở companion và kết nối robot.
2. Mở tab **Firmware**.
3. Chọn **Build firmware** để chỉ build, hoặc **Build và nạp** để build và nạp.
4. Giữ cáp USB trong suốt quá trình. Ứng dụng sao lưu vùng GIF trước khi nạp và khôi phục lại sau đó.

Nếu tab Firmware báo thiếu toolchain, dùng Cách B hoặc đặt đầy đủ thư mục `toolchain/` và `firmware/` cạnh EXE theo bản phân phối portable.

### Cách B — dùng ESP-IDF Terminal

1. Cài ESP-IDF **5.5.x** và mở **ESP-IDF Terminal** tại thư mục dự án.
2. Dùng cổng COM mà Windows nhận diện.
3. Chạy:

```powershell
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
idf.py -p COM8 flash monitor
```

Đổi `COM8` thành cổng thực tế của bo mạch. Để thoát log monitor, nhấn `Ctrl+]`.

Nếu gặp `Failed to connect to ESP32-S3`:

1. Giữ nút **BOOT**.
2. Nhấn rồi thả **RST/EN**.
3. Thả **BOOT**.
4. Chạy lại lệnh `flash`.

Sau khi nạp, nhấn **RST** một lần để chạy firmware mới. Cổng native có thể biến mất vài giây trong lúc reset rồi xuất hiện lại với số COM khác.

## Build và cấu hình firmware

```powershell
idf.py set-target esp32s3
idf.py menuconfig
idf.py reconfigure
idf.py build
```

Các mục cấu hình dự án nằm dưới **Desktop Robot**. Không commit `sdkconfig` vì có thể chứa Wi-Fi hoặc cấu hình riêng của thiết bị.

## Nhật ký và lỗi GIF

Tab **Tổng quan** hiển thị hai nhật ký ngắn hạn: thao tác trên trình duyệt và phản hồi từ bridge USB. Khi nạp GIF, xem dòng `gif upload` để biết giai đoạn lỗi. Các nguyên nhân thường gặp là cáp/cổng USB đang bị ứng dụng khác giữ, firmware cũ chưa có API nạp GIF, hoặc GIF sau khi chuẩn hoá vượt 2 MB. Nhật ký không lưu mật khẩu Wi-Fi hoặc token truy cập.

Nếu bạn vừa cập nhật mã nguồn, hãy build và nạp lại firmware trước khi thử GIF. Đóng Serial Monitor, ESP-IDF monitor và companion đang chạy trước khi nạp; sau đó dùng cổng USB native của ESP32-S3:

```powershell
idf.py build
idf.py -p COM8 flash monitor
```

Thoát monitor bằng `Ctrl+]`, rút/cắm lại USB nếu COM native đổi số, rồi chạy `companion/run_companion.ps1` để mở web điều khiển.

## GIF từ mã nguồn

Để đóng gói GIF vào firmware thay vì nạp từ web:

```powershell
python -m pip install pillow
python tools/convert_gif.py D:\GIF\happy.gif --name robot_happy
python tools/convert_gif.py --check
idf.py build
```

GIF được lưu trong `assets/gifs/`. Khi không có GIF, robot dùng khuôn mặt vẽ sẵn.

## Kiểm thử

Kiểm thử Python của companion:

```powershell
$env:PYTHONPATH = "$PWD\companion\.vendor"
python -m unittest discover -v -s companion
```

Kiểm thử C trên máy chủ (cần shell có trình biên dịch C):

```bash
sh tests/host/run.sh
```

## Xử lý sự cố nhanh

| Hiện tượng | Cách xử lý |
|---|---|
| Không tìm thấy robot | Đóng các companion cũ, rút/cắm lại cáp, kiểm tra cổng Espressif USB Serial/JTAG rồi bấm Kết nối lại USB. |
| Nút web không hoạt động | Kiểm tra dòng trạng thái; chỉ chạy một companion để tránh tranh chấp cổng COM. |
| EXE bị Smart App Control chặn | Dùng `companion/run_companion.ps1`, hoặc ký số EXE trước khi phát hành. |
| Không nạp được firmware | Kiểm tra COM, cáp dữ liệu, vào Download mode bằng BOOT + RST. |
| Màn hình đen hoặc sai màu | Kiểm tra nguồn 3V3, BL, SPI; chỉnh RGB/BGR, offset và rotation trong `menuconfig`. |
| DHT11 timeout | Kiểm tra GPIO4, 3V3/GND và điện trở kéo lên 4.7–10 kΩ nếu dùng cảm biến rời. |

## Cấu trúc chính

```text
main/                 điểm vào firmware
components/           màn hình, animation, cảm biến, Wi-Fi, thời tiết, USB control
companion/            web bridge, nhập GIF, build/nạp firmware, EXE Windows
assets/gifs/          GIF đóng gói cùng firmware
tests/host/           kiểm thử C không cần phần cứng
```

Tài liệu chi tiết hơn về phần cứng, animation và chẩn đoán nằm trong [HUONG_DAN.md](HUONG_DAN.md).
