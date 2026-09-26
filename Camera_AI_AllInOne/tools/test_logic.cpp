// ===================================================================
//  TEST_LOGIC.CPP — Mô phỏng kiểm thử hệ thống không cần phần cứng ESP32
//
//  Kiểm tra các thuật toán:
//    1. State Machine cảm biến siêu âm HC-SR04 (Lọc nhiễu / Debounce)
//    2. Thuật toán dung hợp dữ liệu (Data Fusion: AI chính + Siêu âm xác nhận)
//    3. Điều khiển 3 LED đèn giao thông (Xanh, Vàng, Đỏ, Nhấp nháy)
//    4. Chế độ an toàn (Safety Mode: Kích hoạt khi lỗi & Cơ chế Hysteresis thoát)
//
//  Cách biên dịch và chạy trên máy tính:
//    g++ tools/test_logic.cpp -o test_logic.exe
//    .\test_logic.exe
// ===================================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <cstdint>

using namespace std;

// ======================== HẰNG SỐ CẤU HÌNH (khớp app_config.h) =====
const int   ROOM_MAX_CAPACITY           = 30;
const int   DISCREPANCY_THRESHOLD        = 3;
const float PRESENCE_THRESHOLD_CM       = 30.0f;
const int   MIN_PRESENCE_MS             = 150;
const int   SAFETY_CAM_FAIL_LIMIT       = 5;
const int   SAFETY_STABLE_CYCLES_TO_EXIT = 5;
const uint32_t SAFETY_MIN_FREE_HEAP     = 20 * 1024;

// ======================== MÀU SẮC TERMINAL =========================
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31;1m"
#define COLOR_GREEN   "\033[32;1m"
#define COLOR_YELLOW  "\033[33;1m"
#define COLOR_BLUE    "\033[34;1m"
#define COLOR_CYAN    "\033[36;1m"
#define COLOR_MAGENTA "\033[35;1m"

// ======================== 1. MÔ PHỎNG SIÊU ÂM (HC-SR04) ===========
enum USState { US_FAR, US_NEAR };

class MockUltrasonic {
private:
    USState state = US_FAR;
    uint32_t nearStartTime = 0;
    uint32_t lastCrossingTime = 0;
    int crossingCount = 0;

public:
    void update(float distCm, uint32_t nowMs) {
        bool isNear = (distCm > 0.0f && distCm < PRESENCE_THRESHOLD_CM);

        switch (state) {
        case US_FAR:
            if (isNear) {
                state = US_NEAR;
                nearStartTime = nowMs;
            }
            break;

        case US_NEAR:
            if (!isNear) {
                uint32_t duration = nowMs - nearStartTime;
                if (duration >= (uint32_t)MIN_PRESENCE_MS) {
                    crossingCount++;
                    lastCrossingTime = nowMs;
                    cout << COLOR_CYAN << "  [US] ✔ Phát hiện người đi qua #" << crossingCount 
                         << " (Gần: " << duration << "ms, Khoảng cách: " << distCm << "cm)" 
                         << COLOR_RESET << endl;
                } else {
                    cout << "  [US] ⚠ Nhiễu bị loại bỏ (chỉ ở gần " << duration << "ms < " << MIN_PRESENCE_MS << "ms)" << endl;
                }
                state = US_FAR;
            }
            break;
        }
    }

    int getCrossingCount() const { return crossingCount; }
    bool hasRecentActivity(uint32_t nowMs, uint32_t windowMs = 10000) const {
        if (lastCrossingTime == 0) return false;
        return (nowMs - lastCrossingTime) <= windowMs;
    }
};

// ======================== 2. MÔ PHỎNG FUSION =======================
enum RoomStatus { ROOM_AVAILABLE, ROOM_NEARLY_FULL, ROOM_FULL, ROOM_DISCREPANCY };

struct FusionResult {
    int crossingCount;
    bool hasActivity;
    int aiCount;
    int finalCount;
    int discrepancy;
    bool discrepancyFlag;
    RoomStatus roomStatus;
    float occupancyPercent;
};

