#include "audio_alert.h"
#include "app_config.h"
#include <HardwareSerial.h>

// ===================================================================
//  Triển khai module âm thanh DFPlayer Mini — UART Serial (HardwareSerial)
//  Giao tiếp với DFPlayer Mini qua các gói tin chuẩn 10-byte (baudrate 9600)
//  Không cần bất kỳ thư viện ngoài nào (dùng sẵn HardwareSerial của ESP32).
// ===================================================================

// Sử dụng UART2 của ESP32
static HardwareSerial dfSerial(2);

// Trạng thái phát
static bool isPlaying = false;

// Hàm gửi gói lệnh 10-byte chuẩn cho DFPlayer Mini
static void sendDFCommand(uint8_t cmd, uint16_t param) {
    uint8_t buf[10];
    buf[0] = 0x7E;                     // Start bit
    buf[1] = 0xFF;                     // Version
    buf[2] = 0x06;                     // Length (số byte dữ liệu)
    buf[3] = cmd;                      // Command
    buf[4] = 0x00;                     // Feedback (0 = không cần phản hồi)
    buf[5] = (uint8_t)(param >> 8);    // Param High
    buf[6] = (uint8_t)(param & 0xFF);  // Param Low

    // Tính Checksum: 0 - sum(byte 1 -> 6)
    uint16_t sum = buf[1] + buf[2] + buf[3] + buf[4] + buf[5] + buf[6];
    uint16_t checksum = (uint16_t)(0 - sum);
    buf[7] = (uint8_t)(checksum >> 8);
    buf[8] = (uint8_t)(checksum & 0xFF);
    buf[9] = 0xEF;                     // End bit

    dfSerial.write(buf, 10);
}

void audioInit() {
    // Khởi tạo UART2 với baudrate 9600 cho DFPlayer Mini
    dfSerial.begin(9600, SERIAL_8N1, PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
    delay(200);   // Chờ module DFPlayer sẵn sàng

    // Đặt âm lượng khởi động
    sendDFCommand(0x06, DFPLAYER_VOLUME);
    delay(50);

    Serial.println("[AUDIO] Khởi tạo DFPlayer Mini (UART) hoàn tất.");
    Serial.printf("[AUDIO]   RX = GPIO %d | TX = GPIO %d | Volume = %d/30\n",
                  PIN_DFPLAYER_RX, PIN_DFPLAYER_TX, DFPLAYER_VOLUME);
}

void playAlertRoomFull() {
    Serial.println("[AUDIO] ▶ DFPlayer: Phát Track 0001 (Phòng đã đầy)");
    sendDFCommand(0x03, 1);   // Lệnh 0x03: Play track 1
    isPlaying = true;
}

void playAlertWelcome() {
    Serial.println("[AUDIO] ▶ DFPlayer: Phát Track 0002 (Chào mừng / Còn chỗ)");
    sendDFCommand(0x03, 2);   // Lệnh 0x03: Play track 2
    isPlaying = true;
}

void playAlertSafetyMode() {
    Serial.println("[AUDIO] ▶ DFPlayer: Phát Track 0003 (Cảnh báo Safety Mode)");
    sendDFCommand(0x03, 3);   // Lệnh 0x03: Play track 3
    isPlaying = true;
}

void audioStop() {
    sendDFCommand(0x16, 0);   // Lệnh 0x16: Stop
    isPlaying = false;
    Serial.println("[AUDIO] ■ DFPlayer: Dừng phát.");
}

void setAudioVolume(uint8_t volume) {
    if (volume > 30) volume = 30;
    sendDFCommand(0x06, volume);
    Serial.printf("[AUDIO] Đặt âm lượng: %d/30\n", volume);
}

void audioUpdate() {
    // DFPlayer Mini tự động phát độc lập trên phần cứng ngoài, không cần non-blocking loop
}

bool isAudioPlaying() {
    return isPlaying;
}
