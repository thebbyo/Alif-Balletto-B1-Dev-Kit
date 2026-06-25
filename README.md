# Alif Ethos-U55 OCR Classifier Firmware

This folder contains the C++ firmware required to run the custom-trained Image-based (Offline) Handwriting OCR model directly on the **Alif Balletto B1** (or any Alif MCU with an ARM Ethos-U55 NPU).

## Overview

This firmware is designed to load an INT8 quantized TensorFlow Lite model that has been compiled by the **ARM Vela Compiler**, and execute it using the TensorFlow Lite for Microcontrollers (TFLM) framework.

It specifically handles the Continuous Temporal Classification (CTC) decoding logic required to convert the raw sequence outputs from the Neural Network back into human-readable text.

## File Structure

*   `src/main.cpp`: The core firmware loop. It initializes the NPU, sets up the TFLite MicroInterpreter, allocates the Tensor Arena, and handles the CTC greedy decoding algorithm.
*   `src/model.h`: The C-byte array header file containing the Vela-compiled INT8 model (`word_ocr_model_vela.tflite`). 

## Technical Specifications

*   **Model Input Shape:** `[1, 32, 128, 1]` (INT8)
*   **Model Output Shape:** `[1, 32, 63]` (INT8)
*   **Memory Footprint:** 
    *   Flash Usage: ~700 KB (Fits well within the 1.75MB limit)
    *   SRAM Arena Size: 500 KB (Adjustable in `main.cpp` via `tensor_arena_size`)
*   **Vocabulary:** 62 characters (0-9, a-z, A-Z) + 1 CTC Blank Token (`[UNK]`).

## How to Update the Model

If you retrain the Python model in the parent directory, follow these steps to update the firmware:

1.  Export the model to TFLite using `export_tflite.py`.
2.  Compile the model for the Ethos-U55 using `compile_kaggle_model.py`.
3.  Convert the output `word_ocr_model_vela.tflite` into a C-array using `xxd`:
    ```bash
    xxd -i word_ocr_model_vela.tflite > ocr-classifier/src/model.h
    ```
4.  Rebuild the firmware using your standard Alif deployment toolchain and flash it to the board.

## CTC Decoding Note

Because the CTC Loss function (`tf.nn.ctc_loss`) cannot be natively compiled to INT8 operations on the NPU, the decoding step must happen *after* inference on the CPU. The `main.cpp` file contains a lightweight C++ greedy decoder that:
1. Discards consecutive duplicate characters.
2. Discards the CTC Blank Token (Index 63).
3. Maps the remaining indices back to the ASCII character set.
