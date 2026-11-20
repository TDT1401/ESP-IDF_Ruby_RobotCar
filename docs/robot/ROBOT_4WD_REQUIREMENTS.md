# Robot 4WD — Low-level Controller Requirements & Development Plan

> **Ưu tiên Motion Control chính xác, ổn định trước khi mở rộng navigation/avoidance.** Thiết kế low-level theo [Hierarchical Layered Architecture](../ARCHITECTURE.md), dùng chung đường điều khiển motor.

## 1. Mục tiêu và phạm vi

### Phần cứng

- Board ESP32-S3-N16R8.
- Khung xe 4WD và 4 động cơ DC.
- Hai L298N, bốn kênh; mỗi kênh điều khiển một motor.
- Hai encoder FC-03 tại bánh trước trái/phải.
- IMU MPU6050.
- HC-SR04 gắn servo, hiện giữ hướng giữa.
- Microphone I2S.
- Mạch khuếch đại MAX98357A và loa.
- Nguồn cấp cho board và nguồn motor riêng.

### Giai đoạn A — Motion Control (ưu tiên cao)

1. Điều khiển **tốc độ hai bên trái/phải** bằng Encoder feedback.
2. **Đi thẳng giữ hướng** bằng Encoder + IMU.
3. **Di chuyển quãng đường chỉ định** khi không có vật cản.
4. **Quay một góc chỉ định** (ví dụ: ±90°).
5. Ước lượng `(x, y, θ)` từ Encoder + IMU và đi tới tọa độ tương đối `(x, y)` khi không có vật cản.
6. Dừng an toàn khi lỗi cảm biến / mất lệnh / điều khiển bất thường.

### Giai đoạn B — High level 

## 2. Thuật toán điều khiển — ưu tiên triển khai

| Thành phần              | Lựa chọn                                | Ưu tiên                                  |
| ----------------------- | --------------------------------------- | ---------------------------------------- |
| Motor / side speed      | **PI Controller**                       | Bắt buộc                                 |
| Heading                 | **PD Controller**                       | Bắt buộc                                 |
| Control strategy        | **Cascaded Dual-Loop Closed-Loop**      | Bắt buộc                                 |
| Quãng đường / góc       | **Motion Profile + feedback**           | Bắt buộc                                 |
| Vị trí tương đối        | **Wheel Odometry + Gyro Heading**       | Bắt buộc                                 |
| Encoder speed filtering | Pulse-period hoặc cửa sổ đếm xung + lọc | Bắt buộc                                 |
| Sensor fusion nâng cao  | **EKF từ Encoder + IMU**                | Phase 5; dùng sau khi kiểm chứng         |
| Behavior                | **FSM**                                 | Giai đoạn B                              |
| Avoidance               | **Bug2 (phương án nghiên cứu)**         | Giai đoạn B; phụ thuộc cấu hình cảm biến |

## 3. Kế hoạch thực hiện theo Phase

Checklist giữ theo ghi nhận cũ, không xác nhận thêm kết quả nghiệm thu.

### **Phase 0 — Sensor & Hardware Validation** 🔴

- [x] Kiểm tra encoder trái/phải, xung/vòng, chiều và tốc độ; hiệu chuẩn gyro bias.
- [x] Kiểm tra L298N, chiều motor, tải, nguồn/GND và dừng khẩn. Cần đối chiếu bài thử cũ với wiring bốn kênh hiện tại.

### **Phase 1 — Closed-Loop Speed Control** 🔴

- [x] Đo/lọc tốc độ encoder; cài Speed PI hai bên với `anti-windup` và saturation.
- [x] Tune `Kp/Ki`, thử nhiều tốc độ, tải và đảo chiều.

**Done khi:** Bám tốc độ ổn định; log `target/measured/PWM` hai bên, sai số đạt ngưỡng chốt sau baseline.

### **Phase 2 — Heading Control / Dual-Loop** 🔴

- [x] Lấy heading từ gyro Z, kiểm soát drift; tích hợp Heading PD với Speed PI.
- [x] Thử giữ hướng ở nhiều tốc độ và quay tại chỗ 90°/180°.

**Done khi:** Hướng/góc đạt ngưỡng, xác nhận bằng phép đo độc lập với IMU.

### **Phase 3 — Distance / Pose Control** 🔴

- [x] Tính odometry trái/phải, `Δs/Δθ`; hiệu chuẩn CPR, đường kính bánh và `B_eff`.
- [x] Kết hợp encoder + gyro, cập nhật liên tục pose `(x, y, θ)` từ gốc `(0, 0, 0°)`.
- [x] Cài `move_distance(d)`, `rotate_angle(a)` với Motion Profile và pose feedback.
- [x] Thử 0.5/1/2/5 m, ±90°/180° và chuỗi chuyển động; đo độc lập, đánh giá drift, trượt bánh và độ lặp; log pose, distance target/progress và sai số.

**Done khi:** Thực hiện được distance/angle goals, cập nhật pose và báo cáo sai số qua nhiều lần chạy.

### Phase 4 — EKF và điều hướng đến tọa độ (x, y) 🔴

- [x] Chạy EKF song song odometry, log pose/độ bất định/validity; so sánh bằng phép đo độc lập trên cùng bài tiến, lùi, quay.
- [x] Thêm `Navigator` tới `(x_goal, y_goal)` qua controller hiện có; dừng khi pose lỗi, timeout hoặc gặp vật cản.

**Done khi:** EKF tốt hơn và ổn định trước khi dùng điều khiển; tới nhiều đích có cả `x/y ≠ 0` trong ngưỡng, báo lỗi khi không hoàn thành. Encoder + IMU không phát hiện mọi lần trượt bánh.

### Phase 5 — Lắp phần cứng servo và HC-SR04 🔴

- [x] Lắp servo/HC-SR04, ghi vùng mù; nguồn đủ tải, GND chung và ECHO 3,3 V.
- [x] Kiểm tra servo GPIO6, HC-SR04 GPIO14/39 và hướng giữa. Source đã giữ giữa bằng PWM; chưa quét.

**Done khi:** Servo/cảm biến hoạt động độc lập, đúng hướng và mức điện áp; thử không echo và ảnh hưởng lên nguồn, IMU, encoder, audio, motor.

---

## 4. Quy trình thử nghiệm và dữ liệu bắt buộc

**Bài thử Control (không vật cản):**

| Test                  | Mục đích                    | Dữ liệu phải lưu                                   |
| --------------------- | --------------------------- | -------------------------------------------------- |
| T1: Tốc độ từng bên   | Đánh giá Speed PI           | `target_speed`, `measured_speed`, `PWM`, thời gian |
| T2: Đi thẳng 1/2/5 m  | Đánh giá Heading + Distance | Sai số lệch ngang, quãng đường, hướng cuối         |
| T3: Quay ±90°/180°    | Đánh giá Heading            | Góc đặt, góc đo độc lập, sai số                    |
| T4: Lặp lại ≥ 5 lần   | Đánh giá độ lặp             | Mean, Max error, độ phân tán                       |
| T5: Lỗi IMU / Encoder | Đánh giá an toàn            | Trạng thái lỗi, thời gian dừng                     |

**Acceptance criteria:** Chốt ngưỡng cm/độ/%/thời gian sau baseline T1–T4, trước vòng cải tiến tiếp theo. Không cam kết ±1 cm hay ±1° khi chưa đo thực tế.
