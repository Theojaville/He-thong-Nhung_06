#include "yolox_inference.h"
#include "app_config.h"
#include "person_detect_model.h"
#include "image_utils.h"

// --- TensorFlow Lite Micro headers ---
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

// ===================================================================
//  Triển khai module suy luận AI — YoloX-nano + TFLite Micro
// ===================================================================

// ---------- Biến toàn cục TFLite ----------
static const tflite::Model*           model       = nullptr;
static tflite::MicroInterpreter*      interpreter = nullptr;
static TfLiteTensor*                  inputTensor = nullptr;
static uint8_t*                       tensorArena = nullptr;

// ---------- Theo dõi FPS ----------
#define FPS_HISTORY_SIZE  10
static uint32_t inferenceHistory[FPS_HISTORY_SIZE] = {0};
static int      fpsIdx = 0;
static int      fpsCount = 0;

// ===================================================================
//  Hàm nội bộ: Non-Max Suppression (NMS)
//
//  Thuật toán:
//    1. Sắp xếp tất cả box theo confidence giảm dần
//    2. Lấy box confidence cao nhất → thêm vào kết quả
//    3. Loại bỏ tất cả box có IoU > ngưỡng với box vừa chọn
//    4. Lặp lại cho đến khi hết box
// ===================================================================

static void applyNMS(BBox* boxes, int count, float iouThresh,
                     BBox* outBoxes, int& outCount, int maxOut) {
    if (count <= 0) { outCount = 0; return; }

    // Tạo mảng index và cờ đã loại
    bool* suppressed = (bool*)calloc(count, sizeof(bool));
    if (!suppressed) { outCount = 0; return; }

    outCount = 0;

    // Sắp xếp theo confidence giảm dần (selection sort — OK cho số lượng nhỏ)
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (boxes[j].confidence > boxes[i].confidence) {
                BBox tmp = boxes[i];
                boxes[i] = boxes[j];
                boxes[j] = tmp;
            }
        }
    }

    // Greedy NMS
    for (int i = 0; i < count; i++) {
        if (suppressed[i]) continue;
        if (outCount >= maxOut) break;

        outBoxes[outCount++] = boxes[i];

        // Loại các box trùng lặp (IoU cao)
        for (int j = i + 1; j < count; j++) {
            if (suppressed[j]) continue;
            float iou = computeIoU(boxes[i], boxes[j]);
            if (iou > iouThresh) {
                suppressed[j] = true;
            }
        }
    }

    free(suppressed);
}

// ===================================================================
//  Hàm nội bộ: Parse output tensor của YoloX
//
//  YoloX output format (anchor-free):
//    Với input 96×96, 3 stride {8, 16, 32}:
//      Stride 8:  12×12 = 144 cells
//      Stride 16:  6×6  =  36 cells
//      Stride 32:  3×3  =   9 cells
//      Tổng:               189 proposals
//
//    Mỗi proposal: [x, y, w, h, objectness, c0, c1, ..., c79]
//                   4 bbox + 1 obj + 80 class = 85 values
//
//    Điểm cuối cùng cho class k: score_k = obj × c_k
//    Chỉ giữ person (k=0) có score > AI_CONFIDENCE_THRESH
// ===================================================================

