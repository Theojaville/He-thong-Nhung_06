// ===================================================================
//
//  MAIN.CPP — Điểm vào chương trình ESP32-CAM All-in-One
//
//  Hệ thống đếm người ra vào phòng — Nhóm 6
//  Board: ESP32-S3 WROOM N16R8 CAM
//
//  Kiến trúc FreeRTOS:
//  ┌──────────────────────────────────────────────────────────┐
//  │  Core 1 (TASK_AI):                                       │
//  │    capture ảnh → resize 96×96 → inference YoloX-nano     │
//  │    → đếm person → fusion với siêu âm → cảnh báo         │
//  │    → Safety Mode check                                    │
//  │    Chu kỳ: mỗi AI_CAPTURE_INTERVAL_MS (3 giây)          │
//  │                                                          │
//  │  Core 0 (TASK_WEB):                                      │
//  │    WebServer.handleClient()                               │
//  │    MJPEG stream + REST API                                │
//  │    (Chạy cùng core với WiFi stack)                        │
//  │                                                          │
//  │  Main loop() — Core 1:                                    │
//  │    ultrasonicSensorUpdate() — polling state machine       │
//  │    audioUpdate() — phát giai điệu non-blocking           │
//  │    LED heartbeat (PIN_LED_STATUS) — nhấp nháy debug      │
//  └──────────────────────────────────────────────────────────┘
//
// ===================================================================

#include <Arduino.h>
#include "app_config.h"

// --- Các module ---
#include "sensors/ultrasonic_sensor.h"
#include "comm/audio_alert.h"
#include "comm/led_indicator.h"
#include "ai/camera_capture.h"
#include "ai/yolox_inference.h"
#include "ai/image_utils.h"
#include "fusion/data_fusion.h"
#include "comm/web_server.h"
#include "system/safety_mode.h"

// ===================================================================
//  BIẾN TOÀN CỤC
// ===================================================================

// Buffer RGB cho AI input (96×96×3 = 27,648 bytes) — cấp phát PSRAM
static uint8_t* aiInputBuffer = nullptr;

// Kết quả AI gần nhất (chia sẻ giữa task AI và task Web)
static portMUX_TYPE resultMux = portMUX_INITIALIZER_UNLOCKED;
static InferenceResult latestAIResult;

// Trạng thái phòng trước đó (để phát âm thanh khi thay đổi)
static RoomStatus previousRoomStatus = ROOM_AVAILABLE;

// Cờ AI init thành công (dùng cho Safety Mode)
static bool aiInitSuccess = false;

// Task handles
static TaskHandle_t aiTaskHandle  = nullptr;
static TaskHandle_t webTaskHandle = nullptr;

// LED heartbeat — nhấp nháy PIN_LED_STATUS để debug (còn sống)
static uint32_t heartbeatTimer = 0;
static bool     heartbeatState = false;

// ===================================================================
//  TASK AI — Chạy trên Core 1
//
//  Vòng lặp:
//    1. Chụp ảnh + resize → 96×96 RGB888
//    2. Chạy YoloX-nano inference
//    3. Lấy crossing count + activity từ siêu âm
//    4. Dung hợp (fusion) siêu âm + AI
//    5. Safety Mode check
//    6. Xử lý cảnh báo (LED đèn giao thông + âm thanh)
//    7. Nghỉ theo chu kỳ AI_CAPTURE_INTERVAL_MS
// ===================================================================

