# USB Companion và trình dựng firmware cho Desktop Robot

## Ý định

Người dùng không cần mở mã nguồn hay dùng dòng lệnh. Một ứng dụng Windows kết nối bot qua USB, mở trang điều khiển cục bộ, nhập GIF, quản lý các màn hình và có thể build/nạp firmware. Bot giữ nguyên đúng một màn hình cho đến khi người dùng đổi trên web.

## Mục tiêu

- Điều khiển bot qua USB, không dùng Wi-Fi làm đường điều khiển.
- Đổi ngay giữa khuôn mặt/GIF, cảm biến và thời tiết.
- Giữ màn hình đã chọn qua khởi động lại; không tự luân phiên màn hình.
- Nhập, xem trước, đổi tên, xóa và nạp GIF mà không build lại firmware.
- Cho phép build và nạp firmware từ giao diện Windows bằng bộ công cụ đi kèm.
- Chạy được trên máy Windows mới mà không yêu cầu cài ESP-IDF, Python hay sửa code.

## Không thuộc phạm vi

- Điều khiển từ xa qua Internet, cloud hoặc ứng dụng điện thoại.
- Chỉnh sửa mã C trong ứng dụng.
- OTA; mọi thao tác firmware và kho GIF dùng cáp USB.
- Tạo/chỉnh sửa nội dung GIF ngoài các bước chuẩn hóa kích thước, tốc độ và bảng màu.
- Hỗ trợ macOS/Linux trong đợt đầu.

## Chống mục tiêu

- Không build lại toàn bộ firmware chỉ vì thêm một GIF.
- Không yêu cầu người dùng biết COM port, `idf.py`, phân vùng flash hay `sdkconfig`.
- Không biến firmware thành web server qua Wi-Fi thứ hai song song với USB.
- Không tuyên bố một file EXE vài MB chứa được toolchain ESP-IDF. Bản phát hành là bộ cài Windows gồm ứng dụng và payload công cụ portable; sau cài đặt người dùng chỉ chạy ứng dụng.

## Ràng buộc hiện có

- Phần cứng là ESP32-S3 N16R8, flash 16 MB, PSRAM 8 MB, màn hình 240×240.
- Bảng phân vùng hiện có app 7 MB và `storage` SPIFFS khoảng 8,9 MB; kho GIF sẽ dùng phân vùng `storage`.
- Firmware đã có màn hình face/weather/sensor, NVS settings, hàng đợi lệnh LVGL, web UI và bộ chuyển GIF bằng Pillow.
- Chỉ luồng LVGL được phép thay đổi giao diện bot.
- USB native/JTAG-Serial hoặc cổng UART bridge có thể xuất hiện dưới dạng COM; nạp flash có thể làm cổng ngắt và xuất hiện lại.
- Wi-Fi vẫn được phép dùng riêng để lấy thời tiết. Nếu không cấu hình Wi-Fi, màn hình cảm biến và GIF vẫn hoạt động ngoại tuyến.

## Các phương án đã cân nhắc

1. Build toàn bộ firmware cho mọi thay đổi: đơn giản về mô hình nhưng chậm, hao flash và không phù hợp khi chỉ thêm GIF.
2. Firmware cố định, chỉ nạp kho GIF: nhanh và nhỏ nhưng không đáp ứng yêu cầu build/cập nhật firmware từ ứng dụng.
3. Kết hợp: kho GIF được nạp riêng; firmware chỉ build/nạp khi cần. **Đã chọn** vì thao tác thường ngày nhanh nhưng vẫn cho phép cập nhật firmware hoàn toàn từ ứng dụng.

## Kiến trúc đã chọn

### 1. Ứng dụng Windows Companion

Ứng dụng desktop .NET 8 tự chứa runtime, ưu tiên WPF để dùng thành phần có sẵn của Windows và giảm phụ thuộc. Bản phát hành dùng installer, cài:

- `DesktopRobot.exe`;
- mã nguồn firmware phiên bản tương ứng;
- ESP-IDF 5.5.x, Python và các tool/package cần thiết ở dạng portable;
- `esptool`, công cụ tạo SPIFFS và bộ chuyển GIF;
- firmware dựng sẵn tương ứng với phiên bản ứng dụng.

Ứng dụng có bốn vùng chức năng: **Kết nối**, **Điều khiển**, **Thư viện GIF**, **Firmware**. Tác vụ dài hiển thị tiến độ và log ngắn có thể sao chép; nút hủy chỉ được bật ở giai đoạn an toàn.

Ứng dụng tự dò COM bằng VID/PID và thử bắt tay giao thức. Nếu có nhiều bot, người dùng chọn từ danh sách tên/serial; không chọn COM thủ công trừ mục khắc phục sự cố. Khi flash làm cổng xuất hiện lại, ứng dụng chờ và kết nối lại theo serial thiết bị.

### 2. Trang điều khiển qua `localhost`

