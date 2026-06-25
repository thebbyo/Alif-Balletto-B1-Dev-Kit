import os
import urllib.request
import numpy as np
import tensorflow as tf
from sklearn.datasets import load_iris
from sklearn.model_selection import train_test_split

print("1. Load Data")
iris = load_iris()
X = iris.data.astype(np.float32)
y = iris.target

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

print("2. Build and Train a Tiny Model")
model = tf.keras.Sequential([
    tf.keras.layers.Dense(16, activation='relu', input_shape=(4,)),
    tf.keras.layers.Dense(3, activation='softmax')
])
model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
model.fit(X_train, y_train, epochs=200, verbose=0)
print("Model Trained!")

print("3. Quantize to INT8")
def representative_dataset():
    for i in range(len(X_train)):
        yield [X_train[i:i+1]]

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8
tflite_quant_model = converter.convert()

with open("iris_quant.tflite", "wb") as f:
    f.write(tflite_quant_model)
print("Model Quantized to INT8!")

print("4. Create Vela Configuration")
ini_content = """
[System_Config.Ethos_U55_High_End_Embedded]
core_clock=500e6
axi0_port=Sram
axi1_port=OffChipFlash
Sram_clock=500e6
OffChipFlash_clock=500e6

[Memory_Mode.Shared_Sram]
const_mem_area=Axi1
arena_mem_area=Axi0
cache_mem_area=Axi0
"""
with open("my_vela_cfg.ini", "w") as f:
    f.write(ini_content)

print("5. Compile with ARM Vela for Ethos-U55")
# Use raw string r"" or forward slashes to prevent escape char issues
vela_cmd = r'"C:\Users\Notes PC\AppData\Local\Programs\Python\Python311\Scripts\vela.exe" iris_quant.tflite --accelerator-config ethos-u55-128 --optimise Performance --memory-mode Shared_Sram --output-dir .'
os.system(vela_cmd)

print("6. Convert to C-Array (model.h)")
try:
    with open("iris_quant_vela.tflite", "rb") as f:
        vela_data = f.read()

    os.makedirs("src", exist_ok=True)
    with open("src/model.h", "w") as f:
        f.write("#ifndef MODEL_H\n#define MODEL_H\n\n")
        f.write("const unsigned char model_data[] __attribute__((aligned(16))) = {\n")
        for i, byte in enumerate(vela_data):
            f.write(f"0x{byte:02x}, ")
            if (i + 1) % 12 == 0: f.write("\n")
        f.write("\n};\n\n")
        f.write(f"const int model_data_len = {len(vela_data)};\n\n")
        f.write("#endif // MODEL_H\n")

    print("\nSUCCESS! model.h generated at src/model.h")
except FileNotFoundError:
    print("\n[!] Vela failed to compile the model. Please check the output logs.")
