#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// ===================================================================
//  CẤU HÌNH CHUNG — Hệ thống đếm người ESP32-CAM All-in-One
//  Mọi hằng số phần cứng & phần mềm tập trung tại đây.
//  Khi cần thay đổi ngưỡng / chân / WiFi → chỉ sửa file này.
// ===================================================================

// ======================== CHÂN GPIO =================================
// ĐÃ CHỐT: Sơ đồ chân an toàn cho ESP32-S3 WROOM N16R8 CAM
// Tránh xung đột với các chân Camera (4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 38)
// và chip nhớ Octal PSRAM/Flash (33–37).

// Cảm biến siêu âm HC-SR04 — phát hiện người đi ngang cửa
#define PIN_TRIG              1    // Chân Trigger (OUTPUT) (chân tự do trên S3)
#define PIN_ECHO              2    // Chân Echo    (INPUT)  (qua cầu trở phân áp 3.3V)

// DFPlayer Mini — Giao tiếp UART Serial phát âm thanh / MP3
#define PIN_DFPLAYER_RX      41    // ESP32 RX (nối vào chân TX của DFPlayer)
#define PIN_DFPLAYER_TX      42    // ESP32 TX (nối vào chân RX của DFPlayer)

// LED trạng thái heartbeat (nhấp nháy debug hệ thống còn sống)
#define PIN_LED_STATUS       40    // LED Heartbeat (chân tự do trên S3)

// LED đèn giao thông 3 màu — qua điện trở hạn dòng
#define PIN_LED_RED          14    // LED1 Đỏ   (GPIO 14 — chân tự do)
#define PIN_LED_YELLOW       21    // LED2 Vàng (GPIO 21 — chân tự do)
#define PIN_LED_GREEN        39    // LED3 Xanh (GPIO 39 — chân tự do)

// ======================== CẤU HÌNH WIFI =============================
#define WIFI_SSID           "Galaxy A05 2145"      // ← Đổi tên WiFi
#define WIFI_PASSWORD       "186491851826"       // ← Đổi mật khẩu

// ======================== CẤU HÌNH AI (YoloX-nano) ==================
#define AI_INPUT_W           96      // Chiều rộng ảnh đầu vào model
#define AI_INPUT_H           96      // Chiều cao ảnh đầu vào model
#define AI_INPUT_CHANNELS     3      // Số kênh: 3 = RGB

#define AI_CONFIDENCE_THRESH  0.45f  // Ngưỡng confidence tối thiểu giữ lại
#define AI_NMS_IOU_THRESH     0.45f  // Ngưỡng IoU cho Non-Max Suppression
#define AI_MAX_DETECTIONS     10     // Số bounding box tối đa sau NMS

// Kích thước tensor arena TFLite Micro (byte) — cấp phát trong PSRAM
// ⚠ Nếu model quá lớn, tăng giá trị này (tối đa ~2MB trên 4MB PSRAM)
#define TENSOR_ARENA_SIZE    (400 * 1024)  // 400 KB

// Số class COCO (YoloX-nano output). Person = class index 0
#define YOLOX_NUM_CLASSES     80
#define YOLOX_PERSON_CLASS     0

// ======================== CẤU HÌNH PHÒNG ============================
#define ROOM_MAX_CAPACITY     30     // Sức chứa tối đa (người)
#define DISCREPANCY_THRESHOLD  3     // |Sensor − AI| > ngưỡng → cảnh báo sai lệch

// ======================== CẤU HÌNH CẢM BIẾN SIÊU ÂM ================
#define PRESENCE_THRESHOLD_CM  30    // Khoảng cách < ngưỡng này (cm) = có người
#define MIN_PRESENCE_MS       150    // Phải ở trạng thái NEAR tối thiểu bao lâu (debounce)
#define ULTRASONIC_POLL_MS     50    // Chu kỳ polling đo khoảng cách (ms)

// ======================== CẤU HÌNH THỜI GIAN ========================
#define AI_CAPTURE_INTERVAL_MS   3000   // Chu kỳ chụp + suy luận AI (ms)

// ======================== CẤU HÌNH ÂM THANH (DFPLAYER MINI) =========
#define DFPLAYER_VOLUME       25    // Mức âm lượng DFPlayer (0 - 30)

// ======================== CẤU HÌNH WEB SERVER =======================
#define WEB_SERVER_PORT         80
#define STREAM_FRAME_DELAY_MS  100    // Delay giữa 2 frame MJPEG (ms)
#define STREAM_PART_BOUNDARY   "frame_boundary_nhom6"

// ======================== CẤU HÌNH SAFETY MODE ======================
#define SAFETY_MIN_FREE_HEAP_BYTES   (20 * 1024)   // Heap tối thiểu (bytes)
#define SAFETY_CAM_FAIL_LIMIT         5             // Số lần capture thất bại liên tiếp
#define SAFETY_DISCREPANCY_DURATION_MS  60000       // Sai lệch liên tục quá 60 giây
#define SAFETY_STABLE_CYCLES_TO_EXIT    5           // Số chu kỳ ổn định liên tiếp để thoát Safety

// ======================== FREERTOS ==================================
#define TASK_AI_STACK_SIZE     (20 * 1024)  // Stack cho task AI (byte) — JPEG decode cần nhiều stack
#define TASK_WEB_STACK_SIZE    (8 * 1024)   // Stack cho task Web server
#define TASK_AI_PRIORITY        2           // Ưu tiên cao cho AI
#define TASK_WEB_PRIORITY       1           // Ưu tiên thấp hơn cho Web
#define TASK_AI_CORE            1           // Core 1 — chạy inference
#define TASK_WEB_CORE           0           // Core 0 — WiFi stack ở đây
#endif // APP_CONFIG_H
