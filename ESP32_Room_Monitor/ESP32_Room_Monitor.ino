#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "logger.h"
#include "web_server.h"

// Đăng ký các đối tượng Web Server
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Biến toàn cục chính của hệ thống
int soNguoiIR = 0;
int soNguoiAI = 0;
int soNguoiHienTai = 0;
bool coSaiLech = false;
String timestampAI = "--:--:--";

// Timers
unsigned long lastWebUpdate = 0;
unsigned long lastLogUpdate = 0;

// =========================================================================
// HÀM TÍCH HỢP DỮ LIỆU (SAU NÀY BẠN NỐI PHẦN CỦA A VÀ B VÀO ĐÂY)
// =========================================================================
void updateDataFromHardwareAndAI() {
  // -----------------------------------------------------------------------
  // DÙNG GIẢ LẬP ĐỂ TEST ĐỘC LẬP (Xóa phần này khi ghép đồ thật)
  soNguoiIR += random(-1, 2);
  if (soNguoiIR < 0) soNguoiIR = 0;

  soNguoiAI = soNguoiIR + random(-2, 2);
  if (soNguoiAI < 0) soNguoiAI = 0;

  soNguoiHienTai = soNguoiIR; // Logic ưu tiên nguồn IR
  coSaiLech = (abs(soNguoiIR - soNguoiAI) >= NGUONG_SAI_LECH);

  timestampAI = String(random(10,23)) + ":" + String(random(10,59)) + ":" + String(random(10,59));
  // -----------------------------------------------------------------------

  /* KHI CÓ ĐỒ THẬT TỪ A VÀ B -> BẠN CHỈ CẦN DÁN CODE NÀY VÀO:
     soNguoiIR = bienDemTuBanA;
     soNguoiAI = bienDemTuBanB;
     soNguoiHienTai = soNguoiIR;
     coSaiLech = (abs(soNguoiIR - soNguoiAI) >= NGUONG_SAI_LECH);
  */
}

void setup() {
  Serial.begin(115200);

  // 1. Kết nối WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Đang kết nối WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK - IP: " + WiFi.localIP().toString());

  // 2. Khởi tạo Module
  initLogger();
  initWebServer();
}

void loop() {
  ws.cleanupClients();

  // Task 1: Cập nhật dữ liệu & đẩy lên Web Dashboard mỗi 2s
  if (millis() - lastWebUpdate > WEBSOCKET_INTERVAL) {
    updateDataFromHardwareAndAI(); // Cập nhật số liệu
    notifyClients();               // Đẩy lên Web
    checkWiFiConnection();         // Kiêm tra kết nối WiFi
    lastWebUpdate = millis();
  }

  // Task 2: Ghi Log lịch sử vào bộ nhớ Flash LittleFS mỗi 5s (Tuần 4)
  if (millis() - lastLogUpdate > LOG_RECORD_INTERVAL) {
    writeLog(soNguoiHienTai);
    lastLogUpdate = millis();
  }
}