Companion bind HTTP chỉ trên `127.0.0.1` với một cổng khả dụng, phục vụ lại HTML/CSS/JS hiện có và chuyển API sang USB Serial. Ứng dụng mở trình duyệt mặc định tới URL có token phiên ngẫu nhiên; token được đổi mỗi lần chạy. Không lắng nghe LAN và không ghi bí mật vào log.

API phía trình duyệt giữ mô hình hiện tại: đọc trạng thái, liệt kê màn hình/GIF và gửi lệnh. Lệnh đổi màn hình trả kết quả sau khi bot xác nhận đã áp dụng, nhờ đó giao diện không báo thành công giả khi USB mất kết nối.

### 3. Giao thức USB runtime

Firmware bổ sung task USB/UART nhỏ dùng khung JSON theo từng dòng, có giới hạn kích thước và mã yêu cầu:

- `hello`: phiên bản giao thức, firmware, chip ID và khả năng;
- `get_status`, `get_settings`, `list_screens`;
- `show_screen`, `show_gif`, `set_settings`;
- phản hồi `{id, ok, result|error}`.

Parser chỉ tạo lệnh đã kiểm tra và đẩy vào hàng đợi `app_controller`; task USB không gọi LVGL trực tiếp. Giao thức có phiên bản ngay từ đầu. Chỉ một yêu cầu thay đổi trạng thái được xử lý tại một thời điểm; thông báo cảm biến/trạng thái có thể được đẩy bất đồng bộ.

### 4. Hành vi màn hình đơn

Chế độ tự luân phiên/FSM không còn là mặc định runtime. NVS lưu một lựa chọn hiện hành gồm loại `face`, `gif`, `sensor` hoặc `weather` và ID nội dung nếu cần. Sau boot, bot tải lựa chọn hợp lệ gần nhất; nếu GIF đã bị xóa/hỏng thì về khuôn mặt dựng sẵn.

`show_screen` và `show_gif` được áp dụng trong LVGL context, dừng/ẩn nội dung trước rồi hiện nội dung mới. Bot giữ màn hình đó vô thời hạn cho đến lệnh tiếp theo. GIF lặp liên tục; màn hình thông tin chỉ cập nhật dữ liệu bên trong, không tự chuyển trang.

### 5. Kho GIF độc lập

Companion nhận GIF nguồn, dùng pipeline hiện có để fit/pad 240×240, giới hạn FPS và giảm bảng màu. Trước khi nạp, ứng dụng hiển thị bản xem trước, kích thước sau chuyển đổi và dung lượng còn lại.

Phân vùng `storage` chứa:

- manifest có phiên bản, ID ổn định, tên hiển thị, tên file, checksum và metadata;
- các GIF đã chuẩn hóa.

Danh mục không bị khóa vào chín slot hiện tại. Giới hạn thực tế là dung lượng phân vùng; để giữ RAM ổn định, firmware chỉ tạo decoder/buffer cho GIF đang chạy thay vì nạp mọi GIF lúc boot. Manifest và file mới được dựng thành ảnh SPIFFS tạm trên PC rồi nạp riêng đúng offset `storage`. Trước khi ghi, Companion giữ bản sao thư viện trên PC và xác minh checksum ảnh; nếu thao tác lỗi, cho phép nạp lại từ bản sao.

Bot là nguồn chính của thư viện. Khi kết nối, Companion có thể đọc manifest và tải từng GIF theo khối qua USB, kiểm tra checksum rồi lưu cache theo chip ID. Trước mọi lần full-flash, ứng dụng bắt buộc đồng bộ toàn bộ thư viện từ bot; nếu không đọc và xác minh được bản sao thì dừng trước bước xóa/ghi flash. Vì vậy một máy tính mới vẫn bảo toàn được GIF đã có trên bot. Sau khi firmware khởi động lại, Companion dựng/nạp lại storage từ bản sao và đối chiếu manifest/checksum.

Thêm/xóa/đổi tên GIF không build app firmware. Nếu thay đổi bảng phân vùng, Companion bắt buộc dùng quy trình nạp firmware đầy đủ để tránh ghi sai offset.

### 6. Build và nạp firmware

Màn hình Firmware có hai đường:

- **Nạp bản ổn định**: dùng binary dựng sẵn, nhanh và là lựa chọn mặc định.
- **Build rồi nạp**: giao diện tạo một overlay cấu hình được phép thay đổi, chạy ESP-IDF portable trong tiến trình con, stream tiến độ, rồi nạp bootloader/partition table/app/storage bằng `esptool`.

Chỉ các tùy chọn phần cứng và firmware đã công bố được hiển thị; không có trình sửa `sdkconfig` tự do. Build chạy trong thư mục cache theo phiên bản ứng dụng và hash cấu hình. Cache có thể xóa từ Settings. Toolchain không tải ngầm trong lần chạy đầu vì đã nằm trong installer; tải Internet chỉ dành cho cập nhật do người dùng chủ động chọn.

