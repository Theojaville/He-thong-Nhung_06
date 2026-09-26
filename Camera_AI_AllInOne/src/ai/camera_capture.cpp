#include "camera_capture.h"
#include "app_config.h"
#include "camera_config.h"

// ===================================================================
//  Triển khai module Camera — OV2640 + PSRAM buffer
// ===================================================================

// Buffer RGB888 full-size dùng chung, cấp phát 1 lần trong PSRAM
// (tránh ps_malloc/free liên tục mỗi chu kỳ AI → tránh phân mảnh PSRAM)
static uint8_t* s_rgbFullBuffer = nullptr;
static size_t   s_rgbFullBufferSize = 0;

bool cameraInit() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_1;   // Tránh trùng LEDC channel 0 (audio)
    config.ledc_timer   = LEDC_TIMER_1;
    config.pin_d0       = CAM_PIN_D0;
    config.pin_d1       = CAM_PIN_D1;
    config.pin_d2       = CAM_PIN_D2;
    config.pin_d3       = CAM_PIN_D3;
    config.pin_d4       = CAM_PIN_D4;
    config.pin_d5       = CAM_PIN_D5;
    config.pin_d6       = CAM_PIN_D6;
    config.pin_d7       = CAM_PIN_D7;
    config.pin_xclk     = CAM_PIN_XCLK;
    config.pin_pclk     = CAM_PIN_PCLK;
    config.pin_vsync    = CAM_PIN_VSYNC;
    config.pin_href     = CAM_PIN_HREF;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_pwdn     = CAM_PIN_PWDN;
    config.pin_reset    = CAM_PIN_RESET;

    config.xclk_freq_hz = CAM_XCLK_FREQ;    // 20 MHz
    config.pixel_format = PIXFORMAT_RGB565;  // RGB565: Tương thích tất cả các loại cảm biến (GC0308, OV7670, OV7725, OV2640...)

    // Cấu hình chất lượng & kích thước theo khả năng PSRAM
    if (psramFound()) {
        Serial.println("[CAM] PSRAM detected → dùng QVGA + 2 framebuffer");
        config.frame_size   = FRAMESIZE_QVGA;    // 320×240 — kích thước chuẩn cho streaming & AI
        config.jpeg_quality = 12;                 // Không ảnh hưởng tới RGB565
        config.fb_count     = 2;                  // Double buffer → stream mượt hơn
        config.fb_location  = CAMERA_FB_IN_PSRAM;
    } else {
        Serial.println("[CAM] ⚠ PSRAM KHÔNG tìm thấy! Chất lượng giảm.");
        config.frame_size   = FRAMESIZE_QQVGA;   // 160×120 — tiết kiệm RAM
        config.jpeg_quality = 15;
        config.fb_count     = 1;
        config.fb_location  = CAMERA_FB_IN_DRAM;
    }

    config.grab_mode = CAMERA_GRAB_LATEST;   // Luôn lấy frame mới nhất

    // Khởi tạo camera driver
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAM] ✖ Lỗi khởi tạo camera: 0x%x\n", err);
        return false;
    }

    // Tinh chỉnh sensor (tùy chọn)
    sensor_t* s = esp_camera_sensor_get();
    if (s != nullptr) {
        s->set_brightness(s, 1);      // Tăng sáng nhẹ (+1)
        s->set_contrast(s, 1);        // Tăng tương phản nhẹ
        s->set_saturation(s, 0);      // Giữ nguyên độ bão hòa
        s->set_whitebal(s, 1);        // Bật cân bằng trắng tự động
        s->set_awb_gain(s, 1);        // Bật AWB gain
        s->set_exposure_ctrl(s, 1);   // Bật điều khiển phơi sáng tự động
        s->set_aec2(s, 1);            // Bật AEC DSP
        s->set_gain_ctrl(s, 1);       // Bật AGC (tự động tăng gain)
    }

    Serial.println("[CAM] ✔ Camera khởi tạo thành công!");
    return true;
}

