#ifndef SAFETY_MODE_H
#define SAFETY_MODE_H

#include <Arduino.h>

// ===================================================================
//  MODULE SAFETY MODE — Bảo vệ hệ thống khi gặp sự cố
//
//  Phát hiện các tình huống bất thường và kích hoạt chế độ an toàn:
//    1. AI init thất bại
//    2. Camera capture thất bại liên tiếp >= SAFETY_CAM_FAIL_LIMIT
//    3. Free heap < SAFETY_MIN_FREE_HEAP_BYTES
//    4. Sai lệch liên tục quá SAFETY_DISCREPANCY_DURATION_MS
//
//  Khi Safety Mode kích hoạt:
//    - LED hiển thị pattern đặc biệt (đỏ+vàng luân phiên)
//    - Phát 1 âm cảnh báo (chỉ khi vừa vào mode, không lặp)
//    - Hệ thống VẪN tiếp tục hoạt động (web server vẫn chạy)
//    - KHÔNG khóa cứng bằng while(true)
//
//  Thoát Safety Mode:
//    - Cần N chu kỳ liên tiếp ổn định (hysteresis đơn giản)
//    - Tránh dao động bật/tắt liên tục
// ===================================================================

/// Khởi tạo module Safety Mode
void safetyModeInit();

/// Cập nhật trạng thái Safety Mode — gọi mỗi chu kỳ trong taskAI
/// @param aiInitOk       AI đã khởi tạo thành công hay chưa
/// @param lastCaptureOk  Lần chụp ảnh gần nhất có thành công không
/// @param freeHeap       Heap khả dụng hiện tại (bytes)
/// @param discrepancyFlag Cờ sai lệch từ module fusion
/// @return true nếu Safety Mode đang active
bool safetyModeUpdate(bool aiInitOk, bool lastCaptureOk,
                      uint32_t freeHeap, bool discrepancyFlag);

/// Kiểm tra Safety Mode có đang kích hoạt không
bool isSafetyModeActive();

#endif // SAFETY_MODE_H
