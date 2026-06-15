#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_io.h>

/* GPIO4 Base Address on Alif Balletto / Ensemble */
#define GPIO4_BASE 0x49004000
#define GPIO4_DR   (GPIO4_BASE + 0x00) /* Data Register */
#define GPIO4_DDR  (GPIO4_BASE + 0x04) /* Data Direction Register */

/* RGB LED Pins on Port 4 */
#define LED_RED    (1 << 7) /* P4_7 */
#define LED_GREEN  (1 << 5) /* P4_5 */
#define LED_BLUE   (1 << 3) /* P4_3 */

int main(void)
{
    printk("Hello World from Balletto B1 DK!\n");
    printk("Blinking the RGB LED on Port 4 via memory-mapped I/O!\n");

    /* Configure LED pins as Output */
    uint32_t ddr = sys_read32(GPIO4_DDR);
    ddr |= (LED_RED | LED_GREEN | LED_BLUE);
    sys_write32(ddr, GPIO4_DDR);

    int state = 0;
    while (1) {
        /* Read current state of Port 4 */
        uint32_t dr = sys_read32(GPIO4_DR);
        
        /* Clear the RGB pins (Turn OFF) */
        dr &= ~(LED_RED | LED_GREEN | LED_BLUE);
        
        /* Cycle through colors */
        if (state == 0) {
            dr |= LED_RED;
            printk("LED State: RED\n");
        } else if (state == 1) {
            dr |= LED_GREEN;
            printk("LED State: GREEN\n");
        } else if (state == 2) {
            dr |= LED_BLUE;
            printk("LED State: BLUE\n");
        } else if (state == 3) {
            dr |= (LED_RED | LED_GREEN | LED_BLUE);
            printk("LED State: WHITE\n");
        } else {
            printk("LED State: OFF\n");
        }
        
        /* Write the new state back to Port 4 */
        sys_write32(dr, GPIO4_DR);
        
        state++;
        if (state > 4) {
            state = 0;
        }
        
        k_msleep(1000);
    }
    return 0;
}
