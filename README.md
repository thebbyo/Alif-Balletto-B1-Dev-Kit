# Alif Balletto B1 - Interactive UART LED CLI

Welcome! This repository contains a fully interactive, bare-metal C application running on Zephyr RTOS for the Alif Balletto B1 DevKit. 

This project demonstrates how to build an interactive string-parsing Command Line Interface over UART0 at 57600 baud, and most importantly, how to manually ungate the Power Management Unit (PMU) clock to physically drive the onboard RGB LED from the M55_HE core.

## Features
- **Interactive UART Polling:** Real-time character polling over UART0 with backspace support.
- **State Machine Parsing:** Interprets text commands (`red`, `green`, `blink 200`, `status`, etc.).
- **Direct Hardware Control:** Bypasses Zephyr's missing PMU drivers to manually ungate the `GPIO4` clock via `CLKCTL_PER_SLV` MMIO writes.

## 1. The PMU Clock Gating Workaround

By default, the Alif Secure Enclave Bootloader disables the clock to the GPIO peripherals when booting into the High-Efficiency (M55_HE) core to save power. Attempting to access the GPIO registers without the clock will trigger a silent Bus Fault. 

This project solves this by directly writing to the PMU before accessing GPIO:
```c
/* Clock Control for GPIO4 */
#define CLKCTL_PER_SLV_GPIO_CTRL4 (0x4902F000 + 0x80 + (4 * 4))
#define GPIO_CTRL_CKEN            (1 << 16)

// Ungate the clock to GPIO4
uint32_t clk_ctrl = sys_read32(CLKCTL_PER_SLV_GPIO_CTRL4);
clk_ctrl |= GPIO_CTRL_CKEN;
sys_write32(clk_ctrl, CLKCTL_PER_SLV_GPIO_CTRL4);
```

## 2. Building the Project

To compile for the M55_HE core, open your Zephyr environment terminal and run:

```powershell
cd C:\alif-workspace\zephyrproject\zephyr
west build -p always -b balletto_b1_dk/ab1c1f4m51820ph0 C:\alif-workspace\uart-cli-led -d C:\alif-workspace\uart-cli-led\build
```

Ensure you pad the resulting binary to a multiple of 16 bytes and generate a new Table of Contents (TOC) using the Alif Security Toolkit before flashing.

## 3. Flashing

Put the board into Hard Maintenance Mode to halt the boot process, then erase and write the MRAM:
```powershell
# Put board in Hard Maintenance Mode (1 -> 1 -> RESET -> 3 -> 1 -> Erase)
.\maintenance.exe -c COM3

# Flash padded binary and TOC
.\app-write-mram.exe -nr -p -c COM3
```

## 4. Usage

To interact with the CLI, use a terminal program (or our provided Python script) connected to **COM3 at 57600 baud**. 

Press the **RESET** button on the DevKit to view the boot logs, followed by the CLI banner:

```text
========================================
  BALLETTO B1 - PHYSICAL LED UART CLI
========================================
Hardware clocks are now ENABLED! Look at your board!
Available Commands: red, green, blue, white, off, blink <ms>, steady
cmd>
```

### Available Commands:
* `red` / `green` / `blue` / `white` - Set a solid color
* `blink <ms>` - Blink the active color at the specified speed (e.g. `blink 100`)
* `steady` - Stop blinking and remain solid
* `off` - Turn off the LED
* `status` - Print the current virtual state engine values
