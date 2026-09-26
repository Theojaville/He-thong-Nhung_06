#ifndef CAMERA_CONFIG_H
#define CAMERA_CONFIG_H

// ===================================================================
//  Cấu hình chân Camera — Board ESP32-S3 WROOM N16R8 CAM
//  Module camera: OV2640 / OV3660 (tùy board)
//  Bộ nhớ: 16MB Flash + 8MB PSRAM
//
//  ⚠ KHÔNG thay đổi các giá trị này trừ khi dùng board khác.
// ===================================================================

#define CAMERA_MODEL_ESP32S3_WROOM_CAM    // Identifier cho board

// --- Chân điều khiển ---
#define CAM_PIN_PWDN       38     // Power Down (active HIGH)
#define CAM_PIN_RESET      -1     // Reset (không dùng, -1 = bỏ qua)
#define CAM_PIN_XCLK       15     // Clock ngoài cho camera

// --- Chân I2C (SCCB) — giao tiếp cấu hình camera ---
#define CAM_PIN_SIOD        4     // I2C Data  (SDA)
#define CAM_PIN_SIOC        5     // I2C Clock (SCL)

// --- Chân dữ liệu song song (8-bit) ---
#define CAM_PIN_D7         16     // D7 (MSB)
#define CAM_PIN_D6         17
#define CAM_PIN_D5         18
#define CAM_PIN_D4         12
#define CAM_PIN_D3         10
#define CAM_PIN_D2          8
#define CAM_PIN_D1          9
#define CAM_PIN_D0         11     // D0 (LSB)

// --- Chân đồng bộ ---
#define CAM_PIN_VSYNC        6     // Vertical Sync
#define CAM_PIN_HREF         7     // Horizontal Reference
#define CAM_PIN_PCLK         13     // Pixel Clock

// --- Tần số XCLK ---
#define CAM_XCLK_FREQ     20000000   // 20 MHz

#endif // CAMERA_CONFIG_H