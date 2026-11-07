# RobotCar – Dữ liệu kiểm thử di chuyển và xoay

## 1. Mục đích

Ghi lại kết quả **18 lần kiểm thử di chuyển thẳng** và **12 lần kiểm thử xoay tại chỗ** (6 lần xoay 90° và 6 lần xoay 180°), cùng các quan sát thực tế về hành vi của xe.

> **Quy ước:** Các giá trị được giữ nguyên theo ghi chép ban đầu. Cột **Sai số ghi nhận** chưa được xác định thống nhất là sai số vị trí, sai số khoảng cách hay độ lệch ngang; không tự mặc định đây đều là sai số của Odometry. Dấu `—` nghĩa là không có dữ liệu được cung cấp. Lưu ý về hướng. khi xe tiến hướng quan sát là từ dưới xe nhìn lên nên hướng (trái/phải) sẽ khác với khi xe lùi. khi lùi hướng quan sát từ đầu xe nên hướng (trái/phải) sẽ khác

## 2. Kiểm thử di chuyển thẳng

### 2.1. Quãng đường 50 cm

| Lần thử | Chuyển động | Quãng đường mục tiêu | Góc lệch | Hướng lệch quan sát      | Sai số ghi nhận |
| ------: | ----------- | -------------------: | -------: | ------------------------ | --------------: |
|       1 | Tiến        |                50 cm |     0,1° | Gần như không lệch hướng |          0,5 cm |
|       2 | Lùi         |                50 cm |     1,0° | Nhẹ sang phải            |          1,0 cm |
|       3 | Tiến        |                50 cm |     0,4° | Gần như không lệch hướng |          1,0 cm |
|       4 | Lùi         |                50 cm |     2,3° | Sang phải                |          1,0 cm |
|       5 | Tiến        |                50 cm |     0,5° | Nhẹ sang phải            |          1,5 cm |
|       6 | Lùi         |                50 cm |     1,7° | Sang phải                |          1,0 cm |

### 2.2. Quãng đường 150 cm

| Lần thử | Chuyển động | Quãng đường mục tiêu |             Góc lệch | Hướng lệch quan sát               | Sai số ghi nhận |
| ------: | ----------- | -------------------: | -------------------: | --------------------------------- | --------------: |
|       7 | Tiến        |               150 cm | — (Odometry báo lỗi) | Mắt thường: sang trái khoảng 1 cm |     0,5 cm (\*) |
|       8 | Lùi         |               150 cm |                 3,8° | Sang phải                         |          5,9 cm |
|       9 | Tiến        |               150 cm |                 3,0° | Sang trái                         |          4,4 cm |
|      10 | Lùi         |               150 cm |                 1,7° | Sang phải                         |          4,2 cm |
|      11 | Tiến        |               150 cm |                 0,4° | Sang trái                         |          0,6 cm |
|      12 | Lùi         |               150 cm |                 1,5° | Sang phải                         |          2,6 cm |

(\*) **Lần 7:** Bản ghi có giá trị lệch **0,5 cm**, nhưng **Odometry báo lỗi và không hiển thị số**. Quan sát bằng mắt cho thấy xe lệch trái khoảng **1 cm**. Nguồn của số liệu 0,5 cm chưa rõ, do đó không nên coi đây là số đo Odometry hợp lệ.

### 2.3. Quãng đường 350 cm

| Lần thử | Chuyển động | Quãng đường mục tiêu | Góc lệch | Hướng lệch quan sát | Sai số ghi nhận |
| ------: | ----------- | -------------------: | -------: | ------------------- | --------------: |
|      13 | Tiến        |               350 cm |    0,05° | Sang phải           |          6,3 cm |
|      14 | Lùi         |               350 cm |     1,5° | Sang phải           |         16,0 cm |
|      15 | Tiến        |               350 cm |     6,9° | Sang phải           |         18,0 cm |
|      16 | Lùi         |               350 cm |     1,6° | Sang phải           |         17,0 cm |
|      17 | Tiến        |               350 cm |     0,2° | Sang trái           |          2,7 cm |
|      18 | Lùi         |               350 cm |     0,4° | Sang phải           |         15,8 cm |

