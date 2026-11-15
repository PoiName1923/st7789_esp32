# Thiết kế chẩn đoán Companion USB

## Mục đích

Sửa trường hợp lệnh điều khiển đã đến robot nhưng giao diện lại báo mất kết nối, đồng thời cung cấp chẩn đoán đủ rõ cho toàn bộ luồng nạp GIF.

## Phạm vi

- Tách kết quả gửi lệnh khỏi lần đọc trạng thái nền sau đó.
- Hiển thị nhật ký thao tác phía trình duyệt và nhật ký bridge/server trong tab Tổng quan.
- Lưu tối đa 100 sự kiện trên trình duyệt và 200 sự kiện tại bridge, chỉ trong phiên đang chạy.
- Ghi các giai đoạn GIF: nhận tệp, chuẩn hoá, bắt đầu gửi, hoàn tất hoặc lỗi.
- Không ghi mật khẩu Wi-Fi, token điều khiển, hoặc nội dung GIF vào log.
- Cập nhật hướng dẫn build và nạp firmware, bao gồm yêu cầu đóng Serial Monitor/ứng dụng đang giữ cổng USB.

## Ngoài phạm vi

- Không gửi log ra Internet hoặc lưu vĩnh viễn.
- Không thay đổi giao thức firmware hiện có.
- Không tự nạp lại firmware khi nạp GIF thất bại.

## Thiết kế

Bridge giữ một hàng đợi sự kiện có khóa. Mỗi API ghi sự kiện thành công hoặc lỗi đã được rút gọn, và `GET /api/log` trả về bản sao an toàn của hàng đợi. Upload GIF ghi riêng các mốc bắt đầu, dữ liệu đã chuẩn hoá và kết quả; ngoại lệ cổng USB cũng được trả về JSON thay vì làm đứt HTTP response.

Trang điều khiển có hai vùng log: sự kiện thao tác do trình duyệt ghi ngay khi người dùng bấm và nhật ký bridge được làm mới định kỳ. Một lệnh được xem là thành công ngay khi API lệnh trả về thành công. Việc làm mới trạng thái chạy sau đó, không được đổi kết quả lệnh thành lỗi; khi thất bại, trạng thái chỉ báo dữ liệu cũ hoặc lỗi đọc trạng thái.

## Tiêu chí hoàn thành

- Lệnh thành công vẫn hiện thành công khi lần đọc trạng thái sau đó thất bại.
- Người dùng xem được thời gian, nguồn, thao tác và chi tiết an toàn của ít nhất các lỗi kết nối và GIF.
- Lỗi upload qua `SerialException` luôn trả JSON có thông điệp thay vì lỗi parse JSON chung chung.
- Log không chứa giá trị mật khẩu Wi-Fi hay token.
- Tất cả kiểm thử Python hiện có và kiểm thử mới đều chạy thành công.
