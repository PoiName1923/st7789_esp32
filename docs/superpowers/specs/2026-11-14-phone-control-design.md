# Điều khiển Desktop Robot từ điện thoại

## Mục tiêu và lựa chọn đã chốt

Trang web responsive chạy trên ESP32-S3, mở bằng trình duyệt điện thoại. Dùng Wi-Fi gia đình để điều khiển; khi chưa có cấu hình hoặc không kết nối được, bot phát Wi-Fi cài đặt dự phòng. Không cần cloud hoặc ứng dụng điện thoại.

Hai mật khẩu độc lập: mật khẩu quản trị dùng đăng nhập web; mật khẩu điểm truy cập dùng kết nối Wi-Fi do bot phát. Mật khẩu Wi-Fi gia đình là thông tin thứ ba, dùng riêng để bot truy cập Internet. Không ghi mật khẩu vào log hoặc trả lại qua API.

## Phương án

- Web nội bộ trên ESP32: đã được chọn; tận dụng ESP-IDF và trình duyệt, dùng được trên Android/iOS.
- BLE với ứng dụng: cần thêm ứng dụng và quy trình cập nhật, không chọn.
- Cloud: hỗ trợ truy cập từ xa nhưng cần máy chủ và tài khoản, không chọn.

## Chức năng

- Chọn màn khuôn mặt, cảm biến, thời tiết hoặc chuyển tự động; chọn biểu cảm và animation có sẵn.
- Cài tốc độ animation, màu khuôn mặt vẽ sẵn, thời gian chuyển màn và thời gian hiện thông tin.
- Cài hướng xoay, màu RGB/BGR, đảo màu và offset màn hình; các thay đổi cần khởi động lại phải được báo trước.
- Độ sáng chỉ hỗ trợ nếu BL được nối tới GPIO phù hợp để PWM. Với cấu hình hiện tại BL nối 3V3, giao diện phải báo không hỗ trợ chỉnh độ sáng.
- Cài Wi-Fi gia đình, vị trí, múi giờ và chu kỳ cập nhật thời tiết; xem dữ liệu cảm biến, trạng thái mạng và lỗi gần nhất.
- Đổi riêng mật khẩu quản trị và mật khẩu Wi-Fi dự phòng. Lưu cài đặt trong NVS và khôi phục sau mất nguồn.

## Kết nối và bảo mật

- Lần đầu đặt hai mật khẩu riêng qua menuconfig, không có mật khẩu mặc định dùng chung. Wi-Fi dự phòng dùng mật khẩu ASCII 8–63 ký tự.
- Trang web dùng HTTPS nội bộ với chứng chỉ thiết bị; hướng dẫn người dùng xác nhận chứng chỉ tự ký trên điện thoại. Không truyền mật khẩu qua HTTP thuần.
- Mật khẩu quản trị lưu dạng băm có salt và hàm dẫn xuất khóa; đăng nhập tạo phiên ngẫu nhiên có thời hạn, cookie HttpOnly/SameSite/Secure, giới hạn lần đăng nhập và kiểm tra yêu cầu thay đổi trạng thái.
- Đính chính lời hứa trước: mật khẩu Wi-Fi phải có dạng có thể khôi phục để ESP32 cấu hình điểm phát sau khởi động, không thể chỉ lưu băm như mật khẩu quản trị. NVS thường không chống được trích xuất flash; bảo vệ trước truy cập vật lý cần NVS/flash encryption riêng.
- Khi đổi Wi-Fi hoặc mật khẩu điểm phát, trang web thông báo kết nối có thể ngắt và hướng dẫn kết nối lại. Nếu mạng gia đình lỗi, điểm phát dự phòng vẫn cho phép sửa cấu hình.

## Tổ chức triển khai

Dùng HTTP(S) server, Wi-Fi và NVS có sẵn của ESP-IDF; trang HTML/CSS/JavaScript nhúng trong firmware, không tải thư viện từ Internet. Một module settings quản lý kiểm tra dữ liệu và lưu trữ; web API gửi lệnh qua hàng đợi, chỉ luồng LVGL thay đổi giao diện bot. Wi-Fi và weather đọc cấu hình runtime thay vì chỉ dùng CONFIG tại build.

## Giới hạn

Không triển khai cloud, ứng dụng native, OTA hay tải GIF mới trong đợt này. Có thể chọn GIF đã nhúng; đổi màu chỉ áp dụng khuôn mặt vẽ sẵn. Không cam kết chỉnh độ sáng khi chưa đổi dây BL.

## Kiểm chứng

Kiểm thử xác thực, đầu vào không hợp lệ, lưu/đọc NVS, điều khiển qua hàng đợi và hồi quy logic hiện có. Build bằng ESP-IDF đang cài. Kiểm thử trên board: đăng nhập từ điện thoại, thay từng cấu hình, reset để kiểm tra lưu, Wi-Fi sai để kiểm tra dự phòng và đổi mật khẩu để kiểm tra phiên cũ bị vô hiệu hóa. Phân biệt rõ kiểm thử host và phần cần phần cứng.
