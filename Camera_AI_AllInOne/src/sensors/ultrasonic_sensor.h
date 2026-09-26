#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

#include <Arduino.h>

// ===================================================================
//  MODULE CẢM BIẾN SIÊU ÂM HC-SR04 — Phát hiện người đi ngang cửa
//
//  ⚠ GIỚI HẠN QUAN TRỌNG:
//    - CHỈ phát hiện có người đi qua, KHÔNG phân biệt hướng vào/ra.
//    - Biến đếm crossingCount là tổng số lượt cắt ngang cộng dồn,
//      KHÔNG phải số người hiện tại trong phòng.
//    - Để biết số người thực tế → dùng kết hợp với AI (module fusion).
//
//  Thuật toán:
//  ┌─────────────────────────────────────────────────────────┐
//  │  State Machine 2 trạng thái: FAR / NEAR                │
//  │                                                         │
//  │  FAR ──(khoảng cách < PRESENCE_THRESHOLD_CM)──► NEAR    │
//  │  NEAR ──(khoảng cách >= PRESENCE_THRESHOLD_CM            │
//  │          VÀ đã ở NEAR >= MIN_PRESENCE_MS)──► FAR        │
//  │          → Hoàn tất 1 chu kỳ NEAR→FAR → crossingCount++ │
//  │                                                         │
//  │  Polling mỗi ULTRASONIC_POLL_MS (50ms mặc định)        │
//  │  Debounce MIN_PRESENCE_MS (150ms) để lọc nhiễu          │
//  └─────────────────────────────────────────────────────────┘
// ===================================================================

/// Khởi tạo chân GPIO cho HC-SR04 (TRIG=OUTPUT, ECHO=INPUT)
void ultrasonicSensorInit();

/// Cập nhật state machine — GỌI THƯỜNG XUYÊN trong loop/task
/// Hàm này tự kiểm tra thời gian polling, không block nếu chưa đến chu kỳ.
void ultrasonicSensorUpdate();

/// Lấy tổng số lượt cắt ngang cộng dồn (crossing count).
/// ⚠ Đây KHÔNG phải số người hiện tại — chỉ là tổng lượt phát hiện.
/// Hàm này thread-safe (dùng spinlock).
int getUltrasonicCrossingCount();

/// Reset bộ đếm crossing về giá trị chỉ định (mặc định 0)
void resetUltrasonicCount(int value = 0);

/// Kiểm tra có hoạt động gần đây không (người vừa đi qua trong vài giây gần đây)
/// Dùng làm cờ xác nhận cho module fusion.
/// @param windowMs  Khoảng thời gian kiểm tra (ms), mặc định 10 giây
/// @return true nếu có crossing xảy ra trong windowMs gần nhất
bool hasRecentActivity(uint32_t windowMs = 10000);

/// Lấy khoảng cách đo được gần nhất (cm). 0 = timeout / không đo được.
float getLastDistanceCm();

#endif // ULTRASONIC_SENSOR_H
