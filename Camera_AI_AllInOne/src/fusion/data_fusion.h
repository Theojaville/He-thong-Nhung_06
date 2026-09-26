#ifndef DATA_FUSION_H
#define DATA_FUSION_H

#include <Arduino.h>
#include "ai/image_utils.h"   // Dùng struct BBox

// ===================================================================
//  MODULE DUNG HỢP DỮ LIỆU (FUSION) — AI chính + Siêu âm xác nhận
//
//  Vai trò:
//    - AI (camera) là NGUỒN CHÍNH cho occupancy hiện tại
//    - Cảm biến siêu âm chỉ là CỜ XÁC NHẬN hoạt động (hasRecentActivity)
//    - KHÔNG dùng công thức trung bình có trọng số 0.7*IR+0.3*AI cũ
//
//  Quy tắc ưu tiên:
//    - AI quyết định số người (finalCount = aiCount)
//    - Cảm biến siêu âm xác nhận có hoạt động tại cửa hay không
//    - Nếu AI báo 0 nhưng siêu âm phát hiện crossing → cảnh báo sai lệch
//    - Nếu AI báo nhiều nhưng siêu âm im lặng lâu → cảnh báo sai lệch
// ===================================================================

/// Trạng thái phòng
enum RoomStatus {
    ROOM_AVAILABLE,       // Còn chỗ trống
    ROOM_NEARLY_FULL,     // Gần đầy (>= 80% sức chứa)
    ROOM_FULL,            // Đã đầy (>= 100%)
    ROOM_DISCREPANCY      // Có sai lệch bất thường giữa sensor và AI
};

/// Cấu trúc chứa kết quả dung hợp
struct FusionResult {
    int         crossingCount;     // Tổng lượt cắt ngang (từ siêu âm, cộng dồn)
    bool        hasActivity;       // Có hoạt động gần đây tại cửa không
    int         aiCount;           // Số người theo AI
    int         finalCount;        // Giá trị cuối cùng (= aiCount, AI là nguồn chính)
    int         discrepancy;       // Giá trị sai lệch (để log/debug)
    bool        discrepancyFlag;   // true nếu phát hiện sai lệch bất thường
    RoomStatus  roomStatus;        // Trạng thái phòng
    float       occupancyPercent;  // Phần trăm lấp đầy (0–100+)
};

/// Khởi tạo module fusion
void fusionInit();

/// Cập nhật dung hợp dữ liệu — gọi sau mỗi lần AI inference xong
/// @param crossingCount   Tổng lượt crossing từ siêu âm (cộng dồn)
/// @param hasActivity     Cờ có hoạt động gần đây (từ hasRecentActivity())
/// @param aiCount         Số người từ kết quả AI mới nhất
/// @return                Kết quả dung hợp
FusionResult fusionUpdate(int crossingCount, bool hasActivity, int aiCount);

/// Lấy kết quả fusion gần nhất (không tính lại)
FusionResult getLastFusionResult();

/// Lấy trạng thái phòng dưới dạng chuỗi (cho log/web)
const char* roomStatusToString(RoomStatus status);

#endif // DATA_FUSION_H
