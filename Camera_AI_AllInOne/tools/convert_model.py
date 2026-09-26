"""
=================================================================
  SCRIPT CONVERT MODEL YoloX-nano → TFLite INT8 → C Header
  Hệ thống đếm người — Nhóm 6
  
  Chạy trên MÁY TÍNH (không chạy trên ESP32!)
  
  Yêu cầu cài đặt:
    pip install onnx onnxruntime onnx-tf tensorflow numpy pillow

  Cách dùng:
    python convert_model.py
    
  Kết quả:
    → File person_detect_model.h (copy vào thư mục include/ của project)
=================================================================
"""

import glob
import os
import sys
import struct
import subprocess
import shutil
import numpy as np

if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

# ======================== CẤU HÌNH ========================
INPUT_SIZE    = 96          # Kích thước ảnh đầu vào (96×96)
NUM_CHANNELS  = 3           # RGB
OUTPUT_FILE   = "person_detect_model.h"   # File header C output

# ======================== BƯỚC 0: Kiểm tra thư viện ========================
def check_dependencies():
    """Kiểm tra các thư viện Python cần thiết"""
    missing = []
    for pkg in ['onnx', 'onnxruntime', 'tensorflow', 'numpy', 'PIL']:
        try:
            __import__(pkg)
        except ImportError:
            actual = 'pillow' if pkg == 'PIL' else pkg
            missing.append(actual)
    
    if missing:
        print("⚠ Thiếu thư viện! Chạy lệnh sau để cài:")
        print(f"  pip install {' '.join(missing)}")
        print()
        # Tự động cài nếu user đồng ý
        ans = input("Cài tự động? (y/n): ").strip().lower()
        if ans == 'y':
            subprocess.check_call([sys.executable, '-m', 'pip', 'install'] + missing)
        else:
            sys.exit(1)

# ======================== BƯỚC 1: Tải model YoloX-nano ========================
def download_yolox_nano():
    """
    Tải model YoloX-nano pretrained (COCO) dạng ONNX.
    Model này đã được train sẵn nhận diện 80 class COCO, 
    bao gồm class 'person' (index 0).
    """
    onnx_url = "https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_nano.onnx"
    onnx_file = "yolox_nano.onnx"
    
    if os.path.exists(onnx_file):
        print(f"✔ Đã có file {onnx_file}, bỏ qua tải.")
        return onnx_file
    
    print(f"⏳ Đang tải YoloX-nano ONNX từ GitHub...")
    print(f"   URL: {onnx_url}")
    
    import urllib.request
    urllib.request.urlretrieve(onnx_url, onnx_file)
    
    size_mb = os.path.getsize(onnx_file) / (1024 * 1024)
    print(f"✔ Tải xong: {onnx_file} ({size_mb:.1f} MB)")
    return onnx_file

# ======================== BƯỚC 2: Thay đổi input size ========================
def resize_onnx_input(onnx_file, target_size=96):
    """
    Thay đổi input shape của model ONNX từ kích thước gốc (416×416) 
    sang kích thước nhỏ hơn (96×96) để chạy được trên ESP32.
    """
    import onnx
    from onnx import helper, TensorProto
    
    print(f"⏳ Đang thay đổi input size → {target_size}×{target_size}...")
    
    model = onnx.load(onnx_file)
    
    # Thay đổi input shape
    for inp in model.graph.input:
        for dim in inp.type.tensor_type.shape.dim:
            if dim.dim_value > 3:  # Chỉ thay đổi H, W (không đổi batch, channels)
                dim.dim_value = target_size
    
    resized_file = f"yolox_nano_{target_size}x{target_size}.onnx"
    onnx.save(model, resized_file)
    print(f"✔ Đã tạo: {resized_file}")
    return resized_file

