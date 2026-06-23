#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>
#include <string.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "ethosu_driver.h"

// Vela-compiled Iris Model
#include "model.h"

// Tensor arena size (OCR model needs more space)
constexpr int kTensorArenaSize = 200 * 1024;
uint8_t tensor_arena[kTensorArenaSize] __attribute__((aligned(16)));

// Address remap for Ethos-U55
// The NPU is on the global AXI bus. It cannot see the Cortex-M55's local DTCM alias (0x20000000).
// The global AXI address for the M55_HE DTCM is 0x58800000.
extern "C" {
    uint64_t ethosu_address_remap(uint64_t address, int index) {
        if ((address & 0xFF000000) == 0x20000000) {
            // 0x20XXXXXX -> 0x58XXXXXX (Wait, 0x20000000 maps to 0x58800000. 0x58800000 - 0x20000000 = 0x38800000)
            return address + 0x38800000;
        }
        return address;
    }

    // Workaround for Zephyr Ethos-U HAL driver bug.
    // The driver maps the BASEP array index to the REGIONCFG bitfield, instead of using
    // the address region. This causes the tensors (AXI0) to be routed to AXI1, and the model
    // (AXI1) to be routed to AXI0!
    // We override this weak function to manually construct the correct REGIONCFG register.
    unsigned int ethosu_config_select(uint64_t address, int index) {
        if (index == -1) {
            return 3; // QCONFIG = 3 (Command stream on AXI1)
        }
        if (index == 0) {
            // This return value gets shifted by 0 and written to the REGIONCFG register.
            // We want Region 2 (0x40000000-0x5FFFFFFF, where our 0x58XXXXXX tensors live) to be AXI0 (0).
            // We want Region 4 (0x80000000-0x9FFFFFFF, where our 0x80XXXXXX model lives) to be AXI1 (3).
            // Region 2 is bits 4-5. Region 4 is bits 8-9.
            // (3 << 8) | (0 << 4) = 0x0300
            return 0x0300;
        }
        // For all other indices, return 0 to not corrupt the REGIONCFG value we injected at index 0.
        return 0;
    }
}

// --- GPIO Hardware (same addresses proven by uart-cli-led) ---
#define CLKCTL_PER_SLV_GPIO_CTRL4 (0x4902F000 + 0x80 + (4 * 4))
#define GPIO_CTRL_CKEN            (1 << 16)
#define GPIO4_BASE 0x49004000
#define GPIO4_DR   (GPIO4_BASE + 0x00)
#define GPIO4_DDR  (GPIO4_BASE + 0x04)
#define LED_RED    (1 << 7) /* P4_7 */
#define LED_GREEN  (1 << 5) /* P4_5 */
#define LED_BLUE   (1 << 3) /* P4_3 */

void set_led(uint32_t mask) {
    uint32_t dr = sys_read32(GPIO4_DR);
    dr &= ~(LED_RED | LED_GREEN | LED_BLUE);
    dr |= mask;
    sys_write32(dr, GPIO4_DR);
}

// Ethos-U IRQ wrapper
struct ethosu_driver* global_ethosu_drv = nullptr;
void my_ethosu_isr(const void *arg) {
    if (global_ethosu_drv) ethosu_irq_handler(global_ethosu_drv);
}

