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

// Vela-compiled Word Model
#include "model.h"

// Tensor arena size
constexpr int kTensorArenaSize = 250 * 1024;
uint8_t tensor_arena[kTensorArenaSize] __attribute__((aligned(16)));

extern "C" {
    uint64_t ethosu_address_remap(uint64_t address, int index) {
        if ((address & 0xFF000000) == 0x20000000) {
            return address + 0x38800000;
        }
        return address;
    }

    unsigned int ethosu_config_select(uint64_t address, int index) {
        if (index == -1) {
            return 3;
        }
        if (index == 0) {
            return 0x0300;
        }
        return 0;
    }
}

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

struct ethosu_driver* global_ethosu_drv = nullptr;
void my_ethosu_isr(const void *arg) {
    if (global_ethosu_drv) ethosu_irq_handler(global_ethosu_drv);
}

int main(void)
{
    uint32_t clk_ctrl = sys_read32(CLKCTL_PER_SLV_GPIO_CTRL4);
    clk_ctrl |= GPIO_CTRL_CKEN;
    sys_write32(clk_ctrl, CLKCTL_PER_SLV_GPIO_CTRL4);

    uint32_t ddr = sys_read32(GPIO4_DDR);
    ddr |= (LED_RED | LED_GREEN | LED_BLUE);
    sys_write32(ddr, GPIO4_DDR);

    uint32_t dr = sys_read32(GPIO4_DR);
    dr &= ~(LED_RED | LED_GREEN | LED_BLUE);
    sys_write32(dr, GPIO4_DR);

    printk("\r\n==========================================\r\n");
    printk("     ALIF BALLETTO B1 - FULL WORD OCR     \r\n");
    printk("==========================================\r\n");

    set_led(LED_RED | LED_GREEN | LED_BLUE);
    k_msleep(300);
    set_led(0);

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

    const tflite::Model* model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printk("ERROR: Model schema mismatch\r\n");
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

    TfLiteTensor* input  = interpreter.input(0);
    TfLiteTensor* output = interpreter.output(0);
    
    printk("\r\nModel Ready. Send 'IMG:' + 8192 hex chars.\r\n");

    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
    if (!device_is_ready(uart_dev)) return 0;

    static char hex_buffer[8192 + 1];
    int hex_idx = 0;
    int prefix_idx = 0;
    const char prefix[] = "IMG:";

    while (1) {
        unsigned char c;
        if (uart_poll_in(uart_dev, &c) == 0) {
            if (prefix_idx < 4) {
                if (c == prefix[prefix_idx]) {
                    prefix_idx++;
                } else {
                    prefix_idx = 0;
                    if (c == prefix[0]) prefix_idx = 1;
                }
            } else {
                if (c == '\n' || c == '\r') {
                    if (hex_idx == 8192) {
                        for (int j = 0; j < 4096; j++) {
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
                            
                            input->data.int8[j] = (int8_t)pixel_val;
                        }

                        if (interpreter.Invoke() != kTfLiteOk) {
                            printk("ERROR: Invoke() failed!\r\n");
                            set_led(LED_RED);
                            k_msleep(500);
                        } else {
                            // CTC Greedy Decoder
                            // Output shape is [1, 32, 63]
                            // Classes: 0-9 (10), a-z (26), A-Z (26), blank (1) = 63 total
                            // wait, the python mapping: char_to_num = 0 to 61.
                            // The python code used StringLookup which assigns 0 for OOV.
                            // Oh wait, tf.keras.layers.StringLookup:
                            // chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            // length is 62.
                            // However, StringLookup usually reserves 0 for mask.
                            // Since mask_token=None, it reserves 0 for OOV? Let's assume indices 1 to 62 are the characters.
                            // Actually, wait, let's map from python chars string.
                            // We will just replicate the vocabulary mapping on device.
                            
                            const char* vocab = "?0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
                            
                            char decoded_word[33];
                            int out_idx = 0;
                            int last_best = -1;
                            
                            for (int t = 0; t < 32; t++) {
                                int best = 0;
                                int highest_val = -128;
                                
                                for (int c = 0; c < 64; c++) {
                                    int val = output->data.int8[t * 64 + c];
                                    if (val > highest_val) {
                                        highest_val = val;
                                        best = c;
                                    }
                                }
                                
                                // 63 is CTC blank token. 0 is StringLookup OOV token.
                                if (best != 63 && best != 0 && best != last_best) {
                                    if (best < 63) {
                                        decoded_word[out_idx++] = vocab[best];
                                    }
                                }
                                last_best = best;
                            }
                            decoded_word[out_idx] = '\0';
                            
                            printk("\r\nRESULT:%s\r\n", decoded_word);
                            set_led(LED_GREEN);
                            k_msleep(100);
                            set_led(0);
                        }
                    } else {
                        printk("\r\nERROR: Incomplete image received (%d hex chars)\r\n", hex_idx);
                    }
                    
                    hex_idx = 0;
                    prefix_idx = 0;
                } else if (hex_idx < 8192) {
                    hex_buffer[hex_idx++] = c;
                }
            }
        } else {
            k_usleep(100);
        }
    }

    return 0;
}