# ======================== BƯỚC 3: ONNX → TensorFlow SavedModel ========================
def onnx_to_savedmodel(onnx_file):
    """Chuyển từ ONNX sang TensorFlow SavedModel bằng onnx2tf (chuẩn mới)"""
    saved_model_dir = "yolox_savedmodel"
    
    if os.path.exists(os.path.join(saved_model_dir, "saved_model.pb")):
        print(f"✔ Đã có {saved_model_dir}/saved_model.pb, bỏ qua convert.")
        return saved_model_dir
    
    print(f"⏳ Đang convert ONNX → TF SavedModel bằng onnx2tf...")
    print(f"   (Bước này có thể mất vài phút)")
    
    import onnx2tf
    import onnx2tf.onnx2tf as o2t
    import onnx2tf.utils.common_functions as cfuncs
    dummy_data_fn = lambda: np.zeros((1, 96, 96, 3), dtype=np.float32)
    o2t.download_test_image_data = dummy_data_fn
    cfuncs.download_test_image_data = dummy_data_fn

    onnx2tf.convert(
        input_onnx_file_path=onnx_file,
        output_folder_path=saved_model_dir,
        output_signaturedefs=True,
        check_onnx_tf_outputs_elementwise_close=False,
        check_onnx_tf_outputs_elementwise_close_full=False,
        non_verbose=True
    )
    
    print(f"✔ SavedModel tạo xong: {saved_model_dir}/")
    return saved_model_dir

# ======================== BƯỚC 4: TF SavedModel → TFLite INT8 ========================
def savedmodel_to_tflite_int8(saved_model_dir, input_size=96):
    """
    Chuyển TF SavedModel → TFLite + Lượng tử hóa INT8 (Post-Training Quantization).
    
    INT8 quantization:
      - Giảm kích thước model 4× (float32 → int8)
      - Tăng tốc inference trên ESP32 (integer-only ops)
      - Cần dữ liệu đại diện (representative dataset) để calibrate
    """
    import tensorflow as tf
    
    tflite_file = f"yolox_nano_{input_size}x{input_size}_int8.tflite"
    
    if os.path.exists(tflite_file):
        print(f"✔ Đã có {tflite_file}, bỏ qua convert.")
        return tflite_file
    
    print(f"⏳ Đang convert SavedModel → TFLite INT8...")
    
    converter = tf.lite.TFLiteConverter.from_saved_model(saved_model_dir)
    
    # Bật lượng tử hóa INT8 toàn phần
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    
    # Hàm tạo dữ liệu đại diện (representative dataset) cho calibration
    # Dùng ảnh ngẫu nhiên — thay bằng ảnh thật để chính xác hơn
    
    
    import glob
    from PIL import Image

    def representative_dataset():
        files = (
            glob.glob("calib_images/*.jpg") + glob.glob("calib_images/*.png") + glob.glob("calib_images/*.jpeg") +
            glob.glob("calib_image/*.jpg") + glob.glob("calib_image/*.png") + glob.glob("calib_image/*.jpeg") +
            glob.glob("tools/calib_images/*.jpg") + glob.glob("tools/calib_images/*.png") +
            glob.glob("tools/calib_image/*.jpg") + glob.glob("tools/calib_image/*.png")
        )
        if not files:
            raise FileNotFoundError("Thư mục calib_images/ hoặc calib_image/ trống, hãy thêm ảnh mẫu!")
        print(f"✔ Đang calibrate lượng tử hóa INT8 với {min(len(files), 100)} ảnh mẫu...")
        for path in files[:100]:
            img = Image.open(path).convert("RGB").resize((input_size, input_size))
            data = np.array(img, dtype=np.float32)[np.newaxis, ...]
            # Nếu model YOLOX cần chuẩn hóa 0-1 thì chia 255:
            # data = data / 255.0
            yield [data]
    
    converter.representative_dataset = representative_dataset
    
    # Convert
    tflite_model = converter.convert()
    
    # Lưu file
    with open(tflite_file, 'wb') as f:
        f.write(tflite_model)
    
    size_kb = os.path.getsize(tflite_file) / 1024
    print(f"✔ TFLite INT8: {tflite_file} ({size_kb:.0f} KB)")
    return tflite_file

