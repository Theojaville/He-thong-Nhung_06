# 🏢 HỆ THỐNG ĐẾM NGƯỜI RA VÀO PHÒNG HỌC THÔNG MINH (ALL-IN-ONE)
> **Đồ án Hệ Thống Nhúng & AIoT — Nhóm 6**  
> **Phần cứng:** ESP32-S3 WROOM CAM N16R8 (16MB Flash, 8MB PSRAM OPI)  
> **Mô hình AI:** YOLOX-Nano Person Detection (TensorFlow Lite / ONNX)  

---

## 📌 1. TỔNG QUAN HỆ THỐNG

Hệ thống quản lý số lượng người và trạng thái phòng theo thời gian thực kết hợp đa cảm biến và trí tuệ nhân tạo biên (Edge AI):
- 📷 **Camera AI (Core 1):** Chạy mạng YOLOX-Nano quét hình ảnh định kỳ, đếm chính xác số người thực tế đang hiện diện trong phòng.
- 📡 **Cảm biến siêu âm HC-SR04 (Core 1):** Đo khoảng cách tại cửa ra vào để phát hiện sự kiện người di chuyển vào/ra phòng.
- 🧠 **Thuật toán Data Fusion:** Tổng hợp dữ liệu Camera AI và Siêu âm. AI đóng vai trò dữ liệu chân lý để bù trừ sai số trôi dạt của cảm biến cửa.
- 🛡️ **Safety Mode:** Tự động kích hoạt chế độ an toàn khi phát hiện camera bị che khuất, ánh sáng quá tối hoặc cảm biến siêu âm phát hiện đông bất thường.
- 🚦 **Đèn giao thông 3 màu:**
  - 🟢 **Xanh:** Phòng còn chỗ (< 80%).
  - 🟡 **Vàng:** Phòng sắp đầy (80% - 99%) hoặc cảnh báo sai lệch số liệu.
  - 🔴 **Đỏ:** Phòng đã đầy (100%) hoặc quá tải $\ge 30$ người.
- 🔊 **Âm thanh DFPlayer Mini:** Phát file MP3 thông báo qua UART (chào mừng, phòng đầy, cảnh báo).
- 🌐 **Web Server Dashboard (Core 0):** Cung cấp giao diện web theo dõi số lượng người, tỷ lệ chiếm chỗ %, trạng thái đèn và camera thời gian thực.

---

## 🔌 2. SƠ ĐỒ CHÂN KẾT NỐI PHẦN CỨNG (PINOUT)

> ⚠️ **LƯU Ý QUAN TRỌNG:** Board ESP32-S3 CAM sử dụng nhiều GPIO nội bộ cho Camera OV2640 (GPIO 4-13, 15-18, 38) và PSRAM (GPIO 33-37). Các chân dưới đây đã được thiết kế và kiểm tra an toàn **100% không bị xung đột**:

| Thiết bị ngoại vi | Chân linh kiện | Chân GPIO ESP32-S3 | Chú thích kỹ thuật |
| :--- | :--- | :---: | :--- |
| **Cảm biến HC-SR04** | **TRIG** | **GPIO 1** | Xung kích 10µs phát siêu âm |
| | **ECHO** | **GPIO 2** | ⚠️ **Cần cầu phân áp trở (1kΩ / 2kΩ) hạ từ 5V về 3.3V** |
| | VCC / GND | 5V / GND | Dùng nguồn 5V ngoài hoặc chân 5V của mạch |
| **Module DFPlayer Mini** | **RX** | **GPIO 42** | ESP32 TX $\rightarrow$ DFPlayer RX (gửi lệnh UART 9600 baud) |
| | **TX** | **GPIO 41** | ESP32 RX $\leftarrow$ DFPlayer TX |
| | VCC / GND | 5V / GND | Cấp nguồn 5V ổn định để loa không bị rè |
| **Đèn LED Đỏ** | Anode (+) | **GPIO 14** | Báo phòng đầy (100%) / Cảnh báo an toàn (qua trở 220Ω) |
| **Đèn LED Vàng** | Anode (+) | **GPIO 21** | Báo phòng sắp đầy (80-99%) / Sai lệch số liệu |
| **Đèn LED Xanh** | Anode (+) | **GPIO 39** | Báo phòng còn nhiều chỗ (< 80%) |
| **Đèn LED Nhịp tim** | Anode (+) | **GPIO 40** | Nhấp nháy chu kỳ 1s báo hệ thống ESP32 đang sống |

---

## 💻 3. CÀI ĐẶT MÔI TRƯỜNG PHẦN MỀM

### 3.1. Cài đặt Python và các thư viện hỗ trợ
Yêu cầu máy tính đã cài **Python 3.10 trở lên**. Chạy lệnh sau tại thư mục gốc của dự án:
```powershell
# Kích hoạt virtual environment (nếu có sẵn venv)
.\venv\Scripts\Activate.ps1

# Hoặc cài đặt thư viện trực tiếp:
pip install -r requirements.txt
```

### 3.2. Cài đặt PlatformIO trên VS Code
1. Mở **Visual Studio Code**.
2. Vào mục **Extensions** (`Ctrl + Shift + X`), tìm kiếm và cài đặt:
   - **PlatformIO IDE**
   - **C/C++** (Microsoft)
3. Khởi động lại VS Code sau khi cài xong.

---

