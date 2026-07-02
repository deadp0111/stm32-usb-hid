/* clock.c — bring up 25 MHz HSE crystal (Black Pill) into the main PLL:
 *   VCO_in  = HSE / M          =  25MHz / 25 = 1 MHz
 *   VCO_out = VCO_in * N       =   1MHz * 384 = 384 MHz
 *   SYSCLK  = VCO_out / P      = 384MHz / 4   = 96 MHz
 *   USB48   = VCO_out / Q      = 384MHz / 8   = 48 MHz   <-- OTG_FS needs exactly this
 */
#include "regs.h"

void clock_init(void) {
    /* 1. Start the external crystal */
    RCC_CR |= RCC_CR_HSEON;
    while (!(RCC_CR & RCC_CR_HSERDY)) {}

    /* 2. Flash wait states for 96MHz @ 3.3V (3 WS per RM0383 table), enable caches/prefetch */
    FLASH_ACR = FLASH_ACR_LATENCY_3WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* 3. Configure main PLL: source = HSE, M=25, N=384, P=4, Q=8 */
    RCC_PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE
                | RCC_PLLCFGR_M(25)
                | RCC_PLLCFGR_N(384)
                | RCC_PLLCFGR_P(4)
                | RCC_PLLCFGR_Q(8);

    /* 4. APB1 max is 50MHz, so divide by 2 from a 96MHz AHB */
    RCC_CFGR |= RCC_CFGR_PPRE1_DIV2;

    /* 5. Turn the PLL on and wait for lock */
    RCC_CR |= RCC_CR_PLLON;
    while (!(RCC_CR & RCC_CR_PLLRDY)) {}

    /* 6. Switch SYSCLK source to the PLL */
    RCC_CFGR = (RCC_CFGR & ~0x3UL) | RCC_CFGR_SW_PLL;
    while ((RCC_CFGR & (0x3UL << 2)) != RCC_CFGR_SWS_PLL) {}
}
