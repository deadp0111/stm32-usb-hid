TARGET  = usb_hid
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU     = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CFLAGS  = $(CPU) -Iinc -O2 -Wall -Wextra -ffreestanding -fno-common \
          -ffunction-sections -fdata-sections -std=c11
LDFLAGS = $(CPU) -Tstm32f411.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(TARGET).map

SRCS = src/startup.c src/clock.c src/usb.c src/main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET).elf $(TARGET).bin
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

flash: $(TARGET).bin
	st-flash write $(TARGET).bin 0x08000000

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).map

.PHONY: all flash clean
