#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

#include <Arduino.h>
#include "fusion/data_fusion.h"

// ===================================================================
//  MODULE LED CHỈ THỊ — 3 LED đèn giao thông (Xanh / Vàng / Đỏ)
//
//  Hiển thị trực quan trạng thái phòng:
//    - < 80%        → Xanh sáng
//    - 80–99%       → Vàng sáng
//    - ≥ 100% (ĐẦY) → Đỏ sáng, nhấp nháy non-blocking
//    - SAI_LECH      → Vàng + Đỏ nhấp nháy đồng thời (kiểu riêng)
//    - SAFETY_MODE   → Đỏ + Vàng nhấp nháy luân phiên (kiểu khác)
//
//  LED heartbeat (PIN_LED_STATUS) vẫn giữ nguyên riêng để debug.
//  Tất cả nhấp nháy dùng millis(), KHÔNG dùng delay().
// ===================================================================

/// Khởi tạo chân GPIO cho 3 LED
void ledIndicatorInit();

/// Cập nhật LED theo trạng thái phòng (gọi mỗi chu kỳ trong taskAI/loop)
/// @param occupancyPercent  Phần trăm lấp đầy (0–100+)
/// @param status            Trạng thái phòng từ module fusion
void updateStatusLEDs(float occupancyPercent, RoomStatus status);

/// Chế độ LED đặc biệt cho Safety Mode
/// Đỏ + Vàng nhấp nháy luân phiên — khác kiểu nháy ROOM_FULL/DISCREPANCY
void updateStatusLEDs_SafetyMode();

#endif // LED_INDICATOR_H
