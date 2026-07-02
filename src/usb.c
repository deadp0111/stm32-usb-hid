/* usb.c — bare-metal USB OTG_FS device driver.
 *
 * Everything here talks straight to the OTG_FS register block; there is no
 * ST HAL/CMSIS driver underneath. The flow implemented is:
 *   1. usb_init()      -> core reset, device mode, FIFO sizing, IRQ unmask
 *   2. USB reset IRQ   -> re-arm EP0, clear address
 *   3. Enumeration IRQ -> latch negotiated speed/turnaround time
 *   4. RXFLVL IRQ      -> pull SETUP/OUT packets out of the shared RX FIFO
 *   5. control_in()    -> answer GET_DESCRIPTOR / SET_ADDRESS / SET_CONFIGURATION
 *   6. hid_send_report()-> push a keyboard report into EP1's TX FIFO
 */
#include "regs.h"
#include "usb_desc.h"

static uint8_t  dev_addr_pending = 0;
static uint8_t  dev_configured   = 0;

/* ---- low-level FIFO helpers ---- */
static void fifo_write(uint8_t ep, const uint8_t *buf, uint32_t len) {
    volatile uint32_t *fifo = USB_FIFO(ep);
    uint32_t words = (len + 3) / 4;
    for (uint32_t i = 0; i < words; i++) {
        uint32_t w = 0;
        for (uint32_t b = 0; b < 4 && (i*4+b) < len; b++)
            w |= ((uint32_t)buf[i*4+b]) << (8*b);
        *fifo = w;
    }
}

static void fifo_read(uint8_t *buf, uint32_t len) {
    volatile uint32_t *fifo = USB_FIFO(0);
    uint32_t words = (len + 3) / 4;
    for (uint32_t i = 0; i < words; i++) {
        uint32_t w = *fifo;
        for (uint32_t b = 0; b < 4 && (i*4+b) < len; b++)
            buf[i*4+b] = (uint8_t)(w >> (8*b));
    }
}

/* ---- EP0 IN transmit helper (used for every control-read response) ---- */
static void ep0_transmit(const uint8_t *data, uint16_t len) {
    OTG_FS_DIEPTSIZ(0) = (1UL << 19) /* PKTCNT=1 */ | (len & 0x7F);
    OTG_FS_DIEPCTL(0) |= DEPCTL_CNAK | DEPCTL_EPENA;
    fifo_write(0, data, len);
}

static void ep0_out_prepare(void) {
    /* Re-arm EP0 OUT to accept the next SETUP stage: 1 setup pkt, 1 pkt, 64B max */
    OTG_FS_DOEPTSIZ(0) = DOEPTSIZ0_STUPCNT_1 | DOEPTSIZ0_PKTCNT | 64;
    OTG_FS_DOEPCTL(0) |= DEPCTL_EPENA | DEPCTL_CNAK;
}

static void ep0_send_status(void) {
    /* zero-length status packet */
    OTG_FS_DIEPTSIZ(0) = (1UL << 19);
    OTG_FS_DIEPCTL(0) |= DEPCTL_CNAK | DEPCTL_EPENA;
}

