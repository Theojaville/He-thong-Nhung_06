#ifndef LOGGER_H
#define LOGGER_H

#include <LittleFS.h>

// Khởi tạo bộ nhớ Flash LittleFS
void initLogger() {
  if (!LittleFS.begin(true)) {
    Serial.println("[LOG] Lỗi khởi tạo bộ nhớ LittleFS!");
    return;
  }
  Serial.println("[LOG] Bộ nhớ LittleFS đã sẵn sàng.");
}

// Hàm ghi dữ liệu lịch sử số người vào file log.txt
void writeLog(int soNguoi) {
  File file = LittleFS.open("/log.txt", FILE_APPEND);
  if (!file) {
    Serial.println("[LOG] Mở file log thất bại!");
    return;
  }
  
  // Ghi chuỗi: [Thời gian chạy (s)], [Số người]
  String logEntry = String(millis() / 1000) + "," + String(soNguoi) + "\n";
  file.print(logEntry);
  file.close();
  Serial.print("[LOG] Đã ghi log: " + logEntry);
}

// Hàm đọc dữ liệu log ra Serial (Dùng để debug)
void readLog() {
  File file = LittleFS.open("/log.txt", FILE_READ);
  if (!file) return;
  
  Serial.println("--- LỊCH SỬ LOG TRONG FLASH ---");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

#endif