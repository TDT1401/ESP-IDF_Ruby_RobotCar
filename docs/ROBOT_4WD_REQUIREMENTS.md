# Robot 4WD — Technical Requirements & Development Plan

> [!important] Chiến lược triển khai
> **Ưu tiên 1: Motion Control chính xác và ổn định.** Chỉ chuyển sang Autonomous Navigation / Obstacle Avoidance sau khi hoàn thành và kiểm thử các chức năng điều khiển cơ bản. Kiến trúc phải cho phép mở rộng mà **không viết lại tầng điều khiển motor**.

## 1. Mục tiêu và phạm vi

### Phần cứng

- Board ESP32-S3-N16R8.
- Khung xe 4WD và 4 động cơ DC.
- Driver motor L298N. kênh trái điều khiển 2 motor trái, kênh phải điều khiển 2 motor phải
- 2 cảm biến encoder FC-03.
- IMU MPU6050.
- Cảm biến siêu âm HC-SR04.
- Màn hình TFT ST7789 240×240.
- Microphone I2S.
- Mạch khuếch đại MAX98357A và loa.
- Nguồn cấp cho board và nguồn motor riêng.

### Giai đoạn A — Motion Control (ưu tiên cao)

Xe cần đạt được:

1. Điều khiển **tốc độ hai bên trái/phải** bằng Encoder feedback.
2. **Đi thẳng giữ hướng** bằng Encoder + IMU.
3. **Di chuyển quãng đường chỉ định** (ví dụ: đi thẳng 1 m, 2 m, 5 m khi không có vật cản).
4. **Quay một góc chỉ định** (ví dụ: ±90°).
5. Kết hợp Encoder + IMU để ước lượng `(x, y, θ)` và đi đến tọa độ tương đối `(x, y)` khi không có vật cản.
6. Dừng an toàn khi lỗi cảm biến / mất lệnh / điều khiển bất thường.

### Giai đoạn B — Autonomous Navigation (sau giai đoạn A)

Từ khả năng đi đến tọa độ tương đối ở Phase 5, xe xử lý vật cản bằng cách dừng → đánh giá đường tránh → đi vòng → tiếp tục đến **đích ban đầu**, không nhầm với tổng chiều dài đường đã đi.

**Ngoài phạm vi hiện tại:** SLAM, bản đồ toàn cục, LiDAR, ROS2 Navigation Stack. Có thể bổ sung trong các phiên bản sau.

**Cấu hình chuyển động:** 4WD **skid/differential steering** (điều khiển theo cặp trái/phải), **không** có 4 kênh điều khiển motor độc lập.

**Thông số cần đo/xác nhận:**

- `B_eff` — khoảng cách vệt bánh **hiệu dụng** dùng cho tính toán quay (phải hiệu chuẩn thực tế với skid-steer).
- `CPR_eff` — số counts/vòng **sau khi đã tính loại encoder và phương pháp đếm** (x1 / x2 / x4 nếu áp dụng).
- Chiều dương Encoder, chiều quay motor, giới hạn tốc độ / PWM, điện áp nguồn.
- Tổng dòng khi 2 motor cùng chạy trên một kênh L298N; tránh quá dòng và quá nhiệt.

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

### 2.1. Nguyên tắc Dual-Loop

1. Motion Controller đặt `v_ref` và `heading_ref`.
2. Heading PD tính `delta_v` từ sai lệch góc và vận tốc quay.
3. Bộ trộn tạo mục tiêu hai bên: `v_L_ref = v_ref - delta_v`, `v_R_ref = v_ref + delta_v` (quy ước dấu phải kiểm thử thực tế).
4. Hai Speed PI so sánh tốc độ thực tế từ Encoder → xuất PWM cho hai kênh L298N.
5. State Estimator liên tục cập nhật feedback cho các vòng điều khiển.

**Yêu cầu triển khai:** Giới hạn PWM, `anti-windup`, giới hạn gia tốc/giảm tốc, xử lý motor dead-zone, giới hạn `delta_v`, dừng có kiểm soát. Trường hợp quay tại chỗ cần cho phép `v_L_ref` và `v_R_ref` khác dấu.

### 2.2. Odometry — mô hình ban đầu

Với bán kính bánh `R = 0.03 m`, số counts/vòng thực tế `CPR_eff`:

```text
Wheel circumference = pi * 0.06 ≈ 0.1885 m
Distance per count = 0.1885 / CPR_eff (m)
Δs_L = Δcount_L * (2*pi*R / CPR_eff)
Δs_R = Δcount_R * (2*pi*R / CPR_eff)
Δs   = (Δs_R + Δs_L) / 2
Δθ_wheel ≈ (Δs_R - Δs_L) / B_eff
```

Lúc đầu dùng **gyro Z đã hiệu chuẩn bias** để cập nhật heading ngắn hạn, kết hợp Encoder tính quãng đường; sau đó tích phân vị trí `x, y` bằng góc hướng ước lượng. Phase 5 kiểm chứng EKF trước khi dùng pose hợp nhất cho điều hướng tọa độ.

> [!warning] Độ chính xác
> Nếu `CPR_eff = 20`, độ phân giải lý thuyết là **9.42 mm/count**. Đây **không phải** độ chính xác vị trí thực tế. Bánh trượt, lệch đường kính, độ phân giải thấp và sai số gyro gây sai lệch tích lũy. HC-SR04/IMU/Encoder **không cung cấp định vị tuyệt đối**.