static void taskAI(void* pvParam) {
    Serial.println("[TASK_AI] Bắt đầu chạy trên Core 1");

    while (true) {
        uint32_t cycleStart = millis();

        // ---- Bước 1: Chụp ảnh + resize ----
        bool captured = captureAndResizeRGB(aiInputBuffer, AI_INPUT_W, AI_INPUT_H);

        int aiCount = 0;
        bool inferenceOk = false;

        if (captured) {
            // ---- Bước 2: Inference ----
            InferenceResult aiResult = runInference(aiInputBuffer);

            // Lưu kết quả (thread-safe)
            portENTER_CRITICAL(&resultMux);
            latestAIResult = aiResult;
            portEXIT_CRITICAL(&resultMux);

            if (aiResult.success) {
                aiCount = aiResult.personCount;
                inferenceOk = true;
            }
        } else {
            Serial.println("[TASK_AI] ⚠ Chụp ảnh thất bại, bỏ qua chu kỳ này.");
        }

        // ---- Bước 3: Lấy dữ liệu siêu âm ----
        int crossings = getUltrasonicCrossingCount();
        bool activity = hasRecentActivity(10000);   // 10 giây

        // ---- Bước 4: Fusion ----
        FusionResult fr = fusionUpdate(crossings, activity, aiCount);

        // ---- Bước 5: Safety Mode ----
        uint32_t freeHeap = ESP.getFreeHeap();
        bool safetyTriggered = safetyModeUpdate(
            aiInitSuccess,       // AI init ok?
            captured,            // Lần capture này ok?
            freeHeap,            // Heap hiện tại
            fr.discrepancyFlag   // Sai lệch?
        );

        // ---- Bước 6: Xử lý LED + Âm thanh ----
        static bool safetyAlertPlayed = false;   // Cờ phát âm safety 1 lần

        if (safetyTriggered) {
            // Safety Mode active → LED pattern đặc biệt
            updateStatusLEDs_SafetyMode();

            // Phát âm cảnh báo CHỈ 1 lần khi vừa vào mode
            if (!safetyAlertPlayed) {
                playAlertSafetyMode();
                safetyAlertPlayed = true;
            }
        } else {
            // Không Safety → LED đèn giao thông bình thường
            updateStatusLEDs(fr.occupancyPercent, fr.roomStatus);

            // Reset flag âm thanh safety khi đã thoát
            safetyAlertPlayed = false;

            // Âm thanh: chỉ phát khi trạng thái THAY ĐỔI (tránh phát liên tục)
            if (fr.roomStatus != previousRoomStatus) {
                if (fr.roomStatus == ROOM_FULL) {
                    playAlertRoomFull();
                }
                else if (previousRoomStatus == ROOM_FULL &&
                         fr.roomStatus == ROOM_AVAILABLE) {
                    playAlertWelcome();
                }
                previousRoomStatus = fr.roomStatus;
            }
        }

        // ---- Log tổng hợp ----
        if (inferenceOk || captured) {
            Serial.println("──────────────────────────────────────");
            Serial.printf("  Crossing: %d | Activity: %s | AI: %d | Final: %d\n",
                          fr.crossingCount,
                          fr.hasActivity ? "CÓ" : "KHÔNG",
                          fr.aiCount, fr.finalCount);
            Serial.printf("  Phòng: %s (%.0f%%) | FPS: %.1f\n",
                          roomStatusToString(fr.roomStatus),
                          fr.occupancyPercent,
                          getAverageFPS());
            Serial.printf("  Heap: %lu bytes | Safety: %s\n",
                          freeHeap,
                          safetyTriggered ? "ACTIVE" : "off");
            Serial.println("──────────────────────────────────────");

            UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            Serial.printf("[TASK_AI] Stack còn dư: %u words (%u bytes)\n", hwm, hwm * 4);
        }

        // ---- Chờ đủ chu kỳ ----
        uint32_t elapsed = millis() - cycleStart;
        if (elapsed < AI_CAPTURE_INTERVAL_MS) {
            vTaskDelay(pdMS_TO_TICKS(AI_CAPTURE_INTERVAL_MS - elapsed));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));   // Tối thiểu nghỉ 100ms
        }
    }
}

// ===================================================================
//  TASK WEB — Chạy trên Core 0 (cùng core WiFi stack)
// ===================================================================

static void taskWeb(void* pvParam) {
    Serial.println("[TASK_WEB] Bắt đầu chạy trên Core 0");

    while (true) {
        webServerHandle();
        vTaskDelay(pdMS_TO_TICKS(1));   // Yield ngắn cho watchdog
    }
}

// ===================================================================
//  SETUP — Khởi tạo toàn bộ hệ thống
// ===================================================================

