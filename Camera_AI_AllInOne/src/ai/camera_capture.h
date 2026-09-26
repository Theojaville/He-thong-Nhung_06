#ifndef CAMERA_CAPTURE_H
#define CAMERA_CAPTURE_H

#include <Arduino.h>
#include "esp_camera.h"
#include "img_converters.h"

// ===================================================================
//  MODULE CHỤP ẢNH — Camera OV2640 trên ESP32-CAM AI-Thinker
//
//  Chức năng:
//    - Khởi tạo camera với cấu hình AI-Thinker
//    - Chụp 1 khung hình JPEG (dùng cho MJPEG streaming)
//    - Chụp 1 khung hình RGB565 (dùng cho AI inference)
//    - Giải phóng buffer sau khi dùng xong
//
//  Lưu ý bộ nhớ:
//    - Frame buffer được cấp phát trong PSRAM
//    - Luôn gọi releaseFrame() sau khi dùng xong captureJPEG()
// ===================================================================

/// Khởi tạo camera. Trả về true nếu thành công.
bool cameraInit();

/// Chụp 1 khung hình JPEG. Trả về con trỏ camera_fb_t*.
/// ⚠ PHẢI gọi releaseFrame() sau khi dùng xong!
camera_fb_t* captureJPEG();

/// Giải phóng frame buffer (trả lại cho camera driver)
void releaseFrame(camera_fb_t* fb);

/// Chụp ảnh và chuyển sang mảng RGB888 kích thước AI_INPUT_W × AI_INPUT_H.
/// Dữ liệu được ghi vào buffer `outRgb` (phải cấp phát trước, kích thước
/// AI_INPUT_W * AI_INPUT_H * 3 bytes).
/// Trả về true nếu thành công.
bool captureAndResizeRGB(uint8_t* outRgb, int targetW, int targetH);

#endif // CAMERA_CAPTURE_H