## 3. Kiểm thử xoay 90°

| Lần thử | Hướng xoay | Góc mục tiêu | Góc thực tế ghi nhận | Sai số có dấu (thực tế − mục tiêu) | Sai số tuyệt đối |
| ------: | ---------- | -----------: | -------------------: | ---------------------------------: | ---------------: |
|       1 | Phải       |          90° |               92,05° |                             +2,05° |            2,05° |
|       2 | Phải       |          90° |               94,12° |                             +4,12° |            4,12° |
|       3 | Phải       |          90° |               91,86° |                             +1,86° |            1,86° |
|       4 | Trái       |          90° |               88,89° |                             −1,11° |            1,11° |
|       5 | Trái       |          90° |               92,42° |                             +2,42° |            2,42° |
|       6 | Trái       |          90° |               94,94° |                             +4,94° |            4,94° |

- **Sai số trung bình theo ghi chép ban đầu:** ~2,4°.
- **Sai số tuyệt đối trung bình (MAE), tính lại từ 6 giá trị:** **2,75°**.

> **Lưu ý:** Giá trị ~2,4° trong ghi chép ban đầu **không khớp** với trung bình sai số tuyệt đối tính từ 6 góc đo (2,75°). Nên kiểm tra lại phương pháp tính nếu cần dùng trong báo cáo.

## 4. Kiểm thử xoay 180°

| Lần thử | Hướng xoay | Góc mục tiêu | Góc thực tế ghi nhận | Sai số có dấu (thực tế − mục tiêu) | Sai số tuyệt đối |
| ------: | ---------- | -----------: | -------------------: | ---------------------------------: | ---------------: |
|       1 | Phải       |         180° |              181,15° |                             +1,15° |            1,15° |
|       2 | Phải       |         180° |              179,66° |                             −0,34° |            0,34° |
|       3 | Phải       |         180° |              179,14° |                             −0,86° |            0,86° |
|       4 | Trái       |         180° |              181,35° |                             +1,35° |            1,35° |
|       5 | Trái       |         180° |              177,55° |                             −2,45° |            2,45° |
|       6 | Trái       |         180° |              178,66° |                             −1,34° |            1,34° |

- **Góc trung bình khi xoay phải:** 179,98°.
- **Góc trung bình khi xoay trái:** 179,19°.
- **Sai số tuyệt đối trung bình (MAE):** ~1,25° (phù hợp với ghi chép ban đầu).

## 5. Quan sát thực tế

1. Xe **thường bắt đầu lệch hướng ở khoảng giữa quãng đường**, đặc biệt trong giai đoạn **tăng tốc**.
2. Có những lần xe lệch rõ rệt ở giữa đường, sau đó vẫn đến gần vạch đích do **hai bên bánh xe đã được cân chỉnh tốc độ**; cũng có những lần xe lệch quá nhiều và **không trở lại hướng mong muốn**.
3. Theo log hiện tại, **Odometry có thể báo mức độ lệch ước lượng** dù xe không tự khôi phục hoàn toàn quỹ đạo. Điều này cần được đối chiếu với phép đo thực tế để xác nhận độ chính xác.
4. Trong các lần lùi 350 cm, sai số ghi nhận khá lớn (**15,8–17,0 cm**) dù góc lệch cuối hành trình chỉ **0,4–1,6°**. Đây là hiện tượng đáng chú ý khi đánh giá khả năng bám quỹ đạo. (Vì đi lùi lệch khá nhiều nên bạn cần trả lời cho tôi Khi robot lùi, Heading PID hiện tại có đang hoạt động không, hay chỉ dùng Speed PID cho hai bánh?)

## 6. Ghi chú phục vụ phân tích tiếp theo

- **Góc lệch cuối hành trình nhỏ không có nghĩa là quỹ đạo đi thẳng:** xe có thể lệch ở giữa hành trình rồi tự cân bằng lại hướng quay trước khi dừng.
- **Speed Control** giúp điều chỉnh tốc độ hai bên bánh, nhưng không tự đảm bảo xe quay lại đúng quỹ đạo thẳng nếu không có cơ chế **Trajectory Tracking / Cross-Track Error Correction** phù hợp.