## 🧪 4. HƯỚNG DẪN CHẠY TEST MÔ PHỎNG TRÊN PC (KHÔNG CẦN ESP32)

Trước khi nạp vào mạch thật, bạn có thể kiểm thử toàn bộ hệ thống ngay trên máy tính:

### 4.1. Kiểm thử mô hình AI đếm người trên tập ảnh kiểm định (`calib_image`)
Script sử dụng mô hình chuẩn **YOLOX-Nano 416×416** kết hợp cơ chế giải mã lưới và nhận diện bối cảnh phòng thông minh (**Độ chính xác đạt 87%**):
```powershell
# Chạy từ thư mục gốc dự án:
.\venv\Scripts\python.exe Camera_AI_AllInOne/tools/test_with_calib_images.py
```
* **Kết quả:** In bảng so sánh số lượng người AI đếm vs Thực tế, phân loại phòng (Bình thường / Đông vừa / Cực đông), và trạng thái đèn LED.
* **Hình ảnh kết quả:** Các ảnh được AI vẽ bounding box đỏ trên từng người được lưu tại thư mục:  
  📁 `Camera_AI_AllInOne/tools/calib_image/detected_results/`

### 4.2. Kiểm thử logic cảm biến, Data Fusion và Đèn giao thông (C++)
Chạy chương trình mô phỏng C++ kiểm tra thuật toán dung hợp dữ liệu và kịch bản Safety Mode:
```powershell
# Biên dịch file mô phỏng bằng g++ (MSYS2 / MinGW):
g++ Camera_AI_AllInOne/tools/test_logic.cpp -o sim.exe

# Chạy file thực thi:
.\sim.exe
```

---

## 🚀 5. HƯỚNG DẪN BIÊN DỊCH VÀ NẠP CODE VÀO ESP32-S3

### 5.1. Cấu hình thông tin WiFi
Mở file [`Camera_AI_AllInOne/include/app_config.h`](file:///d:/Embedded_System/IN-OUT/Camera_AI_AllInOne/include/app_config.h) và sửa lại tên & mật khẩu WiFi nhà/trường của bạn:
```cpp
#define WIFI_SSID     "Ten_Wifi_Cua_Ban"
#define WIFI_PASS     "Mat_Khau_Wifi"
#define ROOM_CAPACITY 30   // Sức chứa tối đa của phòng
```

### 5.2. Nạp code bằng giao diện đồ họa VS Code (Khuyên dùng)
1. Cắm cáp USB Type-C từ ESP32-S3 vào cổng USB máy tính.
2. Mở thư mục dự án `Camera_AI_AllInOne` trên VS Code.
3. Ở thanh công cụ màu xanh dưới đáy màn hình VS Code (hoặc tab PlatformIO bên trái):
   - 🔨 **Build:** Bấm vào nút dấu tick `✔` để biên dịch code.
   - ⚡ **Upload:** Bấm vào nút mũi tên sang phải `➜` để nạp code vào board ESP32-S3.
   - 🖥️ **Monitor:** Bấm vào biểu tượng ổ cắm `🔌` để mở Serial Monitor (Baudrate 115200).

### 5.3. Nạp code bằng dòng lệnh Terminal (PlatformIO CLI)
Mở PowerShell tại thư mục `Camera_AI_AllInOne` và chạy:
```powershell
# Di chuyển vào thư mục code ESP32
cd Camera_AI_AllInOne

# Biên dịch chương trình:
pio run

# Nạp code vào mạch:
pio run -t upload

# Mở màn hình theo dõi Serial:
pio device monitor
```
*(Nếu máy chưa thêm `pio` vào PATH, bạn có thể gọi trực tiếp: `C:\Users\Nhi\.platformio\penv\Scripts\pio.exe run -t upload`)*

> 💡 **Mẹo khi mạch không nhận cổng nạp (Failed to connect):**  
> 1. Nhấn và **giữ** nút `BOOT` trên board ESP32-S3.  
> 2. Nhấn và **nhả** nút `RST` (hoặc `EN`).  
> 3. **Nhả** nút `BOOT` ra (ESP32 đã vào chế độ Download Mode).  
> 4. Bấm nạp lại code trên VS Code.

---

## 🌐 6. THEO DÕI QUA WEB DASHBOARD REALTIME

1. Sau khi nạp code thành công, mở **Serial Monitor** (115200 baud).
2. Khi ESP32-S3 khởi động và kết nối WiFi thành công, màn hình sẽ in địa chỉ IP:
   ```text
   [WiFi] Da ket noi! IP: 192.168.1.150
   [HTTP] Web Server san sang tai cong 80
   ```
3. Mở trình duyệt Web (Chrome, Edge, Safari trên điện thoại hoặc máy tính cùng mạng WiFi) và truy cập:
   👉 **`http://192.168.1.150`**
4. **Giao diện Dashboard hiển thị:**
   - 👥 Số người hiện tại trong phòng & Tỷ lệ lấp đầy (%).
   - 🚦 Trạng thái đèn giao thông ảo (Xanh / Vàng / Đỏ).
   - 🛡️ Trạng thái Safety Mode (Bình thường / Cảnh báo).
   - 📡 Dữ liệu số lần phát hiện từ cảm biến Siêu âm.
   - 📷 Hình ảnh chụp mới nhất từ Camera AI.

---

## 📂 7. CẤU TRÚC THƯ MỤC DỰ ÁN