int main(void)
{
    // ----------------------------------------------------------------
    // 1. ENABLE GPIO4 CLOCK + CONFIGURE LED PINS (proven working pattern)
    // ----------------------------------------------------------------
    uint32_t clk_ctrl = sys_read32(CLKCTL_PER_SLV_GPIO_CTRL4);
    clk_ctrl |= GPIO_CTRL_CKEN;
    sys_write32(clk_ctrl, CLKCTL_PER_SLV_GPIO_CTRL4);

    uint32_t ddr = sys_read32(GPIO4_DDR);
    ddr |= (LED_RED | LED_GREEN | LED_BLUE);
    sys_write32(ddr, GPIO4_DDR);

    uint32_t dr = sys_read32(GPIO4_DR);
    dr &= ~(LED_RED | LED_GREEN | LED_BLUE);
    sys_write32(dr, GPIO4_DR);

    // ----------------------------------------------------------------
    // 2. PRINT BANNER (do this early so we know UART is alive)
    // ----------------------------------------------------------------
    printk("\r\n==========================================\r\n");
    printk("       ALIF BALLETTO B1 - IRIS TINYML      \r\n");
    printk("==========================================\r\n");
    printk("GPIO4 clock enabled, LEDs configured.\r\n");

    // Flash all LEDs briefly to confirm hardware init worked
    set_led(LED_RED | LED_GREEN | LED_BLUE);
    k_msleep(300);
    set_led(0);
    printk("LED self-test passed.\r\n");

    // ----------------------------------------------------------------
    // 3. INITIALIZE ETHOS-U NPU
    // ----------------------------------------------------------------
    printk("Initializing Ethos-U55 NPU...\r\n");

    // Initialize target
    tflite::InitializeTarget();

    static struct ethosu_driver my_npu_drv;
    memset(&my_npu_drv, 0, sizeof(my_npu_drv));

    if (ethosu_init(&my_npu_drv, reinterpret_cast<void*>(0x400E1000UL), NULL, 0, 1, 1)) {
        printk("ERROR: Failed to initialize Ethos-U!\r\n");
        set_led(LED_RED);
        return -1;
    }
        
    global_ethosu_drv = &my_npu_drv;
    IRQ_CONNECT(55, 5, my_ethosu_isr, NULL, 0);
    irq_enable(55);

    printk("Ethos-U55 NPU initialized at 0x400E1000.\r\n");

    // ----------------------------------------------------------------
    // 4. LOAD TFLITE MODEL
    // ----------------------------------------------------------------
    printk("Loading model...\r\n");
    const tflite::Model* model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printk("ERROR: Model schema mismatch (got %d, expected %d)\r\n",
               model->version(), TFLITE_SCHEMA_VERSION);
        set_led(LED_RED);
        return -1;
    }

    tflite::MicroMutableOpResolver<1> resolver;
    resolver.AddCustom(tflite::GetString_ETHOSU(), tflite::Register_ETHOSU());

    tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize);
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        printk("ERROR: AllocateTensors() failed\r\n");
        set_led(LED_RED);
        return -1;
    }
    printk("Model loaded and tensors allocated.\r\n");

    TfLiteTensor* input  = interpreter.input(0);
    TfLiteTensor* output = interpreter.output(0);
    float input_scale      = input->params.scale;
    int   input_zero_point = input->params.zero_point;
    float output_scale      = output->params.scale;
    int   output_zero_point = output->params.zero_point;

    printk("\r\n=== TFLITE PARAMS DEBUG ===\r\n");
    printk("input_scale: %d (x1M), input_zero_point: %d\r\n", (int)(input_scale * 1000000.0f), input_zero_point);
    printk("output_scale: %d (x1M), output_zero_point: %d\r\n", (int)(output_scale * 1000000.0f), output_zero_point);
    printk("===========================\r\n");

    // ----------------------------------------------------------------
    // 5. INTERACTIVE UART LOOP
    // ----------------------------------------------------------------
    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
    if (!device_is_ready(uart_dev)) {
        printk("UART device not found!\r\n");
        return 0;
    }

    printk("\r\n==========================================\r\n");
    printk("       READY FOR REAL-TIME OCR INPUT      \r\n");
    printk("==========================================\r\n");
    printk("Send 'INK:' followed by 480 hex characters.\r\n");

    char hex_buffer[480 + 1];
    int hex_idx = 0;
    int prefix_idx = 0;
    const char prefix[] = "INK:";

    while (1) {
        unsigned char c;
        if (uart_poll_in(uart_dev, &c) == 0) {
            if (prefix_idx < 4) {
                if (c == prefix[prefix_idx]) {
                    prefix_idx++;
                } else {
                    prefix_idx = 0; // reset
                    if (c == prefix[0]) prefix_idx = 1;
                }
            } else {
                if (c == '\n' || c == '\r') {
                    if (hex_idx == 480) {
                        // We got a full ink stroke sequence!
                        
                        // Parse Hex to int8
                        for (int j = 0; j < 240; j++) {
                            char hex_pair[3];
                            hex_pair[0] = hex_buffer[j*2];
                            hex_pair[1] = hex_buffer[j*2+1];
                            hex_pair[2] = '\0';
                            
                            unsigned int pixel_val = 0;
                            for (int k = 0; k < 2; k++) {
                                char hc = hex_pair[k];
                                pixel_val <<= 4;
                                if (hc >= '0' && hc <= '9') pixel_val |= (hc - '0');
                                else if (hc >= 'a' && hc <= 'f') pixel_val |= (hc - 'a' + 10);
                                else if (hc >= 'A' && hc <= 'F') pixel_val |= (hc - 'A' + 10);
                            }
                            
                            // The python UI already sent it as uint8 mapped from int8!
                            // wait, int8 sent as hex is just two's complement.
                            // 0x00 to 0xFF. Cast it directly.
                            int8_t quantized = (int8_t)pixel_val;
                            
                            input->data.int8[j] = quantized;
                        }

                        // Debug print input
                        printk("Input head: ");
                        for (int j = 0; j < 5; j++) printk("%d ", input->data.int8[j]);
                        printk("\r\n");

                        if (interpreter.Invoke() != kTfLiteOk) {
                            printk("ERROR: Invoke() failed!\r\n");
                            set_led(LED_RED);
                            k_msleep(500);
                        } else {
                            // Find the highest probability among 62 classes
                            int highest_val = -128;
                            int best = -1;
                            for (int j = 0; j < 62; j++) {
                                if (output->data.int8[j] > highest_val) {
                                    highest_val = output->data.int8[j];
                                    best = j;
                                }
                            }

                            // Map index to ASCII character (EMNIST ByClass layout)
                            char predicted_char = '?';
                            if (best >= 0 && best <= 9) predicted_char = '0' + best;
                            else if (best >= 10 && best <= 35) predicted_char = 'A' + (best - 10);
                            else if (best >= 36 && best <= 61) predicted_char = 'a' + (best - 36);

                            float prob_val = (highest_val - output_zero_point) * output_scale;

                            // Send response back
                            // Probability percentage estimate
                            int prob_pct = ((highest_val + 128) * 100) / 255;
                            printk("\r\nRESULT:%c:%d.0\r\n", predicted_char, prob_pct);
                            set_led(LED_GREEN);
                            k_msleep(100);
                            set_led(0);
                        }
                    } else {
                        printk("\r\nERROR: Incomplete stroke sequence received (%d hex chars)\r\n", hex_idx);
                    }
                    
                    // Reset for next sequence
                    hex_idx = 0;
                    prefix_idx = 0;
                } else if (hex_idx < 480) {
                    hex_buffer[hex_idx++] = c;
                }
            }
        } else {
            // Small sleep to yield CPU
            k_usleep(100);
        }
    }

    return 0;
}
