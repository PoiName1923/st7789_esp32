# Thiết kế tab và phản hồi thao tác

## Mục đích

Biến trang điều khiển USB hiện tại thành giao diện thao tác nhanh, dễ nhận biết khu vực chức năng và luôn cho người dùng biết một lần bấm đã được tiếp nhận hay chưa.

## Phạm vi

- Thanh tab phía trên gồm: **Tổng quan**, **Màn hình**, **GIF**, **Thời tiết**, **Firmware**.
- Mỗi tab chỉ hiển thị các chức năng liên quan; trạng thái kết nối luôn hiện diện trong phần đầu.
- Nút có bốn trạng thái nhìn thấy được: nhấn, đang xử lý, thành công và thất bại. Trong khi gửi, nút bị khóa để ngăn bấm lặp.
- Thông báo chung có `aria-live`; tab hỗ trợ bàn phím và thuộc tính ARIA phù hợp.
- Trên màn hình hẹp, thanh tab cuộn ngang và các nút chiếm trọn chiều rộng.
- Tôn trọng `prefers-reduced-motion`; không dùng animation trang trí liên tục.

## Ngoài phạm vi

- Không thay đổi API, giao thức USB, firmware hay quy trình nạp.
- Không thêm framework frontend, icon pack hoặc phụ thuộc mới.
- Không biến luồng nạp firmware thành wizard nhiều bước.

## Hướng đã cân nhắc

- **Tab phía trên — đã chọn:** chuyển chức năng nhanh, phù hợp cả desktop và mobile.
- **Sidebar:** dễ quét trên desktop nhưng chiếm chỗ và phải có thêm menu mobile.
- **Wizard:** hữu ích cho thiết lập lần đầu nhưng làm chậm các thao tác điều khiển lặp lại.

## Cách triển khai

Giữ một tài liệu HTML/CSS/JavaScript thuần đang được nhúng trong companion. CSS đảm nhiệm chuyển động nhấn/trạng thái; JavaScript chỉ quản lý tab, khóa nút trong request và gắn phản hồi kết quả. Trạng thái tab hiện tại được giữ trong URL hash để tải lại vẫn ở đúng khu vực.

## Tiêu chí hoàn thành

- Chuyển tab không tải lại trang và không làm mất giá trị biểu mẫu.
- Mọi nút gửi lệnh đều phản hồi ngay khi bấm và không thể gửi trùng trong lúc chờ.
- Kết quả thành công/thất bại được thể hiện cả trên nút lẫn vùng thông báo.
- Giao diện dùng được ở chiều rộng 360 px và bằng bàn phím.