void setup() {
    // Serial debug
    Serial.begin(115200);
    delay(1000);   // Chờ Serial ổn định

    Serial.println();
    Serial.println("╔════════════════════════════════════════════════╗");
    Serial.println("║  HỆ THỐNG ĐẾM NGƯỜI RA VÀO PHÒNG — Nhóm 6   ║");
    Serial.println("║  ESP32-CAM All-in-One                          ║");
    Serial.println("║  AI: YoloX-nano 96×96 | Sensor: HC-SR04        ║");
    Serial.println("╚════════════════════════════════════════════════╝");
    Serial.println();

    // Kiểm tra PSRAM
    if (psramFound()) {
        Serial.printf("[INIT] ✔ PSRAM: %d KB khả dụng\n", ESP.getFreePsram() / 1024);
    } else {
        Serial.println("[INIT] ✖ PSRAM KHÔNG TÌM THẤY! Hệ thống cần PSRAM để hoạt động.");
        Serial.println("[INIT]   Kiểm tra board có chip PSRAM không.");
        // KHÔNG while(true) delay(1000) — để Safety Mode xử lý
        // Nhưng PSRAM là bắt buộc cho AI → vẫn dừng ở đây
        while (true) delay(1000);
    }

    // LED heartbeat (debug)
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    // ---- Khởi tạo các module ----
    Serial.println("\n--- Khởi tạo LED đèn giao thông ---");
    ledIndicatorInit();

    Serial.println("\n--- Khởi tạo Cảm biến siêu âm ---");
    ultrasonicSensorInit();

    Serial.println("\n--- Khởi tạo Âm thanh ---");
    audioInit();

    Serial.println("\n--- Khởi tạo Safety Mode ---");
    safetyModeInit();

    Serial.println("\n--- Khởi tạo Camera ---");
    if (!cameraInit()) {
        Serial.println("[INIT] ✖ Camera init thất bại!");
        Serial.println("[INIT]   Safety Mode sẽ xử lý — hệ thống tiếp tục chạy.");
        // KHÔNG while(true) delay(1000) — Safety Mode sẽ kích hoạt
    }

    Serial.println("\n--- Khởi tạo AI (TFLite Micro) ---");
    // Cấp phát buffer ảnh cho AI trong PSRAM
    size_t inputBufSize = AI_INPUT_W * AI_INPUT_H * AI_INPUT_CHANNELS;
    aiInputBuffer = (uint8_t*)ps_malloc(inputBufSize);
    if (!aiInputBuffer) {
        Serial.printf("[INIT] ✖ Không cấp phát được %d bytes PSRAM cho AI input!\n",
                      inputBufSize);
        // KHÔNG while(true) delay(1000) — Safety Mode sẽ kích hoạt
    } else {
        Serial.printf("[INIT] ✔ AI input buffer: %d bytes (PSRAM)\n", inputBufSize);
    }

    if (inferenceInit()) {
        aiInitSuccess = true;
    } else {
        aiInitSuccess = false;
        Serial.println("[INIT] ⚠ AI init thất bại! Safety Mode sẽ kích hoạt.");
        Serial.println("[INIT]   (Cần thay model placeholder bằng model thật)");
        // KHÔNG dừng — Safety Mode xử lý
    }

    Serial.println("\n--- Khởi tạo Fusion ---");
    fusionInit();

    Serial.println("\n--- Khởi tạo Web Server ---");
    if (!webServerInit()) {
        Serial.println("[INIT] ⚠ Web Server init thất bại! Chạy offline (Serial only).");
    }

    // ---- Tạo FreeRTOS Tasks ----
    Serial.println("\n--- Tạo FreeRTOS Tasks ---");

    xTaskCreatePinnedToCore(
        taskAI,               // Hàm task
        "TaskAI",             // Tên task
        TASK_AI_STACK_SIZE,   // Stack size
        nullptr,              // Parameter
        TASK_AI_PRIORITY,     // Priority
        &aiTaskHandle,        // Handle
        TASK_AI_CORE          // Core 1
    );
    Serial.printf("[INIT] ✔ Task AI → Core %d | Stack %d bytes | Priority %d\n",
                  TASK_AI_CORE, TASK_AI_STACK_SIZE, TASK_AI_PRIORITY);

    xTaskCreatePinnedToCore(
        taskWeb,
        "TaskWeb",
        TASK_WEB_STACK_SIZE,
        nullptr,
        TASK_WEB_PRIORITY,
        &webTaskHandle,
        TASK_WEB_CORE
    );
    Serial.printf("[INIT] ✔ Task Web → Core %d | Stack %d bytes | Priority %d\n",
                  TASK_WEB_CORE, TASK_WEB_STACK_SIZE, TASK_WEB_PRIORITY);

    // Phát âm chào mừng khi khởi động xong
    playAlertWelcome();

    Serial.println();
    Serial.println("═══════════════════════════════════════");
    Serial.println("  ✔ HỆ THỐNG ĐÃ SẴN SÀNG HOẠT ĐỘNG");
    Serial.println("═══════════════════════════════════════");
    Serial.println();

    // In bộ nhớ còn lại
    Serial.printf("[INIT] Free Heap:  %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("[INIT] Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
}

// ===================================================================
//  LOOP — Chạy trên Core 1 (Arduino default)
//
//  Xử lý nhẹ:
//    - Cập nhật siêu âm (polling state machine)
//    - Cập nhật âm thanh (non-blocking playback)
//    - LED heartbeat (PIN_LED_STATUS) — nhấp nháy chậm để debug
// ===================================================================

void loop() {
    // Cập nhật state machine siêu âm (polling)
    ultrasonicSensorUpdate();

    // Cập nhật phát âm thanh (nốt tiếp theo nếu đang phát)
    audioUpdate();

    // LED heartbeat (PIN_LED_STATUS) — nhấp nháy 1Hz để biết hệ thống còn sống
    uint32_t now = millis();
    if (now - heartbeatTimer >= 500) {
        heartbeatTimer = now;
        heartbeatState = !heartbeatState;
        digitalWrite(PIN_LED_STATUS, heartbeatState ? HIGH : LOW);
    }

    // Delay nhỏ để tránh chiếm CPU 100%
    // (Task AI chạy trên cùng core nhưng có priority cao hơn)
    delay(10);
}
