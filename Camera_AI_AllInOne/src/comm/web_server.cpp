#include "web_server.h"
#include "app_config.h"
#include "ai/camera_capture.h"
#include "ai/yolox_inference.h"
#include "fusion/data_fusion.h"
#include "sensors/ultrasonic_sensor.h"
#include "system/safety_mode.h"

#include <WiFi.h>
#include <WebServer.h>
#include "img_converters.h"

// ===================================================================
//  Triển khai Web Server — MJPEG + REST API + HTML Dashboard
// ===================================================================

static WebServer server(WEB_SERVER_PORT);

// ===================================================================
//  TRANG HTML DASHBOARD (lưu trong Flash)
//
//  Giao diện hiển thị:
//    - Video stream (MJPEG) từ camera
//    - Số người: Siêu âm crossing / AI / Quyết định cuối cùng
//    - Trạng thái phòng (màu sắc thay đổi)
//    - FPS, phần trăm lấp đầy
//    - Banner Safety Mode (hiển thị khi safetyMode = true)
//  JavaScript tự động fetch /api/status mỗi 2 giây
// ===================================================================

static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Đếm Người - Nhóm 6</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            background: #1a1a2e;
            color: #eee;
            min-height: 100vh;
        }
        .header {
            background: #16213e;
            padding: 15px 20px;
            text-align: center;
            border-bottom: 2px solid #0f3460;
        }
        .header h1 { font-size: 1.4em; color: #e94560; }
        .header p { font-size: 0.85em; color: #888; margin-top: 4px; }
        .container {
            max-width: 900px;
            margin: 20px auto;
            padding: 0 15px;
        }
        .stream-box {
            text-align: center;
            margin-bottom: 20px;
        }
        .stream-box img {
            width: 100%;
            max-width: 640px;
            border: 2px solid #333;
            border-radius: 8px;
        }
        .cards {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
            gap: 12px;
            margin-bottom: 20px;
        }
        .card {
            background: #16213e;
            border-radius: 10px;
            padding: 18px;
            text-align: center;
            border: 1px solid #0f3460;
        }
        .card .label { font-size: 0.8em; color: #888; margin-bottom: 6px; }
        .card .value { font-size: 2em; font-weight: bold; }
        .status-box {
            text-align: center;
            padding: 20px;
            border-radius: 10px;
            font-size: 1.3em;
            font-weight: bold;
        }
        .status-ok    { background: #1b5e20; color: #a5d6a7; }
        .status-near  { background: #e65100; color: #ffcc80; }
        .status-full  { background: #b71c1c; color: #ef9a9a; animation: blink 1s infinite; }
        .status-warn  { background: #4a148c; color: #ce93d8; }
        @keyframes blink { 50% { opacity: 0.5; } }
        .safety-banner {
            background: #ff1744;
            color: #fff;
            text-align: center;
            padding: 12px;
            font-size: 1.1em;
            font-weight: bold;
            border-radius: 8px;
            margin-bottom: 15px;
            animation: blink 0.8s infinite;
            display: none;
        }
        .footer { text-align: center; padding: 15px; color: #555; font-size: 0.75em; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🏠 HỆ THỐNG ĐẾM NGƯỜI RA VÀO PHÒNG</h1>
        <p>Nhóm 6 — ESP32-CAM + AI YoloX-nano + Cảm biến siêu âm</p>
    </div>

    <div class="container">
        <div class="safety-banner" id="safetyBanner">
            ⚠️ SAFETY MODE — Hệ thống đang chạy chế độ bảo vệ!
        </div>

        <div class="stream-box">
            <img id="stream" src="/stream" alt="Camera Stream">
        </div>

        <div class="cards">
            <div class="card">
                <div class="label">📡 Siêu âm (crossing)</div>
                <div class="value" id="crossingCount">--</div>
            </div>
            <div class="card">
                <div class="label">🤖 Camera AI</div>
                <div class="value" id="aiCount">--</div>
            </div>
            <div class="card">
                <div class="label">✅ Quyết định</div>
                <div class="value" id="finalCount" style="color:#4fc3f7;">--</div>
            </div>
            <div class="card">
                <div class="label">⚡ AI FPS</div>
                <div class="value" id="fps">--</div>
            </div>
            <div class="card">
                <div class="label">📊 Lấp đầy</div>
                <div class="value" id="occupancy">--</div>
            </div>
            <div class="card">
                <div class="label">🚶 Hoạt động cửa</div>
                <div class="value" id="activity">--</div>
            </div>
        </div>

        <div class="status-box status-ok" id="statusBox">
            Đang tải...
        </div>
    </div>

    <div class="footer">ESP32-CAM All-in-One | Cập nhật mỗi 2 giây</div>

    <script>
        const statusMessages = {
            'CON_CHO':   ['🟢 PHÒNG CÒN CHỖ TRỐNG', 'status-ok'],
            'GAN_DAY':   ['🟡 PHÒNG SẮP ĐẦY!', 'status-near'],
            'DA_DAY':    ['🔴 PHÒNG ĐÃ ĐẦY — KHÔNG NHẬN THÊM!', 'status-full'],
            'SAI_LECH':  ['⚠️ CẢNH BÁO: DỮ LIỆU SAI LỆCH', 'status-warn']
        };

        async function fetchStatus() {
            try {
                const res = await fetch('/api/status');
                const d = await res.json();
                document.getElementById('crossingCount').textContent = d.crossingCount;
                document.getElementById('aiCount').textContent = d.aiCount;
                document.getElementById('finalCount').textContent = d.finalCount;
                document.getElementById('fps').textContent = d.fps.toFixed(1);
                document.getElementById('occupancy').textContent = d.occupancy.toFixed(0) + '%';
                document.getElementById('activity').textContent = d.hasActivity ? 'Có' : 'Không';

                const box = document.getElementById('statusBox');
                const [msg, cls] = statusMessages[d.roomStatus] || ['UNKNOWN', 'status-ok'];
                box.textContent = msg;
                box.className = 'status-box ' + cls;

                // Safety Mode banner
                const banner = document.getElementById('safetyBanner');
                banner.style.display = d.safetyMode ? 'block' : 'none';
            } catch(e) {
                console.error('Fetch error:', e);
            }
        }

        setInterval(fetchStatus, 2000);
        fetchStatus();
    </script>
</body>
</html>
)rawliteral";

// ===================================================================
//  HANDLER: Trang chủ /
// ===================================================================
static void handleRoot() {
    server.send_P(200, "text/html", HTML_PAGE);
}

// ===================================================================
//  HANDLER: MJPEG Stream /stream
//
//  Giao thức: HTTP multipart/x-mixed-replace
//  Liên tục gửi các frame JPEG cách nhau bằng boundary
// ===================================================================
static void handleStream() {
    WiFiClient client = server.client();

    // Header multipart
    String header = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace;boundary=" STREAM_PART_BOUNDARY "\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "\r\n";
    client.print(header);

    while (client.connected()) {
        camera_fb_t* fb = captureJPEG();
        if (!fb) {
            delay(100);
            continue;
        }

        uint8_t* jpg_buf = nullptr;
        size_t jpg_len = 0;
        bool converted = false;

        if (fb->format != PIXFORMAT_JPEG) {
            converted = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
        } else {
            jpg_buf = fb->buf;
            jpg_len = fb->len;
        }

        if (jpg_buf && jpg_len > 0) {
            // Gửi 1 frame JPEG
            String partHeader = "--" STREAM_PART_BOUNDARY "\r\n"
                                "Content-Type: image/jpeg\r\n"
                                "Content-Length: " + String(jpg_len) + "\r\n"
                                "\r\n";
            client.print(partHeader);
            client.write(jpg_buf, jpg_len);
            client.print("\r\n");
        }

        if (converted && jpg_buf) {
            free(jpg_buf);
        }

        releaseFrame(fb);

        // Delay giữa các frame (kiểm soát FPS stream)
        delay(STREAM_FRAME_DELAY_MS);

        // Cho phép watchdog timer reset
        yield();
    }
}

// ===================================================================
//  HANDLER: REST API /api/status
//
//  Trả về JSON chứa toàn bộ thông tin hệ thống
// ===================================================================
static void handleApiStatus() {
    FusionResult fr = getLastFusionResult();

    String json = "{";
    json += "\"crossingCount\":" + String(fr.crossingCount) + ",";
    json += "\"hasActivity\":"   + String(fr.hasActivity ? "true" : "false") + ",";
    json += "\"aiCount\":"       + String(fr.aiCount)      + ",";
    json += "\"finalCount\":"    + String(fr.finalCount)    + ",";
    json += "\"discrepancy\":"   + String(fr.discrepancy)   + ",";
    json += "\"discrepancyFlag\":" + String(fr.discrepancyFlag ? "true" : "false") + ",";
    json += "\"occupancy\":"     + String(fr.occupancyPercent, 1) + ",";
    json += "\"roomStatus\":\"" + String(roomStatusToString(fr.roomStatus)) + "\",";
    json += "\"roomCapacity\":" + String(ROOM_MAX_CAPACITY) + ",";
    json += "\"safetyMode\":"   + String(isSafetyModeActive() ? "true" : "false") + ",";
    json += "\"freeHeap\":"     + String(ESP.getFreeHeap()) + ",";
    json += "\"fps\":"          + String(getAverageFPS(), 2);
    json += "}";

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

// ===================================================================
bool webServerInit() {
    Serial.println("\n[WEB] --- Khởi tạo WiFi & Web Server ---");

    // Dọn dẹp cấu hình WiFi cũ
    WiFi.disconnect(true);
    delay(100);

    // Tự phát sóng WiFi (AP Mode) trực tiếp từ ESP32
    // Người dùng chỉ cần kết nối thẳng vào WiFi này để xem Dashboard & Camera
    const char* apSSID = "ESP32-CAM-AI";
    const char* apPass = "12345678";

    WiFi.mode(WIFI_AP);
    bool apStarted = WiFi.softAP(apSSID, apPass);

    if (apStarted) {
        Serial.printf("[WEB] ✔ ĐÃ PHÁT SÓNG WIFI: %s\n", apSSID);
        Serial.printf("[WEB]   Mật khẩu: %s\n", apPass);
        Serial.printf("[WEB]   Địa chỉ IP: http://%s/\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[WEB] ✖ Không thể bật chế độ phát WiFi!");
        return false;
    }

    // --- Đăng ký các route web ---
    server.on("/",           HTTP_GET, handleRoot);
    server.on("/stream",     HTTP_GET, handleStream);
    server.on("/api/status", HTTP_GET, handleApiStatus);

    // --- Khởi động HTTP Server ---
    server.begin();
    Serial.printf("[WEB] ✔ Web Server đã sẵn sàng trên cổng %d\n", WEB_SERVER_PORT);
    Serial.printf("[WEB] 👉 HƯỚNG DẪN TRUY CẬP:\n");
    Serial.printf("[WEB]    1. Kết nối WiFi: '%s' (Mật khẩu: %s)\n", apSSID, apPass);
    Serial.printf("[WEB]    2. Mở trình duyệt web gõ: http://%s/\n\n", WiFi.softAPIP().toString().c_str());

    return true;
}

void webServerHandle() {
    server.handleClient();
}

String getLocalIP() {
    return WiFi.softAPIP().toString();
}
