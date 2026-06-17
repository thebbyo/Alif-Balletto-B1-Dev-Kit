#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Clock Control for GPIO4 */
#define CLKCTL_PER_SLV_GPIO_CTRL4 (0x4902F000 + 0x80 + (4 * 4))
#define GPIO_CTRL_CKEN            (1 << 16)

/* GPIO4 Base Address on Alif Balletto */
#define GPIO4_BASE 0x49004000
#define GPIO4_DR   (GPIO4_BASE + 0x00) /* Data Register */
#define GPIO4_DDR  (GPIO4_BASE + 0x04) /* Data Direction Register */

/* RGB LED Pins on Port 4 */
#define LED_RED    (1 << 7) /* P4_7 */
#define LED_GREEN  (1 << 5) /* P4_5 */
#define LED_BLUE   (1 << 3) /* P4_3 */

int main(void)
{
    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart_dev)) {
        return 0; // Exit safely if UART is dead
    }

    // --- ENABLE PHYSICAL LED HARDWARE ---
    // 1. Ungate the clock to GPIO4 to prevent Bus Faults
    uint32_t clk_ctrl = sys_read32(CLKCTL_PER_SLV_GPIO_CTRL4);
    clk_ctrl |= GPIO_CTRL_CKEN;
    sys_write32(clk_ctrl, CLKCTL_PER_SLV_GPIO_CTRL4);

    // 2. Configure LED pins as Output
    uint32_t ddr = sys_read32(GPIO4_DDR);
    ddr |= (LED_RED | LED_GREEN | LED_BLUE);
    sys_write32(ddr, GPIO4_DDR);

    // 3. Start with LEDs OFF
    uint32_t dr = sys_read32(GPIO4_DR);
    dr &= ~(LED_RED | LED_GREEN | LED_BLUE);
    sys_write32(dr, GPIO4_DR);

    printk("\r\n\r\n========================================\r\n");
    printk("  BALLETTO B1 - PHYSICAL LED UART CLI\r\n");
    printk("========================================\r\n");
    printk("Hardware clocks are now ENABLED! Look at your board!\r\n");
    printk("Available Commands: red, green, blue, white, off, blink <ms>, steady\r\n");
    printk("cmd> ");

    char line[64];
    int line_idx = 0;

    char current_color[8] = "OFF";
    bool is_blinking = false;
    uint32_t blink_delay_ms = 500;

    int64_t last_blink_time = k_uptime_get();
    bool led_toggle = false;

    while (1) {
        // 1. Handle UART Input
        unsigned char c;
        if (uart_poll_in(uart_dev, &c) == 0) {
            // Echo character back
            uart_poll_out(uart_dev, c);

            if (c == '\r' || c == '\n') {
                line[line_idx] = '\0';
                printk("\n");
                
                if (line_idx > 0) {
                    if (strncmp(line, "red", 3) == 0) {
                        strcpy(current_color, "RED");
                        printk("Color set to RED\n");
                    } else if (strncmp(line, "green", 5) == 0) {
                        strcpy(current_color, "GREEN");
                        printk("Color set to GREEN\n");
                    } else if (strncmp(line, "blue", 4) == 0) {
                        strcpy(current_color, "BLUE");
                        printk("Color set to BLUE\n");
                    } else if (strncmp(line, "white", 5) == 0) {
                        strcpy(current_color, "WHITE");
                        printk("Color set to WHITE\n");
                    } else if (strncmp(line, "off", 3) == 0) {
                        strcpy(current_color, "OFF");
                        is_blinking = false;
                        printk("LED turned OFF\n");
                    } else if (strncmp(line, "steady", 6) == 0) {
                        is_blinking = false;
                        printk("Mode set to STEADY\n");
                    } else if (strncmp(line, "blink", 5) == 0) {
                        int speed = 500;
                        if (line_idx > 6) {
                            speed = atoi(&line[6]);
                            if (speed < 10) speed = 10;
                        }
                        is_blinking = true;
                        blink_delay_ms = speed;
                        printk("Mode set to BLINK (%d ms)\n", speed);
                    } else {
                        printk("Unknown command: '%s'\n", line);
                    }
                }
                line_idx = 0;
                printk("cmd> ");
            } else if (c == '\b' || c == 0x7F) { // Backspace
                if (line_idx > 0) {
                    line_idx--;
                    printk(" \b"); // Clear char on screen
                }
            } else if (line_idx < sizeof(line) - 1) {
                line[line_idx++] = c;
            }
        }

        // 2. Handle LED State (Physical Non-blocking)
        int64_t now = k_uptime_get();
        if (is_blinking && strcmp(current_color, "OFF") != 0) {
            if (now - last_blink_time >= blink_delay_ms) {
                last_blink_time = now;
                led_toggle = !led_toggle;
                
                dr = sys_read32(GPIO4_DR);
                dr &= ~(LED_RED | LED_GREEN | LED_BLUE); // clear all
                if (led_toggle) {
                    if (strcmp(current_color, "RED") == 0) dr |= LED_RED;
                    else if (strcmp(current_color, "GREEN") == 0) dr |= LED_GREEN;
                    else if (strcmp(current_color, "BLUE") == 0) dr |= LED_BLUE;
                    else if (strcmp(current_color, "WHITE") == 0) dr |= (LED_RED | LED_GREEN | LED_BLUE);
                }
                sys_write32(dr, GPIO4_DR);
            }
        } else {
            // Steady state
            dr = sys_read32(GPIO4_DR);
            dr &= ~(LED_RED | LED_GREEN | LED_BLUE); // clear all
            if (strcmp(current_color, "RED") == 0) dr |= LED_RED;
            else if (strcmp(current_color, "GREEN") == 0) dr |= LED_GREEN;
            else if (strcmp(current_color, "BLUE") == 0) dr |= LED_BLUE;
            else if (strcmp(current_color, "WHITE") == 0) dr |= (LED_RED | LED_GREEN | LED_BLUE);
            sys_write32(dr, GPIO4_DR);
        }

        // Sleep very briefly to prevent CPU hogging, but fast enough for responsive CLI
        k_msleep(5);
    }

    return 0;
}