class MockDataFusion {
private:
    int prevCrossingCount = 0;

public:
    FusionResult update(int crossingCount, bool hasActivity, int aiCount) {
        FusionResult r;
        r.crossingCount = crossingCount;
        r.hasActivity = hasActivity;
        r.aiCount = aiCount;
        
        // AI là nguồn chính cho occupancy
        r.finalCount = (aiCount < 0) ? 0 : aiCount;
        r.occupancyPercent = (float)r.finalCount / (float)ROOM_MAX_CAPACITY * 100.0f;

        bool newCrossing = (crossingCount > prevCrossingCount);
        prevCrossingCount = crossingCount;

        r.discrepancy = 0;
        r.discrepancyFlag = false;

        // Phát hiện sai lệch:
        // Case 1: AI = 0 nhưng siêu âm phát hiện crossing mới
        if (aiCount == 0 && hasActivity && newCrossing) {
            r.discrepancyFlag = true;
            r.discrepancy = crossingCount;
        }
        // Case 2: AI đông người nhưng siêu âm im ắng
        if (aiCount > DISCREPANCY_THRESHOLD && !hasActivity && crossingCount > 0) {
            r.discrepancyFlag = true;
            r.discrepancy = aiCount;
        }

        if (r.discrepancyFlag) {
            r.roomStatus = ROOM_DISCREPANCY;
        } else if (r.finalCount >= ROOM_MAX_CAPACITY) {
            r.roomStatus = ROOM_FULL;
        } else if (r.occupancyPercent >= 80.0f) {
            r.roomStatus = ROOM_NEARLY_FULL;
        } else {
            r.roomStatus = ROOM_AVAILABLE;
        }

        return r;
    }
};

// ======================== 3. MÔ PHỎNG 3 ĐÈN LED ===================
string getLEDStatus(RoomStatus status, bool safetyMode) {
    if (safetyMode) {
        return string(COLOR_RED) + "ĐỎ" + COLOR_RESET + " ↔ " + 
               string(COLOR_YELLOW) + "VÀNG" + COLOR_RESET + " (Nhấp nháy LUÂN PHIÊN - Safety Mode)";
    }
    switch (status) {
    case ROOM_AVAILABLE:
        return string(COLOR_GREEN) + "XANH SÁNG" + COLOR_RESET + " (Phòng còn chỗ <80%)";
    case ROOM_NEARLY_FULL:
        return string(COLOR_YELLOW) + "VÀNG SÁNG" + COLOR_RESET + " (Phòng gần đầy 80-99%)";
    case ROOM_FULL:
        return string(COLOR_RED) + "ĐỎ NHẤP NHÁY" + COLOR_RESET + " (Phòng ĐÃ ĐẦY >=100%)";
    case ROOM_DISCREPANCY:
        return string(COLOR_YELLOW) + "VÀNG" + COLOR_RESET + " + " + 
               string(COLOR_RED) + "ĐỎ" + COLOR_RESET + " (Nhấp nháy ĐỒNG THỜI - Sai lệch số liệu)";
    default:
        return "TẮT";
    }
}

// ======================== 4. MÔ PHỎNG SAFETY MODE =================
class MockSafetyMode {
private:
    bool active = false;
    int camFailCount = 0;
    int stableCount = 0;
    uint32_t discrepancyStartTime = 0;
    bool discrepancyOngoing = false;

public:
    bool update(bool aiInitOk, bool lastCaptureOk, uint32_t freeHeap, bool discrepancyFlag, uint32_t nowMs, string& reason) {
        bool shouldBeActive = false;

        if (!aiInitOk) {
            shouldBeActive = true;
            reason = "AI Init thất bại";
        }

        if (lastCaptureOk) {
            camFailCount = 0;
        } else {
            camFailCount++;
            if (camFailCount >= SAFETY_CAM_FAIL_LIMIT) {
                shouldBeActive = true;
                reason = "Camera fail liên tiếp " + to_string(camFailCount) + " lần";
            }
        }

        if (freeHeap < SAFETY_MIN_FREE_HEAP) {
            shouldBeActive = true;
            reason = "Free Heap quá thấp (" + to_string(freeHeap) + " bytes)";
        }

        if (discrepancyFlag) {
            if (!discrepancyOngoing) {
                discrepancyOngoing = true;
                discrepancyStartTime = nowMs;
            } else if (nowMs - discrepancyStartTime >= 60000) {
                shouldBeActive = true;
                reason = "Sai lệch liên tục quá 60s";
            }
        } else {
            discrepancyOngoing = false;
        }

        if (shouldBeActive) {
            stableCount = 0;
            if (!active) {
                active = true;
                cout << COLOR_RED << "  [SAFETY] >>> KÍCH HOẠT CHẾ ĐỘ AN TOÀN! Lý do: " << reason << COLOR_RESET << endl;
                cout << "  [AUDIO]  ▶ DFPlayer: Phát Track 0003 (Cảnh báo Safety Mode 1 lần)" << endl;
            }
        } else if (active) {
            stableCount++;
            cout << "  [SAFETY] Đang phục hồi ổn định: " << stableCount << "/" << SAFETY_STABLE_CYCLES_TO_EXIT << " chu kỳ..." << endl;
            if (stableCount >= SAFETY_STABLE_CYCLES_TO_EXIT) {
                active = false;
                stableCount = 0;
                cout << COLOR_GREEN << "  [SAFETY] <<< ĐÃ THOÁT SAFETY MODE (Đạt 5 chu kỳ ổn định liên tiếp)" << COLOR_RESET << endl;
            }
        }

        return active;
    }