## 3. Kế hoạch thực hiện theo Phase

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

- [x] Test di chuyển: `0.5 m → 1 m → 2 m → 5 m`.
- [x] Test quay: `±90°`, `180°`.
- [x] Test chuỗi chuyển động: `đi thẳng → dừng → quay → đi tiếp`.
- [x] Đo vị trí thực tế độc lập để so sánh với Odometry.
- [x] Ghi log: `x`, `y`, `θ`, `target_distance`, `estimated_distance`, `position_error`.

**Done khi:**

- Robot thực hiện được lệnh di chuyển theo quãng đường và góc quay chỉ định.
- State Estimation cập nhật được vị trí tương đối `(x, y, θ)`.
- Đã đo và báo cáo sai số vị trí qua nhiều lần thử nghiệm.
- Xác định được mức độ sai số tích lũy của Dead Reckoning.

### Phase 4 — Bám quỹ đạo thẳng khi tiến/lùi 🔴

**Trạng thái:** Chưa tìm được giải pháp cho vấn đề xe đi lệch quỹ đạo khi tiến/lùi.

### Phase 5 — EKF và điều hướng đến tọa độ (x, y) 🔴

**Mục tiêu:** Kết hợp Encoder + IMU để giảm sai số ước lượng `(x, y, θ)` và đưa xe đến tọa độ tương đối `(x_goal, y_goal)` trong hệ tọa độ lúc bắt đầu, khi không có vật cản.

- [x] Chạy EKF song song với odometry hiện tại; ghi pose, độ bất định và trạng thái hợp lệ.
- [x] So sánh sai số vị trí/góc của EKF và odometry bằng phép đo thực tế độc lập trên cùng bài chạy tiến, lùi và quay. Chỉ dùng pose EKF để điều khiển nếu kết quả tốt hơn và ổn định.
- [x] Thêm `Navigator` điều hướng đến `(x_goal, y_goal)` theo sai lệch tọa độ hiện tại, dùng bộ điều khiển tốc độ, hướng và quãng đường đã có. Dừng an toàn khi pose không hợp lệ, quá thời gian hoặc phát hiện vật cản.

**Done khi:** Phép đo độc lập cho thấy EKF giảm sai số so với odometry trên cùng bài thử; xe đến nhiều tọa độ đích, gồm đích có cả `x` và `y` khác 0, trong ngưỡng đã đặt và báo rõ khi không thể hoàn thành. Encoder + IMU không đo vị trí tuyệt đối hoặc trực tiếp phát hiện mọi lần trượt bánh.

### Phase 6 — Lắp phần cứng servo và HC-SR04 🔴

**Mục tiêu:** Gắn HC-SR04 lên servo, kiểm tra độc lập nguồn, dây tín hiệu và chuyển động của servo. Firmware tiếp tục đo khoảng cách phía trước; tích hợp quét servo với phép đo HC-SR04 thuộc Phase 7.

- [x] Chọn vị trí gắn và góc quét để cảm biến không bị thân xe che; ghi nhận vùng mù. Cấp nguồn servo phù hợp, nối GND chung; giữ mạch hạ mức tín hiệu ECHO của HC-SR04 xuống 3,3 V.
- [x] Kiểm tra servo độc lập trên GPIO6 và xác định vị trí giữa hướng thẳng phía trước. Firmware hiện tại chưa phát PWM trên GPIO6; không dùng góc quét làm dữ liệu an toàn khi chưa có phản hồi vị trí servo.
- [x] Kiểm tra riêng HC-SR04 trên GPIO14/GPIO39 và mạch chia áp ECHO. Chỉ dùng số đo phía trước khi cảm biến thực sự hướng về phía trước.

**Test:** Kiểm tra chuyển động servo và phép đo HC-SR04 riêng rẽ; thử trường hợp không có echo. Kiểm tra nguồn, IMU, encoder, âm thanh và motor khi servo hoạt động.

**Done khi:** Servo và HC-SR04 hoạt động độc lập, mạch ECHO đúng mức 3,3 V và đã ghi nhận vị trí lắp cùng vùng mù để tích hợp ở Phase 7.

### Phase 7 — Obstacle Detection & Safety 🟢

**Mục tiêu:** Phát hiện vật cản, dừng an toàn và quét môi trường để xác định hướng di chuyển khả thi.

**Công việc:**

- [ ] Mở lại chế độ né vật cản có sẵn và nâng cấp nó lên.
- [ ] Sử dụng HC-SR04 phát hiện vật cản phía trước.
- [ ] Khi gặp vật cản ở 35cm: giảm tốc → dừng hoàn toàn ở 25cm.
- [ ] xoay servo để quét trái, giữa, phải.
- [ ] Sử dụng `rotate_angle()` với Motion Profile.
- [ ] Lưu khoảng cách, góc thực tế từ IMU và thời điểm đo.
- [ ] Kiểm tra dữ liệu cảm biến và xác định các hướng có khả năng di chuyển an toàn.
- [ ] Nếu cảm biến gặp lỗi → `FAULT` và dừng.

**Done khi:**

- Robot phát hiện và dừng trước vật cản.
- Quét được các hướng và có thể rẽ theo hướng trống để đi.

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

**Acceptance criteria:** Đặt ngưỡng (cm, độ, %, thời gian đáp ứng) **sau khi có baseline T1–T4**, sau đó đóng băng ngưỡng trước vòng cải tiến tiếp theo. Không tự đặt cam kết ±1 cm hay ±1° khi chưa đo thực tế.