/* ---- Standard control request handling ---- */
static void handle_setup(const usb_setup_t *s) {
    uint8_t desc_type = s->wValue >> 8;
    uint8_t desc_idx   = s->wValue & 0xFF;

    if ((s->bmRequestType & 0x60) == 0x20) {
        /* Class request (HID) */
        if (s->bRequest == USB_REQ_SET_IDLE) { ep0_send_status(); return; }
        if (s->bRequest == USB_REQ_GET_DESCRIPTOR && desc_type == USB_DESC_TYPE_HID_REPORT) {
            uint16_t len = s->wLength < HID_REPORT_DESC_LEN ? s->wLength : HID_REPORT_DESC_LEN;
            ep0_transmit(hid_report_descriptor, len);
            return;
        }
        ep0_send_status();
        return;
    }

    switch (s->bRequest) {
    case USB_REQ_GET_DESCRIPTOR:
        switch (desc_type) {
        case USB_DESC_TYPE_DEVICE: {
            uint16_t len = s->wLength < 18 ? s->wLength : 18;
            ep0_transmit(usb_device_descriptor, len);
            break;
        }
        case USB_DESC_TYPE_CONFIGURATION: {
            uint16_t len = s->wLength < 34 ? s->wLength : 34;
            ep0_transmit(usb_config_descriptor, len);
            break;
        }
        case USB_DESC_TYPE_STRING: {
            const uint8_t *str; uint16_t sl;
            if (desc_idx == 0)       { str = usb_str_lang;    sl = usb_str_lang[0]; }
            else if (desc_idx == 1)  { str = usb_str_manuf;   sl = usb_str_manuf[0]; }
            else                     { str = usb_str_product; sl = usb_str_product[0]; }
            uint16_t len = s->wLength < sl ? s->wLength : sl;
            ep0_transmit(str, len);
            break;
        }
        default:
            OTG_FS_DIEPCTL(0) |= DEPCTL_EPDIS | DEPCTL_SNAK; /* STALL unknown */
            break;
        }
        break;

    case USB_REQ_SET_ADDRESS:
        dev_addr_pending = (uint8_t)(s->wValue & 0x7F);
        ep0_send_status(); /* apply address after the status stage completes, see IRQ handler */
        break;

    case USB_REQ_SET_CONFIGURATION:
        dev_configured = 1;
        /* Bring up EP1 IN (interrupt, 8 bytes) now that we're configured */
        OTG_FS_DIEPCTL(1) = DEPCTL_USBAEP | DEPCTL_EPTYP_INTR | DEPCTL_SD0PID
                           | (1UL << 22) /* TxFNum = 1 */ | 8 /* MPSIZ */;
        OTG_FS_DAINTMSK |= DAINT_IEPINT1;
        ep0_send_status();
        break;

    case USB_REQ_GET_CONFIGURATION: {
        uint8_t v = dev_configured;
        ep0_transmit(&v, 1);
        break;
    }

    case USB_REQ_GET_STATUS: {
        static const uint8_t zero[2] = {0, 0};
        ep0_transmit(zero, 2);
        break;
    }

    default:
        ep0_send_status();
        break;
    }
}

/* ---- IRQ handler ---- */
void OTG_FS_IRQHandler(void) {
    uint32_t gintsts = OTG_FS_GINTSTS;

    if (gintsts & GINT_USBRST) {
        /* Bus reset: flush FIFOs, reset EP0 control state, clear address */
        OTG_FS_GRSTCTL = GRSTCTL_TXFFLSH | GRSTCTL_TXFNUM_ALL;
        while (OTG_FS_GRSTCTL & GRSTCTL_TXFFLSH) {}
        OTG_FS_DAINTMSK = DAINT_IEPINT0 | DAINT_OEPINT0;
        OTG_FS_DOEPMSK  = DOEPMSK_STUPM | DOEPMSK_XFRCM;
        OTG_FS_DIEPMSK  = DIEPMSK_XFRCM;
        dev_configured = 0;
        OTG_FS_GINTSTS = GINT_USBRST;
    }

    if (gintsts & GINT_ENUMDNE) {
        /* Enumeration (speed negotiation) done -> set EP0 max packet + turnaround */
        OTG_FS_DIEPCTL(0) = (OTG_FS_DIEPCTL(0) & ~0x3UL) | 0x0 /* 64 bytes code */;
        OTG_FS_GUSBCFG = (OTG_FS_GUSBCFG & ~(0xFUL << GUSBCFG_TRDT_SHIFT)) | (0x6UL << GUSBCFG_TRDT_SHIFT);
        ep0_out_prepare();
        OTG_FS_GINTSTS = GINT_ENUMDNE;
    }

    if (gintsts & GINT_RXFLVL) {
        OTG_FS_GINTMSK &= ~GINT_RXFLVL;
        uint32_t status = OTG_FS_GRXSTSP;
        uint8_t pktsts = (status >> 17) & 0xF;
        uint16_t bcnt  = (status >> 4) & 0x7FF;

        if (pktsts == 0x6) { /* SETUP packet received */
            usb_setup_t setup;
            fifo_read((uint8_t *)&setup, 8);
            handle_setup(&setup);
        } else if (pktsts == 0x2 && bcnt) { /* OUT data (unused for keyboard, but drain it) */
            uint8_t scratch[64];
            fifo_read(scratch, bcnt);
        }
        /* pktsts == 0x4 (SETUP complete) / 0x3 (OUT complete) need no FIFO action */
        OTG_FS_GINTMSK |= GINT_RXFLVL;
    }

    if (gintsts & GINT_IEPINT) {
        if (OTG_FS_DIEPINT(0) & DIEPMSK_XFRCM) {
            OTG_FS_DIEPINT(0) = DIEPMSK_XFRCM;
            if (dev_addr_pending) {
                /* IN status stage for SET_ADDRESS finished: now it's safe to apply the address */
                OTG_FS_DCFG = (OTG_FS_DCFG & ~(0x7FUL << 4)) | ((uint32_t)dev_addr_pending << 4);
                dev_addr_pending = 0;
            }
            ep0_out_prepare(); /* ready for the next control transfer */
        }
        if (OTG_FS_DIEPINT(1) & DIEPMSK_XFRCM) {
            OTG_FS_DIEPINT(1) = DIEPMSK_XFRCM;
        }
    }
}

