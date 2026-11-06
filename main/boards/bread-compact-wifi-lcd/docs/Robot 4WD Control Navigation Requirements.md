# Robot 4WD — Technical Requirements & Development Plan

> [!important] Chiến lược triển khai
> **Ưu tiên 1: Motion Control chính xác và ổn định.** Chỉ chuyển sang Autonomous Navigation / Obstacle Avoidance sau khi hoàn thành và kiểm thử các chức năng điều khiển cơ bản. Kiến trúc phải cho phép mở rộng mà **không viết lại tầng điều khiển motor**.

## 1. Mục tiêu và phạm vi

### Giai đoạn A — Motion Control (ưu tiên cao)

Xe cần đạt được:

1. Điều khiển **tốc độ hai bên trái/phải** bằng Encoder feedback.
2. **Đi thẳng giữ hướng** bằng Encoder + IMU.
3. **Di chuyển quãng đường chỉ định** (ví dụ: đi thẳng 1 m, 2 m, 5 m khi không có vật cản).
4. **Quay một góc chỉ định** (ví dụ: ±90°).
5. Ước lượng chuyển động tương đối: `x`, `y`, `θ` qua **Wheel Odometry + Gyroscope**.
6. Dừng an toàn khi lỗi cảm biến / mất lệnh / điều khiển bất thường.

### Giai đoạn B — Autonomous Navigation (sau giai đoạn A)

Xe nhận lệnh **đi đến một vị trí đích**, ví dụ `goal = (5.0 m, 0.0 m)` trong hệ tọa độ bắt đầu. Nếu phát hiện vật cản, xe dừng → đánh giá đường tránh → đi vòng → tiếp tục đến **đích ban đầu**, không nhầm với tổng chiều dài đường đã đi.

**Ngoài phạm vi hiện tại:** SLAM, bản đồ toàn cục, LiDAR, ROS2 Navigation Stack. Có thể bổ sung trong các phiên bản sau.

**Cấu hình chuyển động:** 4WD **skid/differential steering** (điều khiển theo cặp trái/phải), **không** có 4 kênh điều khiển motor độc lập.

**Thông số cần đo/xác nhận:**

- `B_eff` — khoảng cách vệt bánh **hiệu dụng** dùng cho tính toán quay (phải hiệu chuẩn thực tế với skid-steer).
- `CPR_eff` — số counts/vòng **sau khi đã tính loại encoder và phương pháp đếm** (x1 / x2 / x4 nếu áp dụng).
- Chiều dương Encoder, chiều quay motor, giới hạn tốc độ / PWM, điện áp nguồn.
- Tổng dòng khi 2 motor cùng chạy trên một kênh L298N; tránh quá dòng và quá nhiệt.

## 4. Thuật toán điều khiển — ưu tiên triển khai

| Thành phần              | Lựa chọn                                | Ưu tiên                                  |
| ----------------------- | --------------------------------------- | ---------------------------------------- |
| Motor / side speed      | **PI Controller**                       | Bắt buộc                                 |
| Heading                 | **PD Controller**                       | Bắt buộc                                 |
| Control strategy        | **Cascaded Dual-Loop Closed-Loop**      | Bắt buộc                                 |
| Quãng đường / góc       | **Motion Profile + feedback**           | Bắt buộc                                 |
| Vị trí tương đối        | **Wheel Odometry + Gyro Heading**       | Bắt buộc                                 |
| Encoder speed filtering | Pulse-period hoặc cửa sổ đếm xung + lọc | Bắt buộc                                 |
| Sensor fusion nâng cao  | **EKF**                                 | Để sau khi hệ cơ bản ổn định             |
| Behavior                | **FSM**                                 | Giai đoạn B                              |
| Avoidance               | **Bug2 (phương án nghiên cứu)**         | Giai đoạn B; phụ thuộc cấu hình cảm biến |

### 4.1. Nguyên tắc Dual-Loop

1. Motion Controller đặt `v_ref` và `heading_ref`.
2. Heading PD tính `delta_v` từ sai lệch góc và vận tốc quay.
3. Bộ trộn tạo mục tiêu hai bên: `v_L_ref = v_ref - delta_v`, `v_R_ref = v_ref + delta_v` (quy ước dấu phải kiểm thử thực tế).
4. Hai Speed PI so sánh tốc độ thực tế từ Encoder → xuất PWM cho hai kênh L298N.
5. State Estimator liên tục cập nhật feedback cho các vòng điều khiển.

