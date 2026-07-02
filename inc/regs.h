/* regs.h — bare-metal register map for STM32F411, RCC/GPIO/USB OTG_FS only.
 * No HAL, no CMSIS device headers — every field is written directly.
 */
#ifndef REGS_H
#define REGS_H

#include <stdint.h>

#define MMIO32(addr) (*(volatile uint32_t *)(addr))

/* ---------------- RCC ---------------- */
#define RCC_BASE        0x40023800UL
#define RCC_CR          MMIO32(RCC_BASE + 0x00)
#define RCC_PLLCFGR     MMIO32(RCC_BASE + 0x04)
#define RCC_CFGR        MMIO32(RCC_BASE + 0x08)
#define RCC_AHB1ENR     MMIO32(RCC_BASE + 0x30)
#define RCC_AHB2ENR     MMIO32(RCC_BASE + 0x34)
#define RCC_APB2ENR     MMIO32(RCC_BASE + 0x44)

#define RCC_CR_HSEON        (1UL << 16)
#define RCC_CR_HSERDY       (1UL << 17)
#define RCC_CR_PLLON        (1UL << 24)
#define RCC_CR_PLLRDY       (1UL << 25)

#define RCC_PLLCFGR_PLLSRC_HSE (1UL << 22)

#define RCC_CFGR_SW_PLL      (2UL << 0)
#define RCC_CFGR_SWS_PLL     (2UL << 2)
#define RCC_CFGR_PPRE1_DIV2  (4UL << 10)   /* APB1 <= 50MHz */

#define RCC_AHB1ENR_GPIOAEN  (1UL << 0)
#define RCC_AHB2ENR_OTGFSEN  (1UL << 7)
#define RCC_APB2ENR_SYSCFGEN (1UL << 14)

#define RCC_PLLCFGR_M(x)  ((x) << 0)
#define RCC_PLLCFGR_N(x)  ((x) << 6)
#define RCC_PLLCFGR_P(x)  ((((x)/2)-1) << 16) /* encodes 2/4/6/8 -> 0/1/2/3 */
#define RCC_PLLCFGR_Q(x)  ((x) << 24)

/* ---------------- FLASH ---------------- */
#define FLASH_BASE      0x40023C00UL
#define FLASH_ACR       MMIO32(FLASH_BASE + 0x00)
#define FLASH_ACR_LATENCY_3WS (3UL << 0)
#define FLASH_ACR_PRFTEN      (1UL << 8)
#define FLASH_ACR_ICEN        (1UL << 9)
#define FLASH_ACR_DCEN        (1UL << 10)

/* ---------------- GPIO ---------------- */
#define GPIOA_BASE      0x40020000UL
#define GPIOA_MODER     MMIO32(GPIOA_BASE + 0x00)
#define GPIOA_OSPEEDR   MMIO32(GPIOA_BASE + 0x08)
#define GPIOA_PUPDR     MMIO32(GPIOA_BASE + 0x0C)
#define GPIOA_AFRH      MMIO32(GPIOA_BASE + 0x24) /* pins 8-15 */

/* Onboard LED on Black Pill = PC13, active low */
#define GPIOC_BASE      0x40020800UL
#define GPIOC_MODER     MMIO32(GPIOC_BASE + 0x00)
#define GPIOC_ODR       MMIO32(GPIOC_BASE + 0x14)

/* ---------------- USB OTG_FS (core) ---------------- */
#define USB_BASE        0x50000000UL

#define OTG_FS_GOTGCTL    MMIO32(USB_BASE + 0x000)
#define OTG_FS_GAHBCFG    MMIO32(USB_BASE + 0x008)
#define OTG_FS_GUSBCFG    MMIO32(USB_BASE + 0x00C)
#define OTG_FS_GRSTCTL    MMIO32(USB_BASE + 0x010)
#define OTG_FS_GINTSTS    MMIO32(USB_BASE + 0x014)
#define OTG_FS_GINTMSK    MMIO32(USB_BASE + 0x018)
#define OTG_FS_GRXSTSR    MMIO32(USB_BASE + 0x01C)
#define OTG_FS_GRXSTSP    MMIO32(USB_BASE + 0x020)
#define OTG_FS_GRXFSIZ    MMIO32(USB_BASE + 0x024)
#define OTG_FS_GNPTXFSIZ  MMIO32(USB_BASE + 0x028)
#define OTG_FS_GCCFG      MMIO32(USB_BASE + 0x038)
#define OTG_FS_DIEPTXF1   MMIO32(USB_BASE + 0x104) /* IN EP1 FIFO (HID) */

