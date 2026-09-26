#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <Arduino.h>

// ===================================================================
//  MODULE TIỆN ÍCH ẢNH — Vẽ Bounding Box + chuyển đổi màu
//
//  Dùng để:
//    - Vẽ khung chữ nhật (Bounding Box) lên buffer RGB888
//    - Chuẩn hóa (normalize) pixel cho input model AI
//    - Chuyển đổi tọa độ box từ tỷ lệ model → tỷ lệ ảnh gốc
// ===================================================================

/// Cấu trúc 1 bounding box đã detect
struct BBox {
    float x;           // Tọa độ x trung tâm (0.0 – 1.0, tỷ lệ so với ảnh)
    float y;           // Tọa độ y trung tâm
    float w;           // Chiều rộng (0.0 – 1.0)
    float h;           // Chiều cao (0.0 – 1.0)
    float confidence;  // Độ tin cậy (0.0 – 1.0)
};

/// Vẽ 1 bounding box (hình chữ nhật) lên buffer RGB888.
/// @param rgb888   Buffer ảnh RGB888 (3 byte/pixel)
/// @param imgW     Chiều rộng ảnh (pixel)
/// @param imgH     Chiều cao ảnh (pixel)
/// @param box      Bounding box cần vẽ (tọa độ tỷ lệ 0–1)
/// @param r,g,b    Màu khung (VD: 255,0,0 = đỏ)
/// @param thickness Độ dày nét vẽ (pixel)
void drawBBox(uint8_t* rgb888, int imgW, int imgH,
              const BBox& box, uint8_t r, uint8_t g, uint8_t b,
              int thickness = 2);

/// Chuẩn hóa (normalize) ảnh RGB888 sang float [-1, 1] hoặc [0, 1]
/// cho input model AI.
/// @param src       Ảnh RGB888 gốc (uint8_t)
/// @param dst       Buffer float đầu ra (phải cấp phát trước)
/// @param numPixels Tổng số pixel (W × H × Channels)
/// @param scale     Hệ số nhân (VD: 1.0/255.0 cho [0,1] hoặc 1.0/127.5 cho [-1,1])
/// @param offset    Hệ số cộng sau scale (VD: 0.0 cho [0,1] hoặc -1.0 cho [-1,1])
void normalizeImage(const uint8_t* src, float* dst, int numPixels,
                    float scale = 1.0f / 255.0f, float offset = 0.0f);

/// Tính IoU (Intersection over Union) giữa 2 bounding box.
/// Dùng cho thuật toán Non-Max Suppression (NMS).
float computeIoU(const BBox& a, const BBox& b);

#endif // IMAGE_UTILS_H
