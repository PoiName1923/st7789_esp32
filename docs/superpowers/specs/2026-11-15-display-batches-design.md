# Thiết kế batch hiển thị

## Mục đích

Tách nội dung robot thành các batch hiển thị loại trừ nhau, để khi người dùng chọn một loại nội dung robot không tự chuyển sang loại khác.

## Hành vi đã chọn

- Nút **Khuôn mặt** chọn batch Biểu cảm: màn hình khuôn mặt luôn hiện; chỉ luân phiên Idle, Blink, Happy, Sleepy, Surprised, Look left và Look right. Không chuyển sang GIF, thời tiết hoặc cảm biến.
- Nút/phần chọn **GIF** chọn batch GIF: chỉ lặp GIF được chọn; không tự thay biểu cảm hay chuyển màn hình.
- Nút **Thời tiết** chọn batch Thời tiết: chỉ hiển thị và cập nhật dữ liệu thời tiết tại chỗ.
- Nút **Cảm biến** chọn batch Cảm biến: chỉ hiển thị và cập nhật dữ liệu cảm biến tại chỗ.
- Batch đang chọn và GIF hiện tại vẫn được lưu trong settings để khởi động lại giữ nguyên hành vi.

## Phạm vi

- Dùng `robot_fsm` hiện có để tạo chu kỳ biểu cảm chỉ khi batch Khuôn mặt hoạt động.
- Không thêm enum chế độ batch hoặc API mới; tái dùng `ROBOT_SCREEN_FACE`, `ROBOT_SCREEN_WEATHER`, `ROBOT_SCREEN_SENSOR` và lệnh chọn animation hiện có.
- Web đổi nhãn Khuôn mặt thành Biểu cảm liên tục; GIF vẫn gọi lệnh phát animation hiện có.

## Ngoài phạm vi

- Không tự chuyển tuần tự giữa batch.
- Không thêm lịch biểu, thời lượng riêng cho GIF, hoặc trộn nhiều GIF.
- Không đổi giao thức USB hay định dạng settings.

## Kiểm thử

- Host test xác nhận FSM biểu cảm không sinh `RS_WEATHER`/`RS_SENSOR` khi dùng batch Khuôn mặt.
- Kiểm thử controller xác nhận chọn Khuôn mặt bật FSM; chọn GIF/Thời tiết/Cảm biến dừng FSM.
- Kiểm tra thủ công: mỗi batch chạy độc lập và giữ nguyên sau reset.
