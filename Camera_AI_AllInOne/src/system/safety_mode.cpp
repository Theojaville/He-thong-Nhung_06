#include "safety_mode.h"
#include "app_config.h"

// ===================================================================
//  Triển khai Safety Mode — Bảo vệ hệ thống khi gặp sự cố
//
//  Logic:
//    VÀO Safety Mode khi BẤT KỲ điều kiện nào đúng:
//      - aiInitOk == false
//      - Camera fail liên tiếp >= SAFETY_CAM_FAIL_LIMIT
//      - freeHeap < SAFETY_MIN_FREE_HEAP_BYTES
//      - discrepancyFlag liên tục >= SAFETY_DISCREPANCY_DURATION_MS
//
//    THOÁT Safety Mode khi:
//      - KHÔNG CÒN điều kiện lỗi nào đúng
//      - VÀ đã ổn định liên tiếp SAFETY_STABLE_CYCLES_TO_EXIT chu kỳ
// ===================================================================

// ---------- Biến trạng thái nội bộ ----------
static bool     safetyActive       = false;  // Safety Mode đang kích hoạt?
static bool     justEntered        = false;  // Vừa mới vào mode? (để phát âm 1 lần)

// Bộ đếm camera fail liên tiếp
static int      camFailConsecutive = 0;

// Thời điểm discrepancy bắt đầu liên tục
static uint32_t discrepancyStartMs = 0;
static bool     discrepancyOngoing = false;

// Hysteresis: bộ đếm chu kỳ ổn định liên tiếp để thoát Safety
static int      stableCycleCount   = 0;

// Lý do vào Safety Mode (cho Serial log)
static const char* safetyReason = "";

// ===================================================================
//  Hàm công khai (Public API)
// ===================================================================

void safetyModeInit() {
    safetyActive       = false;
    justEntered        = false;
    camFailConsecutive = 0;
    discrepancyStartMs = 0;
    discrepancyOngoing = false;
    stableCycleCount   = 0;
    safetyReason       = "";

    Serial.println("[SAFETY] Khởi tạo module Safety Mode.");
    Serial.printf("[SAFETY]   Min Free Heap: %d bytes\n", SAFETY_MIN_FREE_HEAP_BYTES);
    Serial.printf("[SAFETY]   Cam Fail Limit: %d lần\n", SAFETY_CAM_FAIL_LIMIT);
    Serial.printf("[SAFETY]   Discrepancy Timeout: %d ms\n", SAFETY_DISCREPANCY_DURATION_MS);
    Serial.printf("[SAFETY]   Stable Cycles to Exit: %d\n", SAFETY_STABLE_CYCLES_TO_EXIT);
}

bool safetyModeUpdate(bool aiInitOk, bool lastCaptureOk,
                      uint32_t freeHeap, bool discrepancyFlag) {

    uint32_t now = millis();
    bool shouldBeActive = false;
    const char* reason = "";

    // ---- Kiểm tra điều kiện 1: AI init fail ----
    if (!aiInitOk) {
        shouldBeActive = true;
        reason = "AI init that bai";
    }

    // ---- Kiểm tra điều kiện 2: Camera fail liên tiếp ----
    if (lastCaptureOk) {
        camFailConsecutive = 0;   // Reset khi capture thành công
    } else {
        camFailConsecutive++;
    }
    if (camFailConsecutive >= SAFETY_CAM_FAIL_LIMIT) {
        shouldBeActive = true;
        reason = "Camera fail lien tiep";
    }

    // ---- Kiểm tra điều kiện 3: Free heap quá thấp ----
    if (freeHeap < SAFETY_MIN_FREE_HEAP_BYTES) {
        shouldBeActive = true;
        reason = "Free heap qua thap";
    }

    // ---- Kiểm tra điều kiện 4: Discrepancy liên tục quá lâu ----
    if (discrepancyFlag) {
        if (!discrepancyOngoing) {
            // Bắt đầu đếm thời gian discrepancy
            discrepancyOngoing = true;
            discrepancyStartMs = now;
        } else {
            // Kiểm tra đã quá timeout chưa
            if (now - discrepancyStartMs >= SAFETY_DISCREPANCY_DURATION_MS) {
                shouldBeActive = true;
                reason = "Sai lech lien tuc qua lau";
            }
        }
    } else {
        // Discrepancy hết → reset
        discrepancyOngoing = false;
        discrepancyStartMs = 0;
    }

    // ---- Xử lý chuyển trạng thái ----

    if (shouldBeActive) {
        // Reset bộ đếm ổn định
        stableCycleCount = 0;

        if (!safetyActive) {
            // === CHUYỂN VÀO Safety Mode ===
            safetyActive = true;
            justEntered  = true;
            safetyReason = reason;

            Serial.println("╔══════════════════════════════════════════╗");
            Serial.println("║  ⚠ SAFETY MODE — KÍCH HOẠT              ║");
            Serial.printf( "║  Lý do: %-32s ║\n", reason);
            Serial.println("║  Hệ thống chạy chế độ giảm, web vẫn ok  ║");
            Serial.println("╚══════════════════════════════════════════╝");
        }
    } else {
        // Không còn điều kiện lỗi
        if (safetyActive) {
            // Cần N chu kỳ ổn định liên tiếp để thoát (hysteresis)
            stableCycleCount++;

            if (stableCycleCount >= SAFETY_STABLE_CYCLES_TO_EXIT) {
                // === THOÁT Safety Mode ===
                safetyActive     = false;
                justEntered      = false;
                stableCycleCount = 0;

                Serial.println("╔══════════════════════════════════════════╗");
                Serial.println("║  ✔ SAFETY MODE — ĐÃ THOÁT               ║");
                Serial.printf( "║  Ổn định %d chu kỳ liên tiếp             ║\n",
                               SAFETY_STABLE_CYCLES_TO_EXIT);
                Serial.println("╚══════════════════════════════════════════╝");
            } else {
                Serial.printf("[SAFETY] Đang ổn định: %d/%d chu kỳ\n",
                              stableCycleCount, SAFETY_STABLE_CYCLES_TO_EXIT);
            }
        }
    }

    return safetyActive;
}

bool isSafetyModeActive() {
    return safetyActive;
}
