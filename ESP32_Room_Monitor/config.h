#ifndef CONFIG_H
#define CONFIG_H

// 1. CẤU HÌNH WIFI
#define WIFI_SSID     "B1.12.10"
#define WIFI_PASS     "02072006"

// 2. CẤU HÌNH CẢNH BÁO PHÒNG
#define SUC_CHUA_TOI_DA      20    // Sức chứa tối đa của phòng
#define NGUONG_SAI_LECH      3     // Mức chênh lệch giữa IR và AI để báo lỗi

// 3. CẤU HÌNH TIMING (MILLISECONDS)
#define WEBSOCKET_INTERVAL   2000  // Gửi dữ liệu lên Web mỗi 2 giây
#define LOG_RECORD_INTERVAL  5000  // Lưu log vào Flash mỗi 5 giây (nếu có thay đổi)

#endif