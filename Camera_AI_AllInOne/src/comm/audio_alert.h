#ifndef AUDIO_ALERT_H
#define AUDIO_ALERT_H

#include <Arduino.h>

// ===================================================================
//  MODULE ÂM THANH CẢNH BÁO — DFPlayer Mini qua UART Serial
//
//  Sử dụng HardwareSerial (Serial2) giao tiếp với module DFPlayer Mini:
//    - Chân RX ESP32: GPIO 41 (nối vào TX DFPlayer)
//    - Chân TX ESP32: GPIO 42 (nối vào RX DFPlayer)
//
//  Các file âm thanh trên thẻ nhớ MicroSD:
//    - Track 0001.mp3: Cảnh báo phòng đầy (ROOM_FULL)
//    - Track 0002.mp3: Âm chào mừng / phòng còn chỗ (WELCOME)
//    - Track 0003.mp3: Cảnh báo Safety Mode (SAFETY_MODE)
//
//  Phát âm NON-BLOCKING qua lệnh UART chuẩn của DFPlayer.
// ===================================================================

/// Khởi tạo module DFPlayer Mini (cấu hình UART, đặt âm lượng)
void audioInit();

/// Phát cảnh báo "Phòng đã đầy!" (Track 1)
void playAlertRoomFull();

/// Phát âm "Chào mừng / Còn chỗ" (Track 2)
void playAlertWelcome();

/// Phát cảnh báo Safety Mode (Track 3)
/// CHỈ gọi 1 lần khi vừa vào Safety Mode, không lặp mỗi chu kỳ.
void playAlertSafetyMode();

/// Cập nhật trạng thái phát — duy trì tương thích vòng lặp loop()
void audioUpdate();

/// Dừng phát ngay lập tức
void audioStop();

/// Đặt âm lượng (0 - 30)
void setAudioVolume(uint8_t volume);

/// Kiểm tra có đang phát âm thanh không
bool isAudioPlaying();

#endif // AUDIO_ALERT_H