Bản đầu hiển thị các cấu hình cần build: chân và bus màn hình, kích thước/rotation/order/invert/offset, chân cảm biến, bật/tắt sensor/weather, SPI clock, flash/PSRAM theo các profile board được hỗ trợ và diagnostics. Wi-Fi, vị trí thời tiết, màn hình/GIF hiện hành, màu và timing là cấu hình runtime, không kích hoạt build. Bảng phân vùng không được sửa tự do; chỉ profile N16R8 đã kiểm chứng được phép dùng trong bản đầu.

Trước full flash, Companion kiểm tra đúng ESP32-S3 và kích thước flash, dựng lại ảnh storage từ thư viện PC, và không xóa NVS trừ khi người dùng chọn “khôi phục cài đặt gốc”. Sau flash, ứng dụng chờ bot khởi động, bắt tay lại, kiểm tra phiên bản và phục hồi kho GIF nếu cần.

### 7. Wi-Fi và thời tiết

Đường điều khiển Wi-Fi/AP và HTTPS trên bot được tắt trong cấu hình sản phẩm USB để giảm bề mặt lỗi. Wi-Fi STA vẫn là tùy chọn chỉ để gọi Open-Meteo. Cấu hình Wi-Fi/thời tiết được gửi qua USB, lưu NVS và không bao giờ trả lại mật khẩu. Không có Wi-Fi vẫn dùng được toàn bộ điều khiển USB, GIF và cảm biến.

### 8. Cập nhật và tương thích

Companion, giao thức USB, manifest GIF và firmware đều có version. Ứng dụng từ chối thao tác ghi nếu major protocol không tương thích nhưng vẫn cho phép vào màn hình Firmware để nạp bản phù hợp. Mỗi release ghép đúng firmware source, prebuilt binary và toolchain metadata; không tự trộn các phiên bản.

Bản đầu hỗ trợ Windows 10/11 x64. Installer có thể yêu cầu Administrator khi cần cài driver USB, còn ứng dụng và build cache chạy bằng quyền người dùng thường. Không có auto-update trong bản đầu; phiên bản mới được phân phối bằng installer đã ký và do người dùng chủ động chạy.

USB native là đường chính cho điều khiển runtime và flash. UART bridge được hỗ trợ như đường flash/cứu hộ; chỉ bật runtime control trên UART khi firmware xác nhận cổng log riêng hoặc framing không bị boot log làm nhiễu. Ứng dụng luôn ưu tiên thiết bị có chip ID đã bắt tay thay vì chỉ dựa vào tên COM.

## Xử lý lỗi và an toàn

- Mất USB khi điều khiển: web báo mất kết nối, bot tiếp tục màn hình hiện tại.
- Mất USB khi nạp GIF: firmware cũ vẫn chạy; storage có thể cần nạp lại từ bản sao PC.
- Mất USB/nguồn khi nạp app: Companion hướng dẫn BOOT + RST và cho nạp lại; không hứa phục hồi tự động nếu bootloader không chạy.
- GIF sai/không hỗ trợ: chặn trước khi ghi; firmware vẫn kiểm tra header/checksum và fallback.
- Build lỗi: giữ log, không bắt đầu flash.
- Không cho hai instance Companion cùng chiếm một bot.

## Kiểm chứng

### Tự động trên máy phát triển

- Unit test parser/framing USB, ID matching, timeout và giới hạn body.
- Unit test trạng thái “một màn hình”, persistence và fallback khi GIF mất/hỏng.
- Test manifest, tên trùng, checksum, giới hạn dung lượng và ảnh SPIFFS.
- Test API localhost chỉ bind loopback và yêu cầu token.
- Test command orchestration bằng fake serial/esptool: reconnect, build fail, flash fail và thành công.
- Build firmware và build/publish ứng dụng Windows trong CI từ môi trường sạch.

### Cần phần cứng thật

- Dò cả USB native và UART bridge; nhiều bot cùng lúc.
- Đổi liên tục giữa face/GIF/sensor/weather và reset để kiểm tra persistence.
- Nhập nhiều GIF, nạp storage, xóa GIF đang chọn và kiểm tra fallback.
- Rút cáp trong lúc điều khiển, nạp storage và full flash; chạy quy trình phục hồi.
- Build/nạp trên máy Windows chưa từng cài ESP-IDF/Python.
- Xác nhận Wi-Fi không mở web/AP nhưng weather STA vẫn hoạt động.

## Các quyết định giao cho người triển khai

- Bộ VID/PID và chiến lược nhận diện chính xác sau khi kiểm tra hai cổng trên board thật.
- Định dạng manifest cụ thể (JSON hoặc binary đơn giản) miễn có version/checksum và parser bị chặn kích thước.
- Công cụ tạo SPIFFS cụ thể tương thích ESP-IDF 5.5.x.
- Thiết kế chi tiết từng control WPF, miễn giữ bốn vùng chức năng và các trạng thái lỗi/tiến độ nêu trên.