# ======================== BƯỚC 5: TFLite → C Header (xxd -i) ========================
def tflite_to_c_header(tflite_file, output_header):
    """
    Chuyển file .tflite thành mảng byte C (tương đương lệnh xxd -i).
    Kết quả là file .h có thể #include trực tiếp vào project ESP32.
    """
    print(f"⏳ Đang chuyển {tflite_file} → {output_header}...")
    
    with open(tflite_file, 'rb') as f:
        data = f.read()
    
    size_bytes = len(data)
    size_kb = size_bytes / 1024
    
    # Tạo nội dung file header C
    lines = []
    lines.append('#ifndef PERSON_DETECT_MODEL_H')
    lines.append('#define PERSON_DETECT_MODEL_H')
    lines.append('')
    lines.append('#include <Arduino.h>')
    lines.append('')
    lines.append('// ===================================================================')
    lines.append(f'//  Model YoloX-nano — TFLite INT8 Quantized')
    lines.append(f'//  Input:  [1, {INPUT_SIZE}, {INPUT_SIZE}, {NUM_CHANNELS}] (RGB)')
    lines.append(f'//  Kích thước: {size_kb:.1f} KB ({size_bytes} bytes)')
    lines.append(f'//  Tạo bởi: convert_model.py')
    lines.append('// ===================================================================')
    lines.append('')
    lines.append(f'alignas(16) const unsigned char person_detect_model[] PROGMEM = {{')
    
    # Ghi dữ liệu dạng hex, mỗi dòng 16 bytes
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
        if i + 16 < len(data):
            hex_str += ','
        lines.append(f'    {hex_str}')
    
    lines.append('};')
    lines.append('')
    lines.append(f'const unsigned int person_detect_model_len = {size_bytes};')
    lines.append('')
    lines.append('#endif // PERSON_DETECT_MODEL_H')
    lines.append('')
    
    with open(output_header, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    
    print(f"✔ Header C tạo xong: {output_header}")
    print(f"   Kích thước model: {size_kb:.1f} KB ({size_bytes} bytes)")
    print(f"   Dòng code: {len(lines)}")
    return output_header

# ======================== MAIN ========================
def main():
    print()
    print("╔═══════════════════════════════════════════════════════╗")
    print("║  CONVERT YoloX-nano → TFLite INT8 → C Header         ║")
    print("║  Hệ thống đếm người — Nhóm 6                         ║")
    print("╚═══════════════════════════════════════════════════════╝")
    print()
    
    # Kiểm tra thư viện
    check_dependencies()
    
    # Bước 1: Tải model
    onnx_file = download_yolox_nano()
    
    # Bước 2: Resize input
    resized_file = resize_onnx_input(onnx_file, INPUT_SIZE)
    
    # Bước 3: ONNX → TF SavedModel
    saved_model_dir = onnx_to_savedmodel(resized_file)
    
    # Bước 4: SavedModel → TFLite INT8
    tflite_file = savedmodel_to_tflite_int8(saved_model_dir, INPUT_SIZE)
    
    # Bước 5: TFLite → C Header
    header_file = tflite_to_c_header(tflite_file, OUTPUT_FILE)
    
    # Tự động copy vào thư mục include nếu có
    target_inc = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "include", OUTPUT_FILE)
    if os.path.exists(os.path.dirname(target_inc)):
        try:
            shutil.copyfile(header_file, target_inc)
            print(f"  ✔ Đã tự động copy vào: {target_inc}")
        except Exception as e:
            print(f"  ⚠ Không thể copy tự động ({e}), hãy copy thủ công.")

    print()
    print("═══════════════════════════════════════════════════════")
    print("  ✔ HOÀN TẤT!")
    print()
    print(f"  File output: {header_file}")
    print()
    print("  BƯỚC TIẾP THEO:")
    print("  1. Mở PlatformIO trong VS Code")
    print("  2. Nhấn Build (✓) và Upload (→) lên ESP32-CAM")
    print("═══════════════════════════════════════════════════════")

if __name__ == '__main__':
    main()
