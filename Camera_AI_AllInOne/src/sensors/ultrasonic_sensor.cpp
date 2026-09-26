#include "ultrasonic_sensor.h"
#include "app_config.h"

// ===================================================================
//  Triển khai cảm biến siêu âm HC-SR04 — State Machine đếm crossing
//
//  ⚠ GIỚI HẠN:
//    - Chỉ phát hiện người đi ngang cửa, KHÔNG phân biệt hướng vào/ra.
//    - crossingCount là tổng lượt cắt ngang cộng dồn.
//    - Kết hợp với AI để xác định số người thực tế trong phòng.
// ===================================================================

// ---------- Trạng thái State Machine ----------
enum UltrasonicState {
    US_FAR,       // Không có vật thể gần (khoảng cách >= ngưỡng)
    US_NEAR       // Có vật thể gần (khoảng cách < ngưỡng)
};

// ---------- Biến bộ đếm (bảo vệ bằng spinlock) ----------
static portMUX_TYPE usMux = portMUX_INITIALIZER_UNLOCKED;
static volatile int crossingCount = 0;     // Tổng lượt cắt ngang cộng dồn

// ---------- Biến trạng thái nội bộ ----------
static UltrasonicState usState = US_FAR;
static uint32_t nearStartTime    = 0;      // Thời điểm bắt đầu vào trạng thái NEAR
static uint32_t lastPollTime     = 0;      // Thời điểm poll lần cuối
static uint32_t lastCrossingTime = 0;      // Thời điểm crossing gần nhất (cho hasRecentActivity)
static float    lastDistanceCm   = 0.0f;   // Khoảng cách đo được gần nhất

// ===================================================================
//  Hàm đo khoảng cách 1 lần bằng pulseIn()
//  Timeout ~30ms để không block lâu (tương đương ~5m max range)
// ===================================================================
static float measureDistanceCm() {
    // Phát xung TRIG 10µs
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    // Đo thời gian ECHO (timeout 30ms ≈ ~5m max)
    unsigned long duration = pulseIn(PIN_ECHO, HIGH, 30000);

    if (duration == 0) {
        // Timeout — không nhận được echo (quá xa hoặc lỗi)
        return 0.0f;
    }

    // Tính khoảng cách: v_sound ≈ 343 m/s = 0.0343 cm/µs
    // Khoảng cách = duration * 0.0343 / 2 (đi + về)
    float distCm = (float)duration * 0.01715f;
    return distCm;
}

// ===================================================================
//  Hàm công khai (Public API)
// ===================================================================

void ultrasonicSensorInit() {
    // Cấu hình chân GPIO
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);

    // Đảm bảo TRIG ở mức LOW ban đầu
    digitalWrite(PIN_TRIG, LOW);

    Serial.println("[US] Khởi tạo cảm biến siêu âm HC-SR04 hoàn tất.");
    Serial.printf("[US]   TRIG = GPIO %d | ECHO = GPIO %d\n", PIN_TRIG, PIN_ECHO);
    Serial.printf("[US]   Ngưỡng khoảng cách: %d cm\n", PRESENCE_THRESHOLD_CM);
    Serial.printf("[US]   Debounce: %d ms | Polling: %d ms\n", MIN_PRESENCE_MS, ULTRASONIC_POLL_MS);
    Serial.println("[US]   ⚠ Lưu ý: Chỉ phát hiện cắt ngang, KHÔNG phân biệt hướng vào/ra.");
}

void ultrasonicSensorUpdate() {
    // Kiểm tra chu kỳ polling — không block nếu chưa đến lúc
    uint32_t now = millis();
    if (now - lastPollTime < ULTRASONIC_POLL_MS) return;
    lastPollTime = now;

    // Đo khoảng cách
    float distCm = measureDistanceCm();
    lastDistanceCm = distCm;

    // Xác định có vật thể gần hay không
    // distCm == 0 nghĩa là timeout → coi như FAR (không có vật)
    bool isNear = (distCm > 0.0f && distCm < (float)PRESENCE_THRESHOLD_CM);

    // ---------------------------------------------------------------
    //  State Machine: FAR ↔ NEAR
    //
    //  FAR:
    //    - Nếu isNear → chuyển sang NEAR, ghi nhận thời điểm
    //  NEAR:
    //    - Nếu vẫn isNear → chờ (chưa làm gì)
    //    - Nếu !isNear VÀ đã ở NEAR đủ MIN_PRESENCE_MS:
    //      → Hoàn tất 1 chu kỳ NEAR→FAR hợp lệ → tăng crossingCount
    //    - Nếu !isNear NHƯNG chưa đủ MIN_PRESENCE_MS:
    //      → Nhiễu, quay về FAR không đếm
    // ---------------------------------------------------------------

    switch (usState) {

    case US_FAR:
        if (isNear) {
            usState = US_NEAR;
            nearStartTime = now;
        }
        break;

    case US_NEAR:
        if (!isNear) {
            // Kiểm tra đã ở NEAR đủ lâu chưa (debounce)
            uint32_t nearDuration = now - nearStartTime;

            if (nearDuration >= MIN_PRESENCE_MS) {
                // ✅ Chu kỳ NEAR→FAR hợp lệ → đếm 1 lần crossing
                portENTER_CRITICAL(&usMux);
                crossingCount++;
                portEXIT_CRITICAL(&usMux);

                lastCrossingTime = now;

                Serial.printf("[US] ✔ Phát hiện crossing #%d | NEAR %lu ms | Dist=%.1f cm\n",
                              crossingCount, nearDuration, distCm);
            }
            // Quay về FAR (dù hợp lệ hay nhiễu)
            usState = US_FAR;
        }
        // else: vẫn NEAR → chờ tiếp
        break;
    }
}

int getUltrasonicCrossingCount() {
    portENTER_CRITICAL(&usMux);
    int val = crossingCount;
    portEXIT_CRITICAL(&usMux);
    return val;
}

void resetUltrasonicCount(int value) {
    portENTER_CRITICAL(&usMux);
    crossingCount = value;
    portEXIT_CRITICAL(&usMux);
    Serial.printf("[US] Reset bộ đếm crossing → %d\n", value);
}

bool hasRecentActivity(uint32_t windowMs) {
    if (lastCrossingTime == 0) return false;
    return (millis() - lastCrossingTime) <= windowMs;
}

float getLastDistanceCm() {
    return lastDistanceCm;
}
