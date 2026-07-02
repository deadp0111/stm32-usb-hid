/* startup.c — vector table + reset handler for STM32F411, no CMSIS. */
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);
void OTG_FS_IRQHandler(void);

void Reset_Handler(void) {
    /* copy .data from flash to RAM */
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    /* zero .bss */
    dst = &_sbss;
    while (dst < &_ebss) *dst++ = 0;
    main();
    for (;;) {}
}

void Default_Handler(void) { for (;;) {} }

void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));

/* STM32F411 vector table: OTG_FS_IRQn = 67, position 16+67 = 83 in the table (0-indexed from start).
 * Standard F4 vector table layout (positions after the 15 core exceptions), OTG_FS is IRQ#67. */
typedef void (*vector_t)(void);

__attribute__((section(".isr_vector")))
const vector_t vector_table[] = {
    (vector_t)&_estack,      /* 0  initial SP */
    Reset_Handler,            /* 1  */
    NMI_Handler,               /* 2  */
    HardFault_Handler,         /* 3  */
    MemManage_Handler,         /* 4  */
    BusFault_Handler,          /* 5  */
    UsageFault_Handler,        /* 6  */
    0, 0, 0, 0,                 /* 7-10 reserved */
    SVC_Handler,                /* 11 */
    DebugMon_Handler,           /* 12 */
    0,                            /* 13 reserved */
    PendSV_Handler,              /* 14 */
    SysTick_Handler,             /* 15 */
    /* IRQ0..IRQ90: fill with Default_Handler, we only care about IRQ67 (OTG_FS).
     * Vector table index = 16 + IRQn, so OTG_FS (IRQ67) lives at index 83. */
    [16 ... 16+90] = Default_Handler,
    [16+67] = OTG_FS_IRQHandler,
};