camera_fb_t* captureJPEG() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[CAM] ✖ Chụp ảnh thất bại (fb = NULL)");
        return nullptr;
    }
    return fb;
}

void releaseFrame(camera_fb_t* fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

// Đảm bảo buffer RGB full-size đủ lớn cho (w × h), cấp phát lại nếu cần
static bool ensureRgbFullBuffer(size_t neededSize) {
    if (s_rgbFullBuffer != nullptr && s_rgbFullBufferSize >= neededSize) {
        return true;   // Buffer hiện tại đã đủ lớn, dùng lại
    }

    // Cần cấp phát mới (lần đầu, hoặc frame size camera đã đổi)
    if (s_rgbFullBuffer != nullptr) {
        free(s_rgbFullBuffer);
        s_rgbFullBuffer = nullptr;
        s_rgbFullBufferSize = 0;
    }

    s_rgbFullBuffer = (uint8_t*)ps_malloc(neededSize);
    if (!s_rgbFullBuffer) {
        Serial.printf("[CAM] ✖ Không đủ PSRAM cho RGB buffer (%d bytes)\n", neededSize);
        return false;
    }

    s_rgbFullBufferSize = neededSize;
    Serial.printf("[CAM] ✔ Cấp phát RGB buffer: %d bytes (PSRAM)\n", neededSize);
    return true;
}

// Resize nearest-neighbor từ (srcW×srcH) RGB888 → (targetW×targetH) RGB888
static void resizeRgb888(const uint8_t* src, int srcW, int srcH,
                          uint8_t* dst, int targetW, int targetH) {
    for (int y = 0; y < targetH; y++) {
        int srcY = y * srcH / targetH;
        for (int x = 0; x < targetW; x++) {
            int srcX = x * srcW / targetW;
            int srcIdx = (srcY * srcW + srcX) * 3;
            int dstIdx = (y * targetW + x) * 3;

            dst[dstIdx + 0] = src[srcIdx + 0];   // R
            dst[dstIdx + 1] = src[srcIdx + 1];   // G
            dst[dstIdx + 2] = src[srcIdx + 2];   // B
        }
    }
}

bool captureAndResizeRGB(uint8_t* outRgb, int targetW, int targetH) {
    if (!outRgb) {
        Serial.println("[CAM] ✖ captureAndResizeRGB: outRgb = NULL");
        return false;
    }

    // --- Chụp frame (RGB565 / JPEG) → Chuyển sang RGB888 → Resize ---
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[CAM] ✖ captureAndResizeRGB: chụp thất bại (fb = NULL)");
        return false;
    }

    if (fb->buf == nullptr || fb->len == 0) {
        Serial.println("[CAM] ✖ Frame không hợp lệ (rỗng)");
        esp_camera_fb_return(fb);
        return false;
    }

    int srcW = fb->width;
    int srcH = fb->height;
    size_t rgbBufSize = (size_t)srcW * srcH * 3;   // RGB888: 3 byte/pixel

    if (!ensureRgbFullBuffer(rgbBufSize)) {
        esp_camera_fb_return(fb);
        return false;
    }

    // fmt2rgb888 tự động xử lý cả PIXFORMAT_RGB565, PIXFORMAT_JPEG, PIXFORMAT_YUV422
    bool decoded = fmt2rgb888(fb->buf, fb->len, fb->format, s_rgbFullBuffer);

    // Trả frame buffer sớm để camera có thể chụp tiếp
    esp_camera_fb_return(fb);

    if (!decoded) {
        Serial.println("[CAM] ✖ Chuyển đổi sang RGB888 thất bại");
        return false;
    }

    // Bước 2: Resize từ (srcW × srcH) → (targetW × targetH)
    resizeRgb888(s_rgbFullBuffer, srcW, srcH, outRgb, targetW, targetH);

    Serial.printf("[CAM] ✔ Resize ảnh: %dx%d → %dx%d\n", srcW, srcH, targetW, targetH);
    return true;
}