**Yêu cầu triển khai:** Giới hạn PWM, `anti-windup`, giới hạn gia tốc/giảm tốc, xử lý motor dead-zone, giới hạn `delta_v`, dừng có kiểm soát. Trường hợp quay tại chỗ cần cho phép `v_L_ref` và `v_R_ref` khác dấu.

### 4.2. Odometry — mô hình ban đầu

Với bán kính bánh `R = 0.03 m`, số counts/vòng thực tế `CPR_eff`:

```text
Wheel circumference = pi * 0.06 ≈ 0.1885 m
Distance per count = 0.1885 / CPR_eff (m)
Δs_L = Δcount_L * (2*pi*R / CPR_eff)
Δs_R = Δcount_R * (2*pi*R / CPR_eff)
Δs   = (Δs_R + Δs_L) / 2
Δθ_wheel ≈ (Δs_R - Δs_L) / B_eff
```

Lúc đầu dùng **gyro Z đã hiệu chuẩn bias** để cập nhật heading ngắn hạn, kết hợp Encoder tính quãng đường; sau đó tích phân vị trí `x, y` bằng góc hướng ước lượng. EKF chỉ bổ sung sau, nếu kết quả kiểm thử chứng minh cần thiết.

> [!warning] Độ chính xác
> Nếu `CPR_eff = 20`, độ phân giải lý thuyết là **9.42 mm/count**. Đây **không phải** độ chính xác vị trí thực tế. Bánh trượt, lệch đường kính, độ phân giải thấp và sai số gyro gây sai lệch tích lũy. HC-SR04/IMU/Encoder **không cung cấp định vị tuyệt đối**.

### 4.3. Chu kỳ tác vụ tham khảo (cần đo và tinh chỉnh)

| Task                    |             Tần số khởi điểm | Ghi chú                                                      |
| ----------------------- | ---------------------------: | ------------------------------------------------------------ |
| Encoder pulse counting  | Interrupt / hardware counter | Không bỏ xung khi CPU bận                                    |
| IMU read                |                      ~100 Hz | Timestamp từng mẫu                                           |
| Heading / Motion update |                    ~20–50 Hz | Bám theo tốc độ IMU và phản hồi                              |
| Speed PI                |                    ~10–20 Hz | Encoder 20 xung/vòng: tránh cửa sổ quá ngắn, tốc độ giật bậc |
| Odometry update         |                    ~20–50 Hz | Dùng `dt` thực, không mặc định cố định                       |
| Telemetry               |                     ~5–10 Hz | Không để Serial làm chậm vòng điều khiển                     |

Không coi các tần số này là thông số cố định trước khi đo xung thực và độ ổn định loop.

## 5. Kế hoạch thực hiện theo Phase

### **Phase 0 — Sensor & Hardware Validation** 🔴

**Công việc:**

- [x] Test Encoder trái/phải: chiều dương, số xung/vòng, mất xung, đo tốc độ.
- [x] Hiệu chuẩn gyro bias khi xe đứng yên.
- [x] Test riêng 2 kênh L298N và chiều quay; xác minh chịu dòng cho 2 motor/kênh.
- [x] Kiểm tra nguồn motor, nguồn logic, GND chung, dây tín hiệu, dừng khẩn.

### **Phase 1 — Closed-Loop Speed Control** 🔴

**Công việc:**

- [x] Đo tốc độ từng bên bằng Encoder và lọc tín hiệu.
- [x] Cài PI riêng cho bên trái và phải, `anti-windup`, saturation.
- [x] Hiệu chỉnh `Kp`, `Ki`, kiểm thử các mức tốc độ, thay đổi tải và đảo chiều.

**Done khi:** Xe bám tốc độ trái/phải ổn định, không dao động kéo dài; log được `target`, `measured`, `PWM` cho cả hai bên. **Ngưỡng sai số số học** đặt sau khi đo baseline.

### **Phase 2 — Heading Control / Dual-Loop** 🔴

**Công việc:**

- [x] Lấy heading tương đối từ gyro Z và kiểm soát drift.
- [x] Cài Heading PD tạo tốc độ mục tiêu hai bên.
- [x] Tích hợp Speed PI bên trong Heading PD.
- [x] Test chạy thẳng ở nhiều tốc độ, quay tại chỗ 90°/180°.

**Done khi:** Xe có thể giữ hướng và quay theo góc đặt, đánh giá bằng **đo thực tế độc lập** (thước, vạch sàn, video); không chỉ dựa vào số IMU.

### **Phase 3 — Distance / Pose Control** 🔴