/* GAHBCFG */
#define GAHBCFG_GINTMSK   (1UL << 0)
#define GAHBCFG_TXFELVL   (1UL << 7)

/* GUSBCFG */
#define GUSBCFG_TOCAL_MASK  (0x7UL)
#define GUSBCFG_PHYSEL      (1UL << 6)
#define GUSBCFG_FDMOD       (1UL << 30) /* force device mode */
#define GUSBCFG_TRDT_SHIFT  10

/* GRSTCTL */
#define GRSTCTL_CSRST     (1UL << 0)
#define GRSTCTL_RXFFLSH   (1UL << 4)
#define GRSTCTL_TXFFLSH   (1UL << 5)
#define GRSTCTL_TXFNUM_ALL (0x10UL << 6)
#define GRSTCTL_AHBIDL    (1UL << 31)

/* GINTSTS / GINTMSK bits used */
#define GINT_USBRST   (1UL << 12)
#define GINT_ENUMDNE  (1UL << 13)
#define GINT_RXFLVL   (1UL << 4)
#define GINT_IEPINT   (1UL << 18)
#define GINT_OEPINT   (1UL << 19)
#define GINT_USBSUSP  (1UL << 11)
#define GINT_SOF      (1UL << 3)

/* GCCFG */
#define GCCFG_PWRDWN    (1UL << 16)
#define GCCFG_NOVBUSSENS (1UL << 21)

/* ---------------- USB OTG_FS device-mode ---------------- */
#define OTG_FS_DCFG       MMIO32(USB_BASE + 0x800)
#define OTG_FS_DCTL       MMIO32(USB_BASE + 0x804)
#define OTG_FS_DSTS       MMIO32(USB_BASE + 0x808)
#define OTG_FS_DIEPMSK    MMIO32(USB_BASE + 0x810)
#define OTG_FS_DOEPMSK    MMIO32(USB_BASE + 0x814)
#define OTG_FS_DAINT      MMIO32(USB_BASE + 0x818)
#define OTG_FS_DAINTMSK   MMIO32(USB_BASE + 0x81C)

#define DCFG_DSPD_FS      (0x3UL << 0)  /* full speed, internal FS PHY */

#define DAINT_IEPINT0     (1UL << 0)
#define DAINT_IEPINT1     (1UL << 1)
#define DAINT_OEPINT0     (1UL << 16)

#define DOEPMSK_STUPM     (1UL << 3)
#define DOEPMSK_XFRCM     (1UL << 0)
#define DIEPMSK_XFRCM     (1UL << 0)

/* per-endpoint IN registers: base + 0x900 + 0x20*ep */
#define OTG_FS_DIEPCTL(ep)  MMIO32(USB_BASE + 0x900 + 0x20*(ep))
#define OTG_FS_DIEPINT(ep)  MMIO32(USB_BASE + 0x908 + 0x20*(ep))
#define OTG_FS_DIEPTSIZ(ep) MMIO32(USB_BASE + 0x910 + 0x20*(ep))

/* per-endpoint OUT registers: base + 0xB00 + 0x20*ep */
#define OTG_FS_DOEPCTL(ep)  MMIO32(USB_BASE + 0xB00 + 0x20*(ep))
#define OTG_FS_DOEPINT(ep)  MMIO32(USB_BASE + 0xB08 + 0x20*(ep))
#define OTG_FS_DOEPTSIZ(ep) MMIO32(USB_BASE + 0xB10 + 0x20*(ep))

#define DEPCTL_EPENA      (1UL << 31)
#define DEPCTL_EPDIS      (1UL << 30)
#define DEPCTL_SD0PID     (1UL << 28) /* set DATA0 PID */
#define DEPCTL_SNAK       (1UL << 27)
#define DEPCTL_CNAK       (1UL << 26)
#define DEPCTL_USBAEP     (1UL << 15)
#define DEPCTL_EPTYP_CTRL  (0UL << 18)
#define DEPCTL_EPTYP_INTR  (3UL << 18)
#define DEPCTL_MPSIZ_64   (64UL << 0)

#define DOEPTSIZ0_STUPCNT_1 (1UL << 29)
#define DOEPTSIZ0_PKTCNT   (1UL << 19)

#define PCGCCTL           MMIO32(USB_BASE + 0xE00)

/* Data FIFO access: each endpoint has a 4KB window */
#define USB_FIFO(ep)      ((volatile uint32_t *)(USB_BASE + 0x1000 + 0x1000*(ep)))

#endif
