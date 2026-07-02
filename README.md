# Bare-Metal USB HID Keyboard — STM32F411 Black Pill

[![build](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml/badge.svg)](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

No HAL. No CMSIS device layer. Every peripheral touched here is a raw
`volatile uint32_t*` write, defined in `inc/regs.h`. Press the onboard
user button (PA0) and the board types the letter `a`.

## Quick start

```bash
git clone https://github.com/YOUR_USERNAME/YOUR_REPO.git
cd YOUR_REPO
sudo apt install gcc-arm-none-eabi stlink-tools   # or dfu-util, see below
make
make flash        # via ST-Link, or see "Flashing" below for DFU
```

### Flashing without an ST-Link (built-in DFU bootloader)

1. Hold **BOOT0**, tap **RESET**, release **BOOT0** — the chip boots its ROM bootloader.
2. `dfu-util -l` should show `Found DFU: [0483:df11]`.
3. `dfu-util -a 0 -s 0x08000000:leave -D usb_hid.bin`

The board should re-enumerate as **"BlackPill KB"**; pressing the onboard
user button types `a`.

## Layout

```
inc/regs.h        RCC / GPIO / USB OTG_FS register map + bit definitions
inc/usb_desc.h     USB device/config/string descriptors + HID report descriptor
src/startup.c      Vector table, Reset_Handler, .data/.bss init
src/clock.c        HSE(25MHz) -> PLL -> 96MHz SYSCLK / 48MHz USB clock
src/usb.c          OTG_FS core init, enumeration state machine, HID IN
src/main.c         Button poll -> hid_send_report()
stm32f411.ld       Linker script (512K flash / 128K RAM part)
Makefile           arm-none-eabi-gcc build
```

Builds clean with `make` (verified with `arm-none-eabi-gcc 13.2`), produces
`usb_hid.elf` / `usb_hid.bin`. Flash with `make flash` (needs `st-flash` /
st-link) or drag `usb_hid.bin` onto the DFU bootloader with `dfu-util`.

## The bring-up sequence, register by register

**1. Clock (`clock.c`)**
The Black Pill has a 25 MHz HSE crystal. USB OTG_FS needs *exactly* 48 MHz
on its clock input — this is non-negotiable, unlike SYSCLK which has some
slack. The PLL math:

```
VCO_in  = HSE / M   = 25MHz / 25 = 1MHz      (RM0383 wants 1-2MHz here)
VCO_out = VCO_in * N = 1MHz * 384 = 384MHz
SYSCLK  = VCO_out / P = 384MHz / 4 = 96MHz
USB48   = VCO_out / Q = 384MHz / 8 = 48MHz   <- OTG_FS clock
```
`RCC_PLLCFGR` packs M/N/P/Q into bitfields; `RCC_CFGR` then switches SYSCLK
onto the PLL output. Flash latency is bumped to 3 wait states first (as
required above 90MHz at 3.3V per the reference manual's AN latency table),
or execution will fault the moment the switch happens.

**2. GPIO (`usb_init`)**
PA11 (D-) and PA12 (D+) are switched from GPIO to their alternate function
(AF10 = OTG_FS) in `GPIOA_MODER`/`GPIOA_AFRH`. The Black Pill routes USB
D+/D- straight to these pins — no external circuitry needed for FS mode
since STM32F411 has an internal USB FS transceiver.

**3. Core reset & mode (`usb_init`)**
- `GRSTCTL.CSRST` — soft reset the whole OTG core, then wait for it to clear.
- `GUSBCFG.PHYSEL` — select the internal FS PHY (there's no ULPI/HS PHY on
  this pin route).
- `GCCFG.PWRDWN` — power up the transceiver (without this, D+/D- just sit
  there electrically dead).
- `GCCFG.NOVBUSSENS` — the Black Pill doesn't wire VBUS sensing to PA9, so
  we tell the core to stop waiting for a VBUS-valid signal it'll never see.
- `GUSBCFG.FDMOD` — force device mode (OTG_FS can be host or device; we
  hard-pin it to device since there's no ID pin logic here).
- `DCFG.DSPD = 0b11` — device speed = full speed, internal PHY.

**4. FIFO partitioning**
The OTG core has one shared block of packet-buffer RAM that software must
carve up by hand, in 32-bit words:
- `GRXFSIZ` — shared RX FIFO (used by every OUT endpoint and SETUP packets).
- `GNPTXFSIZ` — EP0's non-periodic TX FIFO, positioned right after RX.
- `DIEPTXF1` — EP1's *periodic* TX FIFO (interrupt endpoints are periodic),
  positioned after EP0's.

Get these offsets wrong and either enumeration silently fails or writes
corrupt an adjacent endpoint's buffer — there's no bounds checking.

**5. Interrupts**
`GINTMSK` is unmasked for `USBRST`, `ENUMDNE`, `RXFLVL`, `IEPINT`, `OEPINT`.
The whole rest of the driver lives in `OTG_FS_IRQHandler`:

- **USBRST** (host asserted a bus reset): flush TX FIFOs, reset `DAINTMSK`/
  `DOEPMSK`/`DIEPMSK` to a known state, drop configured status.
- **ENUMDNE** (speed negotiation finished): set EP0's max packet size and
  the PHY turnaround time (`GUSBCFG.TRDT`), then arm EP0 OUT for the first
  SETUP packet.
- **RXFLVL**: every SETUP or OUT packet lands in the *shared* RX FIFO; this
  IRQ tells you a word is ready. We pop the status word off `GRXSTSP`,
  check `PKTSTS` to see if it's a SETUP packet, and if so read 8 bytes out
  of `USB_FIFO(0)` as a `usb_setup_t`.
- **IEPINT**: fires when a TX transfer completes. For EP0 this is used to
  apply `SET_ADDRESS` at the right moment (the address must not change
  until *after* the status-stage IN packet for that same request has gone
  out — do it early and the host's ACK never reaches you).

**6. Enumeration (`handle_setup`)**
Standard control requests are decoded by hand: `GET_DESCRIPTOR` walks a
switch on `wValue`'s high byte to serve the device, configuration, string,
or (via a class request) HID report descriptors defined in `usb_desc.h`.
`SET_CONFIGURATION` is the point where EP1 (the actual keyboard endpoint)
gets enabled via `DIEPCTL(1)`.

**7. Sending a report (`hid_send_report`)**
An 8-byte boot-protocol keyboard report — `[modifiers, reserved, key1..key6]`
— is written straight into EP1's FIFO window (`USB_FIFO(1)`) after arming
`DIEPTSIZ(1)`/`DIEPCTL(1)`. The host's driver picks it up on its next
1kHz/100Hz interrupt poll (we advertise a 10 ms interval).

## Extending this

- **Gamepad instead of keyboard**: swap `hid_report_descriptor` in
  `usb_desc.h` for a joystick/gamepad usage page (buttons + X/Y axes),
  change `bInterfaceSubClass`/`bInterfaceProtocol` to non-boot, and build
  reports matching the new layout in `main.c`.
- **N-key rollover**: the boot-protocol 6-key array is the USB baseline;
  a "real" NKRO keyboard uses a bitmap report instead and typically exposes
  a second boot-protocol interface for BIOS compatibility.
- **Multiple endpoints**: repeat the `DIEPTXFx`/`DIEPCTL(x)` pattern per
  extra IN endpoint (e.g. a separate consumer-control endpoint for media
  keys), remembering each one needs its own FIFO slice carved out of
  `GNPTXFSIZ`'s remaining space.

## Caveats to know before relying on this in a product

- No USB suspend/remote-wakeup handling — add `GINT_USBSUSP` handling if
  you care about host-initiated suspend.
- No error/NAK recovery beyond the happy path — fine for a hobby HID
  device, not fine for anything safety- or compliance-relevant.
- `idVendor`/`idProduct` here reuse ST's own VID with an arbitrary PID,
  which is fine for personal/hobby use but not for anything you'd
  distribute — get your own VID/PID (e.g. via pid.codes) for that.
