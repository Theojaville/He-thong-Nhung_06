#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"

extern AsyncWebServer server;
extern AsyncWebSocket ws;

// Khai báo lại các biến dữ liệu toàn cục
extern int soNguoiIR;
extern int soNguoiAI;
extern int soNguoiHienTai;
extern bool coSaiLech;
extern String timestampAI;

// Mã Giao Diện HTML/CSS/JS nhúng PROGMEM
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Hệ Thống Đếm Người Ra Vào - IoT Dashboard</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700&display=swap" rel="stylesheet">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root { --bg-color: #0f172a; --card-bg: #1e293b; --text-main: #f8fafc; --text-sub: #94a3b8; --accent-green: #10b981; --accent-yellow: #f59e0b; --accent-red: #ef4444; --border-color: #334155; }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Inter', sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-main); padding: 24px; min-height: 100vh; }
        .container { max-width: 1000px; margin: 0 auto; }
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; border-bottom: 1px solid var(--border-color); padding-bottom: 16px; }
        h1 { font-size: 1.5rem; font-weight: 700; color: #60a5fa; }
        .status-badge { background: #064e3b; color: #34d399; padding: 6px 12px; border-radius: 20px; font-size: 0.85rem; font-weight: 600; }
        .hero-card { background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 16px; padding: 24px; text-align: center; margin-bottom: 24px; }
        .hero-title { font-size: 0.9rem; text-transform: uppercase; letter-spacing: 1px; color: var(--text-sub); }
        .hero-number { font-size: 4.5rem; font-weight: 700; margin: 8px 0; color: var(--accent-green); }
        .progress-container { background: #334155; border-radius: 10px; height: 12px; width: 100%; overflow: hidden; margin-top: 12px; }
        .progress-bar { background: var(--accent-green); height: 100%; width: 0%; transition: width 0.5s ease; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; margin-bottom: 24px; }
        .card { background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 12px; padding: 18px; }
        .card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
        .card-title { font-size: 0.85rem; color: var(--text-sub); font-weight: 600; }
        .card-value { font-size: 1.8rem; font-weight: 700; }
        .card-sub { font-size: 0.75rem; color: var(--text-sub); margin-top: 4px; }
        .alert-banner { background: rgba(239, 68, 68, 0.15); border: 1px solid var(--accent-red); color: #fca5a5; padding: 14px 20px; border-radius: 12px; margin-bottom: 24px; display: none; }
        .chart-section { background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 16px; padding: 20px; }
        .status-green { color: var(--accent-green) !important; }
        .status-yellow { color: var(--accent-yellow) !important; }
        .status-red { color: var(--accent-red) !important; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>ROOM MONITORING SYSTEM</h1>
            <div class="status-badge" id="netStatus">Connecting...</div>
        </header>

        <div id="alertBox" class="alert-banner">
            ⚠️ <span><strong>Cảnh báo sai lệch:</strong> Số liệu giữa Cảm biến IR và Camera AI không đồng bộ!</span>
        </div>

        <div class="hero-card">
            <div class="hero-title">Số Người Hiện Tại Trong Phòng</div>
            <div class="hero-number status-green" id="soNguoiHienTai">0</div>
            <div id="trangThaiText" style="color: var(--text-sub); font-weight: 600;">Mật độ: 0% (An toàn)</div>
            <div class="progress-container"><div class="progress-bar" id="progressBar"></div></div>
        </div>

        <div class="grid">
            <div class="card">
                <div class="card-header"><span class="card-title">MẬT ĐỘ PHÒNG</span></div>
                <div class="card-value" id="matDo">0%</div>
                <div class="card-sub">Sức chứa tối đa: <span id="sucChua">20</span> người</div>
            </div>
            <div class="card">
                <div class="card-header"><span class="card-title">CẢM BIẾN HỒNG NGOẠI</span></div>
                <div class="card-value" id="soNguoiIR">0</div>
                <div class="card-sub">Nguồn: Bạn A (Hardware)</div>
            </div>
            <div class="card">
                <div class="card-header"><span class="card-title">CAMERA AI</span></div>
                <div class="card-value" id="soNguoiAI">0</div>
                <div class="card-sub">Cập nhật: <span id="timestampAI">--:--:--</span></div>
            </div>
        </div>

        <div class="chart-section">
            <h3 style="font-size: 1rem; margin-bottom: 16px; color: var(--text-sub);">Lịch Sử Số Lượng Người Tự Động (Real-time)</h3>
            <canvas id="historyChart" height="100"></canvas>
        </div>
    </div>

    <script>
        const ctx = document.getElementById('historyChart').getContext('2d');
        const historyChart = new Chart(ctx, {
            type: 'line',
            data: { labels: [], datasets: [{ label: 'Số người trong phòng', data: [], borderColor: '#60a5fa', backgroundColor: 'rgba(96, 165, 250, 0.1)', fill: true, tension: 0.4 }] },
            options: { responsive: true, scales: { x: { grid: { color: '#334155' }, ticks: { color: '#94a3b8' } }, y: { grid: { color: '#334155' }, ticks: { color: '#94a3b8' }, beginAtZero: true } } }
        });

        let gateway = `ws://${window.location.hostname}/ws`;
        let websocket;

        function initWebSocket() {
            websocket = new WebSocket(gateway);
            websocket.onopen = () => { document.getElementById('netStatus').innerText = '● WebSocket Connected'; document.getElementById('netStatus').style.background = '#064e3b'; };
            websocket.onclose = () => { document.getElementById('netStatus').innerText = '○ Disconnected'; document.getElementById('netStatus').style.background = '#7f1d1d'; setTimeout(initWebSocket, 2000); };
            websocket.onmessage = (event) => { updateDashboard(JSON.parse(event.data)); };
        }

        function updateDashboard(data) {
            document.getElementById('soNguoiHienTai').innerText = data.soNguoiHienTai;
            document.getElementById('matDo').innerText = data.matDo + '%';
            document.getElementById('soNguoiIR').innerText = data.soNguoiIR;
            document.getElementById('soNguoiAI').innerText = data.soNguoiAI;
            document.getElementById('timestampAI').innerText = data.timestampAI;

            const progressBar = document.getElementById('progressBar');
            const numEl = document.getElementById('soNguoiHienTai');
            
            progressBar.style.width = Math.min(data.matDo, 100) + '%';
            numEl.classList.remove('status-green', 'status-yellow', 'status-red');

            if (data.trangThaiDen === 'xanh') { numEl.classList.add('status-green'); progressBar.style.backgroundColor = 'var(--accent-green)'; }
            else if (data.trangThaiDen === 'vang') { numEl.classList.add('status-yellow'); progressBar.style.backgroundColor = 'var(--accent-yellow)'; }
            else { numEl.classList.add('status-red'); progressBar.style.backgroundColor = 'var(--accent-red)'; }

            document.getElementById('alertBox').style.display = data.coSaiLech ? 'flex' : 'none';

            const now = new Date().toLocaleTimeString();
            if (historyChart.data.labels.length > 10) { historyChart.data.labels.shift(); historyChart.data.datasets[0].data.shift(); }
            historyChart.data.labels.push(now);
            historyChart.data.datasets[0].data.push(data.soNguoiHienTai);
            historyChart.update();
        }

        window.addEventListener('load', initWebSocket);
    </script>
</body>
</html>
)rawliteral";

// Đóng gói JSON & Phát Broadcast qua WebSocket
void notifyClients() {
  StaticJsonDocument<256> doc;

  int matDo = (int)(((float)soNguoiHienTai / SUC_CHUA_TOI_DA) * 100);
  String trangThaiDen = "xanh";
  if (matDo >= 80 && matDo < 100) trangThaiDen = "vang";
  else if (matDo >= 100) trangThaiDen = "do";

  doc["soNguoiIR"] = soNguoiIR;
  doc["soNguoiAI"] = soNguoiAI;
  doc["soNguoiHienTai"] = soNguoiHienTai;
  doc["matDo"] = matDo;
  doc["trangThaiDen"] = trangThaiDen;
  doc["coSaiLech"] = coSaiLech;
  doc["timestampAI"] = timestampAI;

  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

// Xử lý sự kiện kết nối WebSocket
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    notifyClients();
  }
}

// Hàm khởi tạo Web Server & WebSocket
void initWebServer() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.begin();
  Serial.println("[WEB] Server & WebSocket đã khởi động.");
}

// Hàm kiểm tra và tự động kết nối lại WiFi nếu rớt mạng (Tuần 4 - Task 4)
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Mất kết nối WiFi, đang tự kết nối lại...");
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

#endif