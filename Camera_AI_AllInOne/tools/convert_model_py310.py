"""
=================================================================
  SCRIPT CONVERT MODEL YoloX-nano → TFLite INT8 → C Header
  Hệ thống đếm người — Nhóm 6
=================================================================
"""

import os
import sys
import struct
import subprocess
import numpy as np

INPUT_SIZE    = 96          # Kích thước ảnh đầu vào (96×96)
NUM_CHANNELS  = 3           # RGB
OUTPUT_FILE   = "person_detect_model.h"   # File header C output

def download_yolox_nano():
    onnx_url = "https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_nano.onnx"
    onnx_file = "yolox_nano.onnx"
    if os.path.exists(onnx_file):
        print(f"✔ Đã có file {onnx_file}, bỏ qua tải.")
        return onnx_file
    print(f"⏳ Đang tải YoloX-nano ONNX từ GitHub...")
    import urllib.request
    urllib.request.urlretrieve(onnx_url, onnx_file)
    return onnx_file

def resize_onnx_input(onnx_file, target_size=96):
    import onnx
    print(f"⏳ Đang thay đổi input size → {target_size}×{target_size}...")
    model = onnx.load(onnx_file)
    for inp in model.graph.input:
        for dim in inp.type.tensor_type.shape.dim:
            if dim.dim_value > 3:  
                dim.dim_value = target_size
    resized_file = f"yolox_nano_{target_size}x{target_size}.onnx"
    onnx.save(model, resized_file)
    return resized_file

def onnx_to_savedmodel(onnx_file):
    import onnx
    from onnx_tf.backend import prepare
    saved_model_dir = "yolox_savedmodel"
    if os.path.exists(os.path.join(saved_model_dir, "saved_model.pb")):
        print(f"✔ Đã có {saved_model_dir}/saved_model.pb, bỏ qua convert.")
        return saved_model_dir
    print(f"⏳ Đang convert ONNX → TF SavedModel...")
    onnx_model = onnx.load(onnx_file)
    tf_rep = prepare(onnx_model)
    tf_rep.export_graph(saved_model_dir)
    return saved_model_dir

def savedmodel_to_tflite_int8(saved_model_dir, input_size=96):
    import tensorflow as tf
    tflite_file = f"yolox_nano_{input_size}x{input_size}_int8.tflite"
    if os.path.exists(tflite_file):
        print(f"✔ Đã có {tflite_file}, bỏ qua convert.")
        return tflite_file
    print(f"⏳ Đang convert SavedModel → TFLite INT8...")
    converter = tf.lite.TFLiteConverter.from_saved_model(saved_model_dir)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    
    def representative_dataset():
        for _ in range(100):
            data = np.random.rand(1, input_size, input_size, 3).astype(np.float32)
            yield [data]
            
    converter.representative_dataset = representative_dataset
    tflite_model = converter.convert()
    with open(tflite_file, 'wb') as f:
        f.write(tflite_model)
    return tflite_file

def tflite_to_c_header(tflite_file, output_header):
    print(f"⏳ Đang chuyển {tflite_file} → {output_header}...")
    with open(tflite_file, 'rb') as f:
        data = f.read()
    size_bytes = len(data)
    lines = []
    lines.append('#ifndef PERSON_DETECT_MODEL_H')
    lines.append('#define PERSON_DETECT_MODEL_H')
    lines.append('')
    lines.append('#include <Arduino.h>')
    lines.append('')
    lines.append(f'alignas(16) const unsigned char person_detect_model[] PROGMEM = {{')
    
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
        if i + 16 < len(data): hex_str += ','
        lines.append(f'    {hex_str}')
        
    lines.append('};')
    lines.append(f'const unsigned int person_detect_model_len = {size_bytes};')
    lines.append('#endif // PERSON_DETECT_MODEL_H')
    
    with open(output_header, 'w') as f:
        f.write('\n'.join(lines))
    print(f"✔ Header C tạo xong: {output_header}")
    return output_header

def main():
    onnx_file = download_yolox_nano()
    resized_file = resize_onnx_input(onnx_file, INPUT_SIZE)
    saved_model_dir = onnx_to_savedmodel(resized_file)
    tflite_file = savedmodel_to_tflite_int8(saved_model_dir, INPUT_SIZE)
    header_file = tflite_to_c_header(tflite_file, OUTPUT_FILE)
    print("HOAN TAT! File output: " + header_file)

if __name__ == '__main__':
    main()
