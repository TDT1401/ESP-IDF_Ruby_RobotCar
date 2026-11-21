Phần low-level có nền tảng để giữ lại, nhưng chưa sẵn sàng làm bộ điều khiển độc lập cho Raspberry Pi. Điểm cần cải thiện nằm ở đường nhận lệnh, đảo chiều và bảo vệ feedback; chưa cần viết lại motor driver hay Speed PI.

Tôi thấy các điểm sau, theo thứ tự ưu tiên:

2. Khi mất xung encoder, PI có thể tiếp tục tăng PWM trước khi dừng

[wheel_encoder.cc (line 191)](C:/Users/ASUS/Documents/PROJECT/Robotics/ESP-IDF_Ruby_RobotCar/components/wheel_encoder/wheel_encoder.cc:191) đặt no_pulses sau 2 giây. Trong khoảng đó, nếu RPM giảm về 0 nhưng mục tiêu vẫn khác 0, PI có thể tăng PWM để bù.

Controller có xử lý dừng khi nhận no_pulses, nhưng cần xem lại thời gian phát hiện và phân biệt giai đoạn khởi động với mất feedback khi đang chạy. Không nên chỉ giảm timeout tùy ý vì ở tốc độ thấp, xung vốn thưa.

3. Chưa có API tốc độ hai bên độc lập

[API hiện tại (line 94)](C:/Users/ASUS/Documents/PROJECT/Robotics/ESP-IDF_Ruby_RobotCar/components/motor_controller/include/motor_controller.h:94) vẫn dựa trên hướng, % tốc độ và các goal.

SetRequestedWheelsLocked() cũng chưa phải API có thể đưa thẳng cho Pi: nó xử lý yêu cầu tốc độ/PWM; chiều quay đang được xử lý ở đường khác.

Low-level nên cung cấp một API rõ ràng, chẳng hạn:
SetWheelSpeeds(left_rpm, right_rpm);
Stop();
GetFeedback();
API này phải xử lý cả dấu, giới hạn tốc độ, tăng/giảm tốc và đảo chiều. Nếu chạy diff_drive_controller trên Pi, việc đổi vận tốc thân xe sang tốc độ bánh đã thuộc controller phía ROS. Tài liệu ROS 2 Control

4. Quyền chạy và timeout còn gắn với web/Wi-Fi

[StartCommand() (line 47)](C:/Users/ASUS/Documents/PROJECT/Robotics/ESP-IDF_Ruby_RobotCar/components/motor_controller/motor_controller.cc:47) yêu cầu web_control_.connected(). Cơ chế hết hạn lệnh liên tục hiện thuộc web session.

Khi nối Pi bằng serial, cần một deadline nhận lệnh thuộc low-level, độc lập với web và Wi-Fi. Pi mất kết nối hoặc ngừng gửi lệnh thì ESP32 phải tự dừng. Trong giai đoạn còn cả web và Pi, cần bảo đảm chỉ một nguồn có quyền điều khiển.

5. safety/ hiện chưa bảo vệ chung mọi chuyển động

[sensor_behavior.cc (line 12)](C:/Users/ASUS/Documents/PROJECT/Robotics/ESP-IDF_Ruby_RobotCar/components/motor_controller/safety/sensor_behavior.cc:12) chủ yếu phát cảnh báo khi web manual tiến. Nhánh dừng vì vật cản nằm riêng trong navigation.

Nếu muốn ESP32 giữ quyền dừng khẩn cấp theo khoảng cách, chính sách đó phải áp dụng được cho lệnh từ Pi, đồng thời quy định rõ cách xử lý mẫu ultrasonic lỗi/cũ.

6. Feedback encoder có giới hạn cần tính đến

Encoder hiện suy chiều từ lệnh, không đo trực tiếp chiều quay; signed_distance_m đã ghi rõ điều này trong [wheel_encoder.h (line 28)](C:/Users/ASUS/Documents/PROJECT/Robotics/ESP-IDF_Ruby_RobotCar/components/wheel_encoder/include/wheel_encoder.h:28).

Với khoảng 20 xung/vòng và bánh 65 mm, mỗi xung tương ứng khoảng 10 mm. Ở 20 RPM, khoảng cách giữa hai xung khoảng 150 ms. Vì vậy, task chạy mỗi 10 ms không đồng nghĩa feedback tốc độ có độ phân giải tương ứng. Chất lượng chạy chậm và đảo chiều cần được đo thực tế.

Những phần hiện làm hợp lý và nên giữ:
- motor_driver chỉ phụ trách GPIO/LEDC; Stop() cố dừng đủ các motor dù có thao tác lỗi.
- Speed PI tách trái/phải, có feedforward, giới hạn output và chống integral windup.
- Task motor riêng, IMU lấy mẫu riêng; các truy cập controller được bảo vệ bằng mutex.
- Lỗi phần cứng được giữ lại, không tự chạy tiếp.
Tôi đề xuất ưu tiên chuẩn hóa API tốc độ bánh + timeout độc lập, sau đó thống nhất đảo chiều và xử lý mất feedback. Ba chức năng cấp cao có thể tiếp tục dùng API đó trong thời gian chưa có Pi, rồi được gỡ sau.

Không sửa file. Chưa build/test (theo yêu cầu); độ ổn định PI, quãng đường dừng và độ chính xác odometry vẫn cần kiểm chứng trên xe.