**Mục tiêu:** Robot có khả năng di chuyển theo quãng đường, quay theo góc chỉ định và ước lượng vị trí tương đối `(x, y, θ)` bằng Encoder + IMU.

**Công việc:**
**1. Wheel Odometry**

- [x] Tính quãng đường di chuyển của bánh trái/phải từ Encoder.
- [x] Tính quãng đường trung bình `Δs` và góc quay `Δθ_wheel`.
- [x] Hiệu chuẩn `CPR_eff`, đường kính bánh xe và `B_eff`.

**2. Dead Reckoning**

- [x] Kết hợp Wheel Odometry + Gyroscope (MPU6050).
- [x] Xây dựng State Estimation để cập nhật vị trí `(x, y, θ)`.
- [x] Sử dụng vị trí ban đầu `(0, 0, 0°)` làm hệ tọa độ tham chiếu.
- [x] Cập nhật vị trí liên tục theo chuyển động của robot.

**3. Distance / Pose Control**

- [x] Cài đặt `move_distance(d)` để di chuyển quãng đường chỉ định.
- [x] Cài đặt `rotate_angle(a)` để quay theo góc chỉ định.
- [x] Sử dụng Motion Profile để giảm tốc trước khi đạt mục tiêu.
- [x] Sử dụng feedback từ State Estimation để kiểm soát chuyển động.

**4. Positional Uncertainty**

- [x] Đánh giá sai số giữa vị trí ước lượng và vị trí thực tế.
- [x] Đánh giá sai số tích lũy của Dead Reckoning khi quãng đường tăng.
- [x] Ghi nhận ảnh hưởng của Encoder, IMU và hiện tượng trượt bánh.
- [x] Thực hiện nhiều lần thử để đánh giá độ phân tán của sai số.

**5. Testing & Validation**

- [ ] Test di chuyển: `0.5 m → 1 m → 2 m → 5 m`.
- [ ] Test quay: `±90°`, `180°`.
- [ ] Test chuỗi chuyển động: `đi thẳng → dừng → quay → đi tiếp`.
- [ ] Đo vị trí thực tế độc lập để so sánh với Odometry.
- [ ] Ghi log: `x`, `y`, `θ`, `target_distance`, `estimated_distance`, `position_error`.

**Done khi:**

- Robot thực hiện được lệnh di chuyển theo quãng đường và góc quay chỉ định.
- State Estimation cập nhật được vị trí tương đối `(x, y, θ)`.
- Đã đo và báo cáo sai số vị trí qua nhiều lần thử nghiệm.
- Xác định được mức độ sai số tích lũy của Dead Reckoning.

> **Lưu ý:** Odometry và Dead Reckoning không cung cấp vị trí tuyệt đối. Positional Uncertainty được đánh giá bằng thực nghiệm trong Phase 3; mô hình hóa uncertainty bằng covariance và sử dụng EKF sẽ được xem xét ở Phase 4.

### Phase 4 — Sensor Fusion Enhancement 🟡 (tùy kết quả kiểm thử)

- [ ] So sánh Odometry thuần Encoder với Encoder + gyro.
- [ ] Nếu cần, triển khai **EKF** cho state estimation (`x, y, θ` hoặc state mở rộng).
- [ ] Đánh giá drift và độ ổn định trên tập bài thử giống Phase 3.

_EKF không tự tạo tham chiếu vị trí tuyệt đối và không bảo đảm hết drift._

### Phase 5 — Obstacle Detection & Safety 🟢

- [ ] Tích hợp HC-SR04; xác định vùng phát hiện an toàn và khoảng cách dừng theo tốc độ.
- [ ] Khi gặp vật cản: giảm tốc → dừng → chờ đánh giá, không tự lao về phía trước.
- [ ] Dữ liệu không hợp lệ hoặc không đo được khoảng cách → phản ứng bảo thủ.

### Phase 6 — Autonomous Navigation 🟢

- [ ] Thêm `BehaviorFSM`: `IDLE`, `NAVIGATING`, `STOPPED_FOR_OBSTACLE`, `SCANNING`, `AVOIDING`, `RETURNING_TO_PATH`, `GOAL_REACHED`, `BLOCKED`, `FAULT`.
- [ ] Thêm `Navigator` và goal tương đối `(x_goal, y_goal)`.
- [ ] Chọn / kiểm chứng thuật toán tránh vật cản trước khi triển khai. **Bug2 là ứng viên**, không bảo đảm khả thi khi chỉ có một HC-SR04 cố định phía trước.
- [ ] Nếu muốn né vật cản thực sự, cần **năng lực quan sát hai bên / theo biên**: HC-SR04 gắn servo quét hoặc bổ sung cảm biến phù hợp; khảo sát độ phủ và điểm mù.
- [ ] Sau khi tránh, tái lập hướng / lộ trình đến **tọa độ mục tiêu gốc**.
- [ ] Nếu không có đường tránh an toàn → `BLOCKED` và dừng.

