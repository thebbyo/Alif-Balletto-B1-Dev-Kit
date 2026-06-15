# Alif Balletto B1 DevKit - Zephyr Blinky (Zero-to-Hero Guide)

Welcome! This repository provides a complete, working "Blinky" application for the Alif Balletto B1 DevKit using the Zephyr RTOS. 

Because the Balletto B1 DevKit is brand new, the current early-release Zephyr port lacks native GPIO drivers. Standard Zephyr applications (like the official `blinky` sample) will fail to compile or blink because the device tree `led0` alias is missing. 

This repository solves that by using **Memory-Mapped I/O (MMIO)** to directly toggle the Multicolor RGB LED. Furthermore, it walks you through the exact steps to overcome the MRAM 16-byte alignment flashing error and the "Target did not respond" Bootloader synchronization bug.

---

## 1. Prerequisites & Resources

Before you begin, ensure you have the following downloaded and installed:
1. **Zephyr Environment:** Follow the official Zephyr Getting Started guide to install `west`, Python, and CMake. 
2. **Alif Security Toolkit (`alif_setools`):** You must download this proprietary toolkit from the official Alif Semiconductor Developer Portal. It contains the essential tools `app-gen-toc.exe`, `app-write-mram.exe`, and `maintenance.exe` needed to sign and flash firmware.
3. **Alif CMSIS DFP:** Ensure your Zephyr workspace includes the Alif hardware definitions.

---

## 2. The Hardware: LED Pins

By cross-referencing the Balletto board schematic, the onboard Multicolor RGB LED is wired to the **GPIO Port 4** multiplexed pins:
*   🔴 **Red LED:** `P4_7` (Port 4, Pin 7)
*   🟢 **Green LED:** `P4_5` (Port 4, Pin 5)
*   🔵 **Blue LED:** `P4_3` (Port 4, Pin 3)

The application in `src/main.c` writes directly to the `GPIO4` hardware registers (Base Address: `0x49004000`) using Zephyr's `sys_read32()` and `sys_write32()` functions.

---

## 3. Building the Application

To compile the Zephyr application for the High-Efficiency (HE) M55 core, open your Zephyr workspace terminal (e.g., PowerShell) and run:

```powershell
# Navigate to your Zephyr directory
cd C:\alif-workspace\zephyrproject\zephyr

# Build the project
west build -p always -b balletto_b1_dk/ab1c1f4m51820ph0 C:\alif-workspace\blink -d C:\alif-workspace\blink\build
```
This generates the raw firmware file located at: `C:\alif-workspace\blink\build\zephyr\zephyr.bin`.

---

## 4. The Flashing Gotcha: 16-Byte MRAM Alignment

**CRITICAL:** The Alif Secure Enclave Bootloader and the MRAM flashing tool strictly require the `.bin` firmware file to be an exact **multiple of 16 bytes** in size. 

If your compiler generates a binary of exactly `19,764` bytes (which is not perfectly divisible by 16), the flashing tool will throw an error: `!!!WARNING: the SIZE is NOT multiple of 16 bytes`.

### Step A: Pad the Firmware
We have included a Python script to automatically pad your binary with zeroes to round it up to the nearest multiple of 16.

Copy the newly built `zephyr.bin` into your `alif_setools/app-release-exec/build/images/` directory, then run the pad script:

```powershell
python scripts/pad_firmware.py C:\alif-workspace\alif_setools\app-release-exec\build\images\zephyr.bin
```

### Step B: Regenerate the TOC (Table of Contents)
Because the file size changed (we padded it), the Secure Enclave Bootloader will reject the firmware unless we update the cryptographic TOC package. 

From inside the `alif_setools/app-release-exec/` directory, run:
```powershell
.\app-gen-toc.exe -f build\config\zephyr_mram_cfg.json
```
*(Ensure your `zephyr_mram_cfg.json` is configured to point to `zephyr.bin` for the `HE_APP` section).*

---

## 5. Flashing the Board (Hard Maintenance Mode)

If you simply try to run `app-write-mram.exe` right now, you might encounter a `[ERROR] Target did not respond` error. This happens because the *old* program on the board boots up too fast and ignores the flashing tool. 

To reliably flash the board, we must force it into **Hard Maintenance Mode** to halt the boot process *before* the M55 application core boots.

### The Flashing Procedure:

1. **Enter Maintenance Mode:**
   From the `app-release-exec` folder, run:
   ```powershell
   .\maintenance.exe -c COM3
   ```
   *(Change COM3 to your actual Secure Enclave COM port).*

2. **Halt the Board:**
   - Press **`1`** (Device Control)
   - Press **`1`** (Hard maintenance mode)
   - You will see a spinning cursor: `Waiting for Target..[RESET Platform] \`
   - **ACTION:** Press the physical **RESET button** on your DevKit.
   - The tool will catch the board as it reboots.

3. **Erase the Old MRAM:**
   - Press **`Enter`** to return to the Main Menu.
   - Press **`3`** (MRAM).
   - Press **`1`** (Erase MRAM).
   - Once erased, press **`Enter`** repeatedly to exit the maintenance tool.

4. **Flash the New Padded Firmware:**
   Now that the MRAM is wiped and the board is waiting, run:
   ```powershell
   .\app-write-mram.exe -nr -c COM3
   ```
   *(The `-nr` flag tells the flasher not to try resetting the board, since it's already perfectly halted).*

---

## 6. Verifying the Boot Sequence

If everything worked, you should see the Multicolor RGB LED slowly cycling through Red, Green, Blue, and White!

If you want to view the Secure Enclave boot logs to verify exactly how the bootloader injected your Zephyr app, we have included a helpful script. 

Ensure you have `pyserial` installed (`pip install pyserial`), and run:
```powershell
python scripts/verify_boot.py
```
Press the RESET button, and the script will parse the 57600 baud logs and output the Bootloader's internal memory map!
