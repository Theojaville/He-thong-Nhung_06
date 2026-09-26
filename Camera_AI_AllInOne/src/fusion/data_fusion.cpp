#include "data_fusion.h"
#include "app_config.h"
#include <math.h>

// ===================================================================
//  Triển khai module Fusion — AI là nguồn chính, siêu âm xác nhận
//
//  LOGIC MỚI (thay thế trung bình có trọng số 0.7*IR+0.3*AI):
//    - finalCount = aiCount (AI quyết định số người)
//    - Siêu âm chỉ dùng làm cờ xác nhận hoạt động (hasActivity)
//    - Phát hiện sai lệch khi:
//      a) AI=0 nhưng siêu âm phát hiện crossing gần đây → cảnh báo
//      b) AI>ngưỡng nhưng siêu âm im lặng lâu → cảnh báo
// ===================================================================

// Kết quả fusion gần nhất (cache)
static FusionResult lastResult;

// Lưu crossingCount lần trước để phát hiện thay đổi
static int prevCrossingCount = 0;

void fusionInit() {
    memset(&lastResult, 0, sizeof(FusionResult));
    lastResult.roomStatus = ROOM_AVAILABLE;
    prevCrossingCount = 0;

    Serial.println("[FUSION] Khởi tạo module dung hợp dữ liệu.");
    Serial.printf("[FUSION]   Nguồn chính: AI (camera)\n");
    Serial.printf("[FUSION]   Nguồn phụ: Siêu âm HC-SR04 (xác nhận hoạt động)\n");
    Serial.printf("[FUSION]   Sức chứa phòng: %d người\n", ROOM_MAX_CAPACITY);
    Serial.printf("[FUSION]   Ngưỡng sai lệch: %d người\n", DISCREPANCY_THRESHOLD);
}

FusionResult fusionUpdate(int crossingCount, bool hasActivity, int aiCount) {
    FusionResult r;
    r.crossingCount   = crossingCount;
    r.hasActivity     = hasActivity;
    r.aiCount         = aiCount;

    // ---------------------------------------------------------------
    //  LOGIC QUYẾT ĐỊNH GIÁ TRỊ CUỐI CÙNG (finalCount)
    //
    //  AI là nguồn chính → finalCount = aiCount
    //  Siêu âm chỉ dùng để phát hiện sai lệch / xác nhận
    // ---------------------------------------------------------------
    r.finalCount = aiCount;

    // Clamp: không cho âm
    if (r.finalCount < 0) r.finalCount = 0;

    // ---------------------------------------------------------------
    //  PHÁT HIỆN SAI LỆCH
    //
    //  Case 1: AI = 0 nhưng siêu âm vừa phát hiện crossing gần đây
    //    → Có thể có người mà AI bỏ sót
    //
    //  Case 2: AI > DISCREPANCY_THRESHOLD nhưng siêu âm im lặng lâu
    //    VÀ có crossing xảy ra trước đó (tức là hệ thống đang theo dõi)
    //    → AI có thể bị ảo (false positive)
    //
    //  Giá trị discrepancy: dùng để log/debug, không dùng để tính
    // ---------------------------------------------------------------

    bool newCrossing = (crossingCount > prevCrossingCount);
    prevCrossingCount = crossingCount;

    r.discrepancy = 0;
    r.discrepancyFlag = false;

    // Case 1: AI=0, nhưng siêu âm mới detect crossing
    if (aiCount == 0 && hasActivity && newCrossing) {
        r.discrepancyFlag = true;
        r.discrepancy = crossingCount;   // Để debug
        Serial.printf("[FUSION] ⚠ AI=0 nhưng siêu âm phát hiện crossing! (count=%d)\n",
                      crossingCount);
    }

    // Case 2: AI nhiều, siêu âm im
    if (aiCount > DISCREPANCY_THRESHOLD && !hasActivity && crossingCount > 0) {
        r.discrepancyFlag = true;
        r.discrepancy = aiCount;
        Serial.printf("[FUSION] ⚠ AI=%d nhưng siêu âm im lặng! Có thể AI false positive.\n",
                      aiCount);
    }

    // ---------------------------------------------------------------
    //  XÁC ĐỊNH TRẠNG THÁI PHÒNG
    // ---------------------------------------------------------------
    r.occupancyPercent = (float)r.finalCount / (float)ROOM_MAX_CAPACITY * 100.0f;

    if (r.discrepancyFlag) {
        r.roomStatus = ROOM_DISCREPANCY;
    }
    else if (r.finalCount >= ROOM_MAX_CAPACITY) {
        r.roomStatus = ROOM_FULL;
    }
    else if (r.occupancyPercent >= 80.0f) {
        r.roomStatus = ROOM_NEARLY_FULL;
    }
    else {
        r.roomStatus = ROOM_AVAILABLE;
    }

    // Lưu cache
    lastResult = r;

    return r;
}

FusionResult getLastFusionResult() {
    return lastResult;
}

const char* roomStatusToString(RoomStatus status) {
    switch (status) {
        case ROOM_AVAILABLE:    return "CON_CHO";
        case ROOM_NEARLY_FULL:  return "GAN_DAY";
        case ROOM_FULL:         return "DA_DAY";
        case ROOM_DISCREPANCY:  return "SAI_LECH";
        default:                return "UNKNOWN";
    }
}
