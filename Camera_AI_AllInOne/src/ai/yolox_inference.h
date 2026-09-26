#ifndef YOLOX_INFERENCE_H
#define YOLOX_INFERENCE_H

#include <Arduino.h>
#include "app_config.h"

#if __has_include("ai/image_utils.h")
#include "ai/image_utils.h"
#else
#include "image_utils.h"
#endif

// ===================================================================
//  MODULE SUY LUẬN AI — YoloX-nano (TFLite Micro)
//
//  Pipeline:
//    1. Nhận ảnh RGB888 đã resize (96×96×3)
//    2. Chuẩn hóa (normalize) pixel → input tensor
//    3. Chạy inference (suy luận mạng neural)
//    4. Parse output tensor → danh sách bounding box thô
//    5. Lọc theo class "person" (index 0) + ngưỡng confidence
//    6. Áp dụng Non-Max Suppression (NMS) loại box trùng lặp
//    7. Trả về số người phát hiện + danh sách bounding box
//
//  Kết quả suy luận:
//    - personCount     : Số người phát hiện được
//    - detections[]    : Mảng bounding box (tối đa AI_MAX_DETECTIONS)
//    - inferenceTimeMs : Thời gian xử lý (ms)
// ===================================================================

/// Cấu trúc chứa kết quả 1 lần suy luận
struct InferenceResult {
    int      personCount;                      // Số người phát hiện
    BBox     detections[AI_MAX_DETECTIONS];    // Mảng bounding box (theo macro trong app_config.h)
    int      detectionCount;                   // Số box thực tế trong mảng
    uint32_t inferenceTimeMs;                  // Thời gian suy luận (ms)
    bool     success;                          // Suy luận thành công hay lỗi
};

/// Khởi tạo TFLite Micro: load model, cấp phát tensor arena, tạo interpreter.
/// Tensor arena được cấp phát trong PSRAM.
/// Trả về true nếu thành công.
bool inferenceInit();

/// Chạy suy luận trên ảnh RGB888 kích thước 96×96.
/// @param rgbInput   Ảnh RGB888 đã resize (kích thước = AI_INPUT_W * AI_INPUT_H * 3)
/// @return           Kết quả suy luận (struct InferenceResult)
InferenceResult runInference(const uint8_t* rgbInput);

/// Giải phóng tài nguyên TFLite (tensor arena, interpreter)
void inferenceCleanup();

/// Lấy FPS trung bình (dựa trên N lần suy luận gần nhất)
float getAverageFPS();

#endif // YOLOX_INFERENCE_H
