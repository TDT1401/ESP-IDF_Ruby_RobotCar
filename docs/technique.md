| Kỹ thuật | Thuật toán / Phương pháp | Mục đích |
|--------|--------|--------|
| **Motor Control** | PWM + H-Bridge (L298N) | Điều khiển tốc độ và chiều quay của 4 motor |
| **Speed Control** | PI + Feedforward | Điều chỉnh tốc độ bánh xe dựa trên encoder |
| **Heading Control** | PD Controller + Gyroscope | Giữ hướng khi chạy thẳng, điều khiển góc quay |
| **Control Architecture** | Cascaded Dual-Loop | Kết hợp vòng điều khiển tốc độ và hướng |
| **Distance Control** | Encoder Feedback + Motion Profile | Điều khiển quãng đường và quá trình tăng/giảm tốc |
| **Odometry** | Differential Drive Kinematics | Ước lượng vị trí `(x, y, θ)` |
| **Sensor Fusion** | Encoder + IMU, dự kiến EKF | Kết hợp dữ liệu để cải thiện ước lượng vị trí |
| **Real-Time Control** | FreeRTOS Periodic Tasks + Interrupt Notification + Multi-rate Control | Tổ chức các tác vụ điều khiển và đọc cảm biến |
| **Safety Control** | Timeout + Fault Detection | Dừng xe khi gặp lỗi hoặc mất lệnh |

## Cách xử lý Real-time

| Tiêu chí | Ruby RobotCar (low-level) |
|----------|-------------------------|
| Mô hình | Multi-task + Multi-rate Control (soft real-time) |
| Điều phối | FreeRTOS: 3 task low-level chính — motor, IMU và siêu âm |
| Chia sẻ dữ liệu | Mutex + snapshot; task notification và semaphore để đồng bộ |
| Tổ chức điều khiển | PI, PD và odometry/EKF dùng chung motor task; IMU và siêu âm có task riêng |

Chu kỳ danh định: motor và IMU 10 ms, siêu âm 60 ms; heading PD 50 ms và speed PI
100 ms trong motor task. FreeRTOS tạo nhịp chạy task; `esp_timer_get_time()` cung cấp
timestamp để tính thời gian thực tế và kiểm tra timeout. Độ trễ và jitter cần đo trên phần cứng.