**Done khi:** Trong môi trường thử đơn giản, xe nhận goal, phát hiện một vật cản, chọn đường vòng khả thi và tiếp tục đến vùng đích; có khả năng dừng an toàn khi không tìm được đường.

## 6. Quy trình thử nghiệm và dữ liệu bắt buộc

**Bài thử Control (không vật cản):**

| Test                  | Mục đích                    | Dữ liệu phải lưu                                   |
| --------------------- | --------------------------- | -------------------------------------------------- |
| T1: Tốc độ từng bên   | Đánh giá Speed PI           | `target_speed`, `measured_speed`, `PWM`, thời gian |
| T2: Đi thẳng 1/2/5 m  | Đánh giá Heading + Distance | Sai số lệch ngang, quãng đường, hướng cuối         |
| T3: Quay ±90°/180°    | Đánh giá Heading            | Góc đặt, góc đo độc lập, sai số                    |
| T4: Lặp lại ≥ 5 lần   | Đánh giá độ lặp             | Mean, Max error, độ phân tán                       |
| T5: Lỗi IMU / Encoder | Đánh giá an toàn            | Trạng thái lỗi, thời gian dừng                     |

**Acceptance criteria:** Đặt ngưỡng (cm, độ, %, thời gian đáp ứng) **sau khi có baseline T1–T4**, sau đó đóng băng ngưỡng trước vòng cải tiến tiếp theo. Không tự đặt cam kết ±1 cm hay ±1° khi chưa đo thực tế.

**Log gợi ý (CSV):**

```csv
time_ms,command,state,x_m,y_m,theta_rad,vL_ref_mps,vL_mps,vR_ref_mps,vR_mps,gyro_z_radps,pwmL,pwmR,error_flags
```

## 7. Quy tắc tổ chức source code

```text
src/
  hal/           motor_driver, encoder, imu, ultrasonic
  estimation/    encoder_speed, odometry, heading_estimator
  control/       pi_controller, heading_pd, motion_controller
  safety/        safety_manager
  navigation/    navigator, obstacle_avoidance      # Giai đoạn B
  behavior/      fsm                                # Giai đoạn B
  app/           robot_app, task_scheduler, config
```

**Nguyên tắc:**

1. HAL đọc / xuất tín hiệu phần cứng; **không** chứa thuật toán Navigation.
2. Estimation tính trạng thái; **không** ghi trực tiếp PWM.
3. Controllers chỉ nhận mục tiêu + feedback, xuất lệnh điều khiển.
4. FSM và Navigator gửi mục tiêu cho Motion Controller; **không** điều khiển chân L298N.
5. Safety Manager có quyền **ghi đè lệnh dừng** ở mọi trạng thái.
6. Thuật toán không dùng `delay()` chặn vòng điều khiển; dùng scheduler có timestamp.

## 8. Quyết định kỹ thuật đã chốt

| Hạng mục                   | Lựa chọn                                     |
| -------------------------- | -------------------------------------------- |
| Kiến trúc                  | **Hierarchical Layered Control**             |
| Control                    | **Cascaded Dual-Loop Closed-Loop**           |
| Speed                      | **PI (trái, phải)**                          |
| Heading                    | **PD**                                       |
| Estimation ban đầu         | **Encoder Odometry + Gyro**                  |
| Estimation nâng cao        | **EKF, sau khi đo baseline**                 |
| Behavior giai đoạn tự hành | **FSM**                                      |
| Goal                       | **Relative Waypoint Navigation**             |
| Obstacle Detection         | **HC-SR04**                                  |
| Obstacle Avoidance         | **Chưa chốt; đánh giá Bug2 khi đủ cảm biến** |
| Ưu tiên hiện tại           | **Phase 0 → 1 → 2 → 3**                      |

> [!success] Kết quả cần đạt trước tiên
> **Robot chạy thẳng ổn định, điều khiển được tốc độ, đi đúng quãng đường theo sai số đã kiểm chứng và quay đúng góc yêu cầu.** Sau milestone này mới triển khai HC-SR04 và tự hành né vật cản.