static void parseYoloxOutput(const TfLiteTensor* output,
                             BBox* rawBoxes, int& rawCount) {
    rawCount = 0;

    // Lấy kích thước output tensor
    int numProposals = output->dims->data[1];   // Dự kiến: 189
    int numValues    = output->dims->data[2];    // Dự kiến: 85

    // Xác định kiểu dữ liệu output (float32 hoặc int8 quantized)
    bool isQuantized = (output->type == kTfLiteInt8 || output->type == kTfLiteUInt8);

    // Thông số dequantize (nếu model quantized)
    float outScale  = 1.0f;
    int   outZero   = 0;
    if (isQuantized) {
        outScale = output->params.scale;
        outZero  = output->params.zero_point;
    }

    for (int i = 0; i < numProposals; i++) {
        // Đọc giá trị objectness
        float obj;
        if (isQuantized) {
            int8_t raw = output->data.int8[i * numValues + 4];
            obj = ((float)raw - outZero) * outScale;
        } else {
            obj = output->data.f[i * numValues + 4];
        }

        // Áp dụng sigmoid nếu cần (YoloX output thường đã qua sigmoid)
        // obj = 1.0f / (1.0f + expf(-obj));  // Bỏ comment nếu output chưa sigmoid

        if (obj < 0.3f) continue;   // Lọc sớm để tiết kiệm thời gian

        // Đọc confidence class "person" (index 0)
        float classScore;
        int personIdx = 5 + YOLOX_PERSON_CLASS;   // 5 + 0 = 5
        if (isQuantized) {
            int8_t raw = output->data.int8[i * numValues + personIdx];
            classScore = ((float)raw - outZero) * outScale;
        } else {
            classScore = output->data.f[i * numValues + personIdx];
        }

        // Điểm cuối cùng = objectness × class_score
        float finalScore = obj * classScore;

        if (finalScore < AI_CONFIDENCE_THRESH) continue;

        // Đọc bounding box (x_center, y_center, width, height) — đã normalize [0,1]
        float bx, by, bw, bh;
        if (isQuantized) {
            bx = ((float)output->data.int8[i * numValues + 0] - outZero) * outScale;
            by = ((float)output->data.int8[i * numValues + 1] - outZero) * outScale;
            bw = ((float)output->data.int8[i * numValues + 2] - outZero) * outScale;
            bh = ((float)output->data.int8[i * numValues + 3] - outZero) * outScale;
        } else {
            bx = output->data.f[i * numValues + 0];
            by = output->data.f[i * numValues + 1];
            bw = output->data.f[i * numValues + 2];
            bh = output->data.f[i * numValues + 3];
        }

        // Normalize về [0, 1] nếu output ở pixel coordinates
        bx /= (float)AI_INPUT_W;
        by /= (float)AI_INPUT_H;
        bw /= (float)AI_INPUT_W;
        bh /= (float)AI_INPUT_H;

        // Clamp giá trị
        bx = fmaxf(0.0f, fminf(1.0f, bx));
        by = fmaxf(0.0f, fminf(1.0f, by));
        bw = fmaxf(0.01f, fminf(1.0f, bw));
        bh = fmaxf(0.01f, fminf(1.0f, bh));

        // Thêm vào danh sách box thô
        if (rawCount < AI_MAX_DETECTIONS * 3) {   // Buffer lớn hơn cho NMS
            rawBoxes[rawCount].x = bx;
            rawBoxes[rawCount].y = by;
            rawBoxes[rawCount].w = bw;
            rawBoxes[rawCount].h = bh;
            rawBoxes[rawCount].confidence = finalScore;
            rawCount++;
        }
    }
}

// ===================================================================
//  HÀM CÔNG KHAI (PUBLIC API)
// ===================================================================

bool inferenceInit() {
    Serial.println("[AI] Đang khởi tạo TFLite Micro...");

    // 1. Load model từ Flash (PROGMEM)
    model = tflite::GetModel(person_detect_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[AI] ✖ Phiên bản model (%lu) không khớp schema (%d)\n",
                      model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }
    Serial.println("[AI]   ✔ Model loaded từ Flash");

    // 2. Cấp phát tensor arena trong PSRAM
    if (!psramFound()) {
        Serial.println("[AI] ✖ PSRAM KHÔNG tìm thấy! Không đủ RAM cho tensor arena.");
        return false;
    }

    tensorArena = (uint8_t*)ps_malloc(TENSOR_ARENA_SIZE);
    if (!tensorArena) {
        Serial.printf("[AI] ✖ Không thể cấp phát %d KB PSRAM cho tensor arena\n",
                      TENSOR_ARENA_SIZE / 1024);
        return false;
    }
    Serial.printf("[AI]   ✔ Tensor arena: %d KB (PSRAM)\n", TENSOR_ARENA_SIZE / 1024);

    // 3. Tạo ops resolver (đăng ký tất cả phép toán TFLite)
    static tflite::AllOpsResolver resolver;

    // 3b. Error reporter (thư viện cũ bắt buộc)
    static tflite::MicroErrorReporter microErrorReporter;
    tflite::ErrorReporter* errorReporter = &microErrorReporter;

    // 4. Tạo interpreter
    static tflite::MicroInterpreter staticInterpreter(
        model, resolver, tensorArena, TENSOR_ARENA_SIZE, errorReporter);
    interpreter = &staticInterpreter;

    // 5. Cấp phát tensors
    TfLiteStatus status = interpreter->AllocateTensors();
    if (status != kTfLiteOk) {
        Serial.println("[AI] ✖ AllocateTensors() thất bại!");
        return false;
    }

    // 6. Lấy con trỏ input tensor
    inputTensor = interpreter->input(0);
    Serial.printf("[AI]   ✔ Input tensor: [%d, %d, %d, %d] type=%d\n",
                  inputTensor->dims->data[0],
                  inputTensor->dims->data[1],
                  inputTensor->dims->data[2],
                  inputTensor->dims->data[3],
                  inputTensor->type);

    // In thông tin output tensor
    TfLiteTensor* outputTensor = interpreter->output(0);
    Serial.printf("[AI]   ✔ Output tensor: dims=%d", outputTensor->dims->size);
    for (int i = 0; i < outputTensor->dims->size; i++) {
        Serial.printf(" [%d]=%d", i, outputTensor->dims->data[i]);
    }
    Serial.printf(" type=%d\n", outputTensor->type);

    Serial.printf("[AI] ✔ TFLite Micro khởi tạo thành công! Arena used: %zu bytes\n",
                  interpreter->arena_used_bytes());
    return true;
}

