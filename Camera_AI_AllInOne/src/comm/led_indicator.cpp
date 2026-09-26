#include "led_indicator.h"
#include "app_config.h"

// ===================================================================
//  Triển khai LED chỉ thị — 3 LED đèn giao thông
//
//  Nhấp nháy non-blocking dùng millis():
//    - ROOM_FULL:       Đỏ nhấp nháy 500ms ON / 500ms OFF
//    - DISCREPANCY:     Vàng + Đỏ nhấp nháy đồng thời 300ms
//    - SAFETY_MODE:     Đỏ + Vàng luân phiên 400ms (đỏ ON→vàng ON→...)
// ===================================================================

// ---------- Biến trạng thái nhấp nháy ----------
static uint32_t blinkTimer   = 0;
static bool     blinkState   = false;   // toggle cho nhấp nháy

// Chu kỳ nhấp nháy (ms) cho từng chế độ
static const uint32_t BLINK_FULL_MS        = 500;   // ROOM_FULL: nháy chậm
static const uint32_t BLINK_DISCREPANCY_MS = 300;   // DISCREPANCY: nháy nhanh hơn
static const uint32_t BLINK_SAFETY_MS      = 400;   // SAFETY: nháy luân phiên

// ===================================================================
//  Hàm nội bộ
// ===================================================================

/// Tắt tất cả 3 LED
static void allOff() {
    digitalWrite(PIN_LED_GREEN,  LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED,    LOW);
}

/// Cập nhật blink toggle theo chu kỳ (non-blocking)
/// @return true nếu vừa toggle (để caller biết cần cập nhật LED)
static bool updateBlink(uint32_t periodMs) {
    uint32_t now = millis();
    if (now - blinkTimer >= periodMs) {
        blinkTimer = now;
        blinkState = !blinkState;
        return true;
    }
    return false;
}

// ===================================================================
//  Hàm công khai (Public API)
// ===================================================================

void ledIndicatorInit() {
    pinMode(PIN_LED_GREEN,  OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_RED,    OUTPUT);

    allOff();

    Serial.println("[LED] Khởi tạo LED đèn giao thông hoàn tất.");
    Serial.printf("[LED]   GREEN = GPIO %d | YELLOW = GPIO %d | RED = GPIO %d\n",
                  PIN_LED_GREEN, PIN_LED_YELLOW, PIN_LED_RED);
}

void updateStatusLEDs(float occupancyPercent, RoomStatus status) {
    switch (status) {

    case ROOM_DISCREPANCY:
        // Vàng + Đỏ nhấp nháy đồng thời — kiểu nháy riêng
        updateBlink(BLINK_DISCREPANCY_MS);
        digitalWrite(PIN_LED_GREEN,  LOW);
        digitalWrite(PIN_LED_YELLOW, blinkState ? HIGH : LOW);
        digitalWrite(PIN_LED_RED,    blinkState ? HIGH : LOW);
        break;

    case ROOM_FULL:
        // Đỏ sáng nhấp nháy
        updateBlink(BLINK_FULL_MS);
        digitalWrite(PIN_LED_GREEN,  LOW);
        digitalWrite(PIN_LED_YELLOW, LOW);
        digitalWrite(PIN_LED_RED,    blinkState ? HIGH : LOW);
        break;

    case ROOM_NEARLY_FULL:
        // Vàng sáng ổn định (80–99%)
        digitalWrite(PIN_LED_GREEN,  LOW);
        digitalWrite(PIN_LED_YELLOW, HIGH);
        digitalWrite(PIN_LED_RED,    LOW);
        break;

    case ROOM_AVAILABLE:
    default:
        // Xanh sáng ổn định (< 80%)
        digitalWrite(PIN_LED_GREEN,  HIGH);
        digitalWrite(PIN_LED_YELLOW, LOW);
        digitalWrite(PIN_LED_RED,    LOW);
        break;
    }
}

void updateStatusLEDs_SafetyMode() {
    // Đỏ + Vàng nhấp nháy LUÂN PHIÊN — khác kiểu nháy FULL/DISCREPANCY
    // Phase A: Đỏ ON, Vàng OFF
    // Phase B: Đỏ OFF, Vàng ON
    updateBlink(BLINK_SAFETY_MS);

    digitalWrite(PIN_LED_GREEN, LOW);

    if (blinkState) {
        // Phase A: Đỏ sáng
        digitalWrite(PIN_LED_RED,    HIGH);
        digitalWrite(PIN_LED_YELLOW, LOW);
    } else {
        // Phase B: Vàng sáng
        digitalWrite(PIN_LED_RED,    LOW);
        digitalWrite(PIN_LED_YELLOW, HIGH);
    }
}