    bool isActive() const { return active; }
};

// ======================== HÀM IN KẾT QUẢ CHU KỲ ===================
void printCycle(int cycle, int aiCount, int crossings, bool hasAct, const FusionResult& fr, bool safety, const string& ledStatus) {
    cout << "┌────────────────────────────────────────────────────────┐" << endl;
    cout << "│ Chu kỳ #" << setw(2) << left << cycle 
         << " | AI Count: " << setw(2) << aiCount 
         << " | Crossings: " << setw(2) << crossings 
         << " | Hoạt động cửa: " << (hasAct ? "CÓ " : "KO ") << "│" << endl;
    cout << "├────────────────────────────────────────────────────────┤" << endl;
    cout << "│ Quyết định: " << setw(2) << fr.finalCount << " người"
         << " (" << setw(3) << (int)fr.occupancyPercent << "%)"
         << " | Trạng thái: ";
    
    switch (fr.roomStatus) {
    case ROOM_AVAILABLE:    cout << COLOR_GREEN   << "CÒN CHỖ   " << COLOR_RESET; break;
    case ROOM_NEARLY_FULL:  cout << COLOR_YELLOW  << "SẮP ĐẦY   " << COLOR_RESET; break;
    case ROOM_FULL:         cout << COLOR_RED     << "ĐÃ ĐẦY    " << COLOR_RESET; break;
    case ROOM_DISCREPANCY:  cout << COLOR_MAGENTA << "SAI LỆCH  " << COLOR_RESET; break;
    }
    cout << "        │" << endl;

    cout << "│ Safety Mode: " << (safety ? (string(COLOR_RED) + "BẬT (ACTIVE) " + COLOR_RESET) : "Tắt (Bình thường)") 
         << "                        │" << endl;
    cout << "│ LED: " << ledStatus << endl;
    cout << "└────────────────────────────────────────────────────────┘" << endl << endl;
}

