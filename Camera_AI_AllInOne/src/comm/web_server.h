#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

// ===================================================================
//  MODULE WEB SERVER — MJPEG Stream + REST API
//
//  Cung cấp:
//    /          → Trang HTML dashboard (embed stream + hiển thị số liệu)
//    /stream    → MJPEG multipart stream (video từ camera)
//    /api/status → JSON trạng thái: số người, IR, AI, FPS, trạng thái phòng
//
//  Giao thức: HTTP (không dùng WebRTC — quá nặng cho ESP32-CAM)
//  MJPEG: đẩy liên tục các frame JPEG qua multipart boundary
// ===================================================================

/// Khởi tạo WiFi STA và HTTP Server
/// @return true nếu kết nối WiFi + khởi tạo server thành công
bool webServerInit();

/// Xử lý request HTTP — GỌI THƯỜNG XUYÊN trong task/loop
void webServerHandle();

/// Lấy địa chỉ IP đã kết nối (dạng String)
String getLocalIP();

#endif // WEB_SERVER_H