/* ---- Public API ---- */
void usb_init(void) {
    RCC_AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA11 = DM, PA12 = DP, alternate function 10 (OTG_FS) */
    GPIOA_MODER  = (GPIOA_MODER & ~((3UL<<22)|(3UL<<24))) | (2UL<<22) | (2UL<<24);
    GPIOA_OSPEEDR |= (3UL<<22) | (3UL<<24);          /* very high speed */
    GPIOA_AFRH   = (GPIOA_AFRH & ~((0xFUL<<12)|(0xFUL<<16))) | (10UL<<12) | (10UL<<16);

    /* Core soft reset */
    while (!(OTG_FS_GRSTCTL & GRSTCTL_AHBIDL)) {}
    OTG_FS_GRSTCTL |= GRSTCTL_CSRST;
    while (OTG_FS_GRSTCTL & GRSTCTL_CSRST) {}

    /* Select the internal FS PHY, force device mode */
    OTG_FS_GUSBCFG |= GUSBCFG_PHYSEL;
    OTG_FS_GCCFG = GCCFG_PWRDWN | GCCFG_NOVBUSSENS; /* power up transceiver, ignore VBUS pin */
    OTG_FS_GUSBCFG |= GUSBCFG_FDMOD;

    /* Device mode, full speed */
    OTG_FS_DCFG |= DCFG_DSPD_FS;

    /* RX FIFO: 128 words is comfortable for control + interrupt traffic */
    OTG_FS_GRXFSIZ = 128;
    /* EP0 TX FIFO (non-periodic): 64 words, starts right after RX FIFO */
    OTG_FS_GNPTXFSIZ = (64UL << 16) | 128;
    /* EP1 TX FIFO (periodic, HID IN): 64 words, starts after EP0's FIFO */
    OTG_FS_DIEPTXF1 = (64UL << 16) | (128 + 64);

    OTG_FS_GRSTCTL = GRSTCTL_TXFFLSH | GRSTCTL_TXFNUM_ALL;
    while (OTG_FS_GRSTCTL & GRSTCTL_TXFFLSH) {}

    OTG_FS_GAHBCFG |= GAHBCFG_GINTMSK; /* global USB interrupt enable */
    OTG_FS_GINTMSK = GINT_USBRST | GINT_ENUMDNE | GINT_RXFLVL | GINT_IEPINT | GINT_OEPINT;

    /* Soft-connect: pull D+ up so the host sees us */
    OTG_FS_DCTL &= ~(1UL << 1);
}

void hid_send_report(const uint8_t report[8]) {
    if (!dev_configured) return;
    /* Wait for EP1 to be idle (not already transmitting a previous report) */
    while (OTG_FS_DIEPCTL(1) & DEPCTL_EPENA) {}
    OTG_FS_DIEPTSIZ(1) = (1UL << 19) | 8;
    OTG_FS_DIEPCTL(1) |= DEPCTL_CNAK | DEPCTL_EPENA;
    fifo_write(1, report, 8);
}