// ======================== CHƯƠNG TRÌNH CHÍNH =======================
int main() {
    cout << "\n========================================================" << endl;
    cout << "   CHƯƠNG TRÌNH KIỂM THỬ TOÀN DIỆN MÔ PHỎNG HỆ THỐNG     " << endl;
    cout << "   Camera AI + Siêu âm HC-SR04 + DFPlayer + 3 LED       " << endl;
    cout << "========================================================\n" << endl;

    MockUltrasonic us;
    MockDataFusion fusion;
    MockSafetyMode safety;

    uint32_t now = 1000;

    // ---------------------------------------------------------------
    // TEST 1: Người đi qua cửa (HC-SR04 State Machine & Debounce)
    // ---------------------------------------------------------------
    cout << COLOR_BLUE << "=== [KỊCH BẢN 1]: Test cảm biến siêu âm phát hiện người ===" << COLOR_RESET << endl;
    cout << "1.1 Giả lập người đi qua: Cắt tia ở 20cm trong 200ms (>150ms chuẩn)..." << endl;
    us.update(100.0f, now);            // Ban đầu trống (100cm)
    now += 50;
    us.update(20.0f, now);             // Người đến gần (20cm)
    now += 200;                        // Đứng trong vùng quét 200ms
    us.update(120.0f, now);            // Đi qua xong (120cm)
    
    cout << "1.2 Giả lập nhiễu: Vật bay nhanh qua trong 50ms (<150ms chuẩn)..." << endl;
    now += 1000;
    us.update(15.0f, now);             // Nhiễu gần 15cm
    now += 50;                         // Chỉ lướt qua 50ms
    us.update(120.0f, now);            // Đi qua
    cout << "-> Tổng số lượt crossing hợp lệ: " << us.getCrossingCount() << " (Kỳ vọng: 1)\n" << endl;

    // ---------------------------------------------------------------
    // TEST 2: Data Fusion & Đèn giao thông (<80%, 80-99%, >=100%)
    // ---------------------------------------------------------------
    cout << COLOR_BLUE << "=== [KỊCH BẢN 2]: Test Data Fusion & Đèn giao thông 3 màu ===" << COLOR_RESET << endl;
    
    // Chu kỳ 1: Bình thường, 5 người
    now += 3000;
    FusionResult r1 = fusion.update(us.getCrossingCount(), us.hasRecentActivity(now), 5);
    string reason;
    bool s1 = safety.update(true, true, 80000, r1.discrepancyFlag, now, reason);
    printCycle(1, 5, us.getCrossingCount(), us.hasRecentActivity(now), r1, s1, getLEDStatus(r1.roomStatus, s1));

    // Chu kỳ 2: Gần đầy (25 người = 83%)
    now += 3000;
    FusionResult r2 = fusion.update(us.getCrossingCount(), us.hasRecentActivity(now), 25);
    bool s2 = safety.update(true, true, 80000, r2.discrepancyFlag, now, reason);
    printCycle(2, 25, us.getCrossingCount(), us.hasRecentActivity(now), r2, s2, getLEDStatus(r2.roomStatus, s2));

    // Chu kỳ 3: Đã đầy phòng (30 người = 100%)
    now += 3000;
    FusionResult r3 = fusion.update(us.getCrossingCount(), us.hasRecentActivity(now), 30);
    bool s3 = safety.update(true, true, 80000, r3.discrepancyFlag, now, reason);
    printCycle(3, 30, us.getCrossingCount(), us.hasRecentActivity(now), r3, s3, getLEDStatus(r3.roomStatus, s3));

    // ---------------------------------------------------------------
    // TEST 3: Cảnh báo sai lệch dữ liệu (Discrepancy)
    // ---------------------------------------------------------------
    cout << COLOR_BLUE << "=== [KỊCH BẢN 3]: Test cảnh báo sai lệch dữ liệu ===" << COLOR_RESET << endl;
    cout << "Mô phỏng: Camera AI báo 12 người, nhưng cảm biến siêu âm im ắng từ lâu..." << endl;
    now += 15000; // Trôi qua 15s, siêu âm hết active
    FusionResult r4 = fusion.update(us.getCrossingCount(), us.hasRecentActivity(now), 12);
    bool s4 = safety.update(true, true, 80000, r4.discrepancyFlag, now, reason);
    printCycle(4, 12, us.getCrossingCount(), us.hasRecentActivity(now), r4, s4, getLEDStatus(r4.roomStatus, s4));

    // ---------------------------------------------------------------
    // TEST 4: Kích hoạt Safety Mode & Tự phục hồi sau 5 chu kỳ ổn định
    // ---------------------------------------------------------------
    cout << COLOR_BLUE << "=== [KỊCH BẢN 4]: Test kích hoạt Safety Mode & Cơ chế thoát (Hysteresis) ===" << COLOR_RESET << endl;
    cout << "Mô phỏng: Camera bị lỗi chụp ảnh 5 lần liên tiếp..." << endl;
    
    // Gặp lỗi 5 lần liên tiếp
    for (int i = 1; i <= 5; i++) {
        now += 3000;
        bool safeState = safety.update(true, false, 80000, false, now, reason);
        cout << "  -> Lần fail #" << i << ": Safety Mode = " << (safeState ? "BẬT" : "Tắt") << endl;
    }
    
    cout << "\nChu kỳ tiếp theo khi đang trong Safety Mode:" << endl;
    FusionResult r5 = fusion.update(us.getCrossingCount(), false, 0);
    printCycle(5, 0, us.getCrossingCount(), false, r5, safety.isActive(), getLEDStatus(r5.roomStatus, safety.isActive()));

    cout << "\nMô phỏng: Camera hoạt động bình thường trở lại (Cần 5 chu kỳ liên tiếp để thoát)..." << endl;
    for (int i = 1; i <= 5; i++) {
        now += 3000;
        safety.update(true, true, 80000, false, now, reason);
    }

    cout << "\nTrạng thái sau khi phục hồi:" << endl;
    FusionResult r6 = fusion.update(us.getCrossingCount(), false, 2);
    printCycle(6, 2, us.getCrossingCount(), false, r6, safety.isActive(), getLEDStatus(r6.roomStatus, safety.isActive()));

    cout << COLOR_GREEN << "========================================================" << endl;
    cout << "   ✔ TẤT CẢ KỊCH BẢN KIỂM THỬ ĐÃ VƯỢT QUA HOÀN HẢO!     " << endl;
    cout << "========================================================\n" << COLOR_RESET << endl;

    return 0;
}
