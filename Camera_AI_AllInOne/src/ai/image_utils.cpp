#include "image_utils.h"
#include <math.h>

// ===================================================================
//  Triển khai tiện ích ảnh — Vẽ BBox, Normalize, IoU
// ===================================================================

// ---- Hàm nội bộ: vẽ 1 đường ngang trên buffer RGB888 ----
static void drawHLine(uint8_t* rgb, int imgW, int imgH,
                      int x0, int x1, int y,
                      uint8_t r, uint8_t g, uint8_t b) {
    if (y < 0 || y >= imgH) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= imgW) x1 = imgW - 1;
    for (int x = x0; x <= x1; x++) {
        int idx = (y * imgW + x) * 3;
        rgb[idx + 0] = r;
        rgb[idx + 1] = g;
        rgb[idx + 2] = b;
    }
}

// ---- Hàm nội bộ: vẽ 1 đường dọc trên buffer RGB888 ----
static void drawVLine(uint8_t* rgb, int imgW, int imgH,
                      int x, int y0, int y1,
                      uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= imgW) return;
    if (y0 < 0) y0 = 0;
    if (y1 >= imgH) y1 = imgH - 1;
    for (int y = y0; y <= y1; y++) {
        int idx = (y * imgW + x) * 3;
        rgb[idx + 0] = r;
        rgb[idx + 1] = g;
        rgb[idx + 2] = b;
    }
}

void drawBBox(uint8_t* rgb888, int imgW, int imgH,
              const BBox& box, uint8_t r, uint8_t g, uint8_t b,
              int thickness) {
    if (!rgb888 || imgW <= 0 || imgH <= 0) return;

    // Chuyển tọa độ tỷ lệ (0–1) → pixel
    int cx = (int)(box.x * imgW);
    int cy = (int)(box.y * imgH);
    int bw = (int)(box.w * imgW);
    int bh = (int)(box.h * imgH);

    // Tọa độ góc trên-trái và góc dưới-phải
    int x0 = cx - bw / 2;
    int y0 = cy - bh / 2;
    int x1 = cx + bw / 2;
    int y1 = cy + bh / 2;

    // Clamp vào biên ảnh
    x0 = max(0, min(x0, imgW - 1));
    y0 = max(0, min(y0, imgH - 1));
    x1 = max(0, min(x1, imgW - 1));
    y1 = max(0, min(y1, imgH - 1));

    // Vẽ 4 cạnh hình chữ nhật với độ dày `thickness`
    for (int t = 0; t < thickness; t++) {
        drawHLine(rgb888, imgW, imgH, x0, x1, y0 + t, r, g, b);   // Cạnh trên
        drawHLine(rgb888, imgW, imgH, x0, x1, y1 - t, r, g, b);   // Cạnh dưới
        drawVLine(rgb888, imgW, imgH, x0 + t, y0, y1, r, g, b);   // Cạnh trái
        drawVLine(rgb888, imgW, imgH, x1 - t, y0, y1, r, g, b);   // Cạnh phải
    }
}

void normalizeImage(const uint8_t* src, float* dst, int numPixels,
                    float scale, float offset) {
    if (!src || !dst) return;
    for (int i = 0; i < numPixels; i++) {
        dst[i] = (float)src[i] * scale + offset;
    }
}

float computeIoU(const BBox& a, const BBox& b) {
    // Chuyển từ (cx, cy, w, h) → (x0, y0, x1, y1)
    float a_x0 = a.x - a.w / 2.0f, a_y0 = a.y - a.h / 2.0f;
    float a_x1 = a.x + a.w / 2.0f, a_y1 = a.y + a.h / 2.0f;
    float b_x0 = b.x - b.w / 2.0f, b_y0 = b.y - b.h / 2.0f;
    float b_x1 = b.x + b.w / 2.0f, b_y1 = b.y + b.h / 2.0f;

    // Tính vùng giao (intersection)
    float inter_x0 = fmaxf(a_x0, b_x0);
    float inter_y0 = fmaxf(a_y0, b_y0);
    float inter_x1 = fminf(a_x1, b_x1);
    float inter_y1 = fminf(a_y1, b_y1);

    float interW = fmaxf(0.0f, inter_x1 - inter_x0);
    float interH = fmaxf(0.0f, inter_y1 - inter_y0);
    float interArea = interW * interH;

    // Diện tích mỗi box
    float areaA = a.w * a.h;
    float areaB = b.w * b.h;

    // IoU = Intersection / Union
    float unionArea = areaA + areaB - interArea;
    if (unionArea <= 0.0f) return 0.0f;

    return interArea / unionArea;
}