InferenceResult runInference(const uint8_t* rgbInput) {
    InferenceResult result;
    result.success        = false;
    result.personCount    = 0;
    result.detectionCount = 0;
    result.inferenceTimeMs = 0;

    if (!interpreter || !inputTensor || !rgbInput) {
        Serial.println("[AI] ✖ Chưa khởi tạo hoặc input NULL");
        return result;
    }

    uint32_t t0 = millis();

    // ---- Bước 1: Copy dữ liệu vào input tensor ----
    int inputSize = AI_INPUT_W * AI_INPUT_H * AI_INPUT_CHANNELS;

    if (inputTensor->type == kTfLiteFloat32) {
        // Model float32: normalize pixel [0,255] → [0,1]
        float* inputData = inputTensor->data.f;
        for (int i = 0; i < inputSize; i++) {
            inputData[i] = (float)rgbInput[i] / 255.0f;
        }
    }
    else if (inputTensor->type == kTfLiteInt8) {
        // Model quantized INT8: áp dụng quantization parameters
        float scale = inputTensor->params.scale;
        int   zero  = inputTensor->params.zero_point;
        int8_t* inputData = inputTensor->data.int8;
        for (int i = 0; i < inputSize; i++) {
            float normalized = (float)rgbInput[i] / 255.0f;
            int quantized = (int)roundf(normalized / scale) + zero;
            quantized = max(-128, min(127, quantized));
            inputData[i] = (int8_t)quantized;
        }
    }
    else if (inputTensor->type == kTfLiteUInt8) {
        // Model quantized UINT8: copy trực tiếp (pixel đã là 0–255)
        memcpy(inputTensor->data.uint8, rgbInput, inputSize);
    }
    else {
        Serial.printf("[AI] ✖ Kiểu input tensor không hỗ trợ: %d\n", inputTensor->type);
        return result;
    }

    // ---- Bước 2: Chạy inference ----
    TfLiteStatus status = interpreter->Invoke();
    if (status != kTfLiteOk) {
        Serial.println("[AI] ✖ Inference thất bại!");
        return result;
    }

    uint32_t inferenceMs = millis() - t0;

    // ---- Bước 3: Parse output → danh sách box thô ----
    TfLiteTensor* outputTensor = interpreter->output(0);

    // Buffer tạm cho box thô (trước NMS)
    const int MAX_RAW = AI_MAX_DETECTIONS * 3;
    BBox rawBoxes[MAX_RAW];
    int rawCount = 0;

    parseYoloxOutput(outputTensor, rawBoxes, rawCount);

    // ---- Bước 4: Non-Max Suppression ----
    BBox nmsBoxes[AI_MAX_DETECTIONS];
    int nmsCount = 0;
    applyNMS(rawBoxes, rawCount, AI_NMS_IOU_THRESH,
             nmsBoxes, nmsCount, AI_MAX_DETECTIONS);

    // ---- Bước 5: Ghi kết quả ----
    result.success         = true;
    result.personCount     = nmsCount;
    result.detectionCount  = nmsCount;
    result.inferenceTimeMs = inferenceMs;

    for (int i = 0; i < nmsCount && i < AI_MAX_DETECTIONS; i++) {
        result.detections[i] = nmsBoxes[i];
    }

    // ---- Cập nhật FPS history ----
    inferenceHistory[fpsIdx] = inferenceMs;
    fpsIdx = (fpsIdx + 1) % FPS_HISTORY_SIZE;
    if (fpsCount < FPS_HISTORY_SIZE) fpsCount++;

    Serial.printf("[AI] ✔ Inference: %lu ms | Người: %d | Boxes thô: %d → NMS: %d\n",
                  inferenceMs, nmsCount, rawCount, nmsCount);

    return result;
}

void inferenceCleanup() {
    if (tensorArena) {
        free(tensorArena);
        tensorArena = nullptr;
    }
    interpreter = nullptr;
    inputTensor = nullptr;
    model = nullptr;
    Serial.println("[AI] Đã giải phóng tài nguyên TFLite.");
}

float getAverageFPS() {
    if (fpsCount == 0) return 0.0f;

    uint32_t totalMs = 0;
    for (int i = 0; i < fpsCount; i++) {
        totalMs += inferenceHistory[i];
    }
    float avgMs = (float)totalMs / fpsCount;
    if (avgMs <= 0.0f) return 0.0f;

    return 1000.0f / avgMs;   // FPS = 1000 / avgMs
}
