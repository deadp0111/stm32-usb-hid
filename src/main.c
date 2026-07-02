/* main.c — demo app: press the Black Pill's user button (PA0) to type 'a'. */
#include "regs.h"

void clock_init(void);
void usb_init(void);
void hid_send_report(const uint8_t report[8]);

#define HID_KEY_A 0x04

static void delay(volatile uint32_t n) { while (n--) __asm__("nop"); }

static void button_init(void) {
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    /* PA0 as input; Black Pill button pulls it low when pressed, external pull-up on board */
    GPIOA_MODER &= ~(3UL << 0);
}

static int button_pressed(void) {
    /* PIDR read: bit0 of GPIOA_IDR */
    return !(*(volatile uint32_t *)(GPIOA_BASE + 0x10) & 1UL);
}

int main(void) {
    clock_init();
    button_init();
    usb_init();

    uint8_t key_down[8]  = {0,0, HID_KEY_A, 0,0,0,0,0};
    uint8_t key_up[8]    = {0,0, 0,         0,0,0,0,0};

    int last_state = 0;
    for (;;) {
        int pressed = button_pressed();
        if (pressed && !last_state) {
            hid_send_report(key_down);
            delay(200000);
            hid_send_report(key_up);
        }
        last_state = pressed;
        delay(50000);
    }
}
