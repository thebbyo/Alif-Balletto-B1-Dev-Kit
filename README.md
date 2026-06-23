# 🌸 Iris TinyML Classifier on Alif Balletto B1 (Ethos-U55 NPU)

This repository contains a cutting-edge embedded Machine Learning application running on the **Alif Balletto B1 DevKit**. It performs real-time classification of the famous Iris flower dataset by utilizing the onboard **Arm Ethos-U55 microNPU (Neural Processing Unit)** for hardware-accelerated inference.

## 🚀 Key Features

* **True NPU Hardware Acceleration:** Completely bypasses the Cortex-M55 CPU for heavy lifting, feeding math operations directly into the Ethos-U55 NPU via TensorFlow Lite Micro.
* **Dual AXI Split Memory Architecture:** The model was compiled via the Arm Vela compiler (`--memory-mode Shared_Sram`) to optimize extreme memory bandwidth:
  * **AXI1:** Fetches constant model weights directly from the global MRAM (`0x80000000`).
  * **AXI0:** Fetches dynamic `tensor_arena` variables directly from the High-Efficiency Cortex-M55 DTCM (`0x58800000` global alias).
* **Zephyr HAL Driver Overrides:** Contains custom low-level memory remapping (`ethosu_address_remap`) and firewall bridging logic (`ethosu_config_select`) to fix routing bugs in the Zephyr Ethos-U drivers.
* **Real-Time Interactive Serial Pipeline:** A built-in UART loop continuously listens for live incoming floating-point data, quantizes it to `int8_t` in microseconds, and streams it to the NPU.
* **Physical RGB Feedback:** Actively changes the onboard RGB LEDs based on the NPU's highest-probability species classification.

## 🗂️ Project Structure

* `src/main.cpp`: The core C++ Zephyr application. Contains the TFLM setup, UART listener, custom memory overrides, and NPU invocation logic.
* `src/model.cpp` / `model.h`: The 8-bit quantized TensorFlow Lite model, heavily optimized by the Arm Vela compiler.
* `app.overlay`: The Zephyr Device Tree Overlay enabling hardware clocks for GPIO/LEDs and assigning memory regions.
* `prj.conf`: Zephyr Kconfig definitions enabling C++17, TFLM, and the Ethos-U drivers.
* `deploy.ps1`: An ultra-convenient, one-click PowerShell deployment script.
* `iris_simulator.py`: A Python Tkinter GUI that serves as a remote control to stream live data to the board.

## 🛠️ How to Build & Flash

This project relies on the Alif Zephyr SDK and Security Tools (`alif_setools`). 
To automatically build, pad the firmware, generate the Secure Enclave TOC, flash it over COM3, and launch the Python simulator, simply run our generalized deployment script:

```powershell
.\deploy.ps1 C:\alif-workspace\iris-classifier
```

*Note: The script will prompt you to press the physical `RESET` button on the board when the flasher is connecting.*

## 🎮 Real-Time Simulator Demo

Once the firmware is successfully flashed, the `deploy.ps1` script will automatically open the Python **Real-Time NPU Simulator GUI**. 

1. Ensure your board is connected via USB (`COM3`).
2. The GUI will present 4 sliders representing:
   * Sepal Length (cm)
   * Sepal Width (cm)
   * Petal Length (cm)
   * Petal Width (cm)
3. **Drag the sliders!** As you move them, the Python script instantly transmits the floating-point values over UART to the board.
4. The Alif Balletto's Cortex-M55 quantizes the floats, and the Ethos-U55 NPU classifies the flower species in real-time.
5. Watch the physical RGB LED on the board change colors dynamically:
   * 🔴 **RED LED** = Iris Setosa
   * 🟢 **GREEN LED** = Iris Versicolor
   * 🔵 **BLUE LED** = Iris Virginica
6. The exact probability scores are piped back over UART and displayed immediately on your computer screen.

---
*Developed for the Alif Balletto B1 DevKit.*
