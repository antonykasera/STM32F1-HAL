TARGET  = firmware
BUILD   = build

FAMILY ?= f1

EXAMPLE ?= blink

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
NM      = arm-none-eabi-nm
SIZE    = arm-none-eabi-size

MCU_FLAGS = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFS = -DSTM32F103xB
INCLUDES = -Iinc -Icmsis/inc

CFLAGS  = $(MCU_FLAGS) $(DEFS) $(INCLUDES) \
          -Wall -Wextra -Wshadow \
          -Wconversion -Wsign-conversion -Wdouble-promotion \
          -Os -g3 -std=c11 \
          -ffunction-sections -fdata-sections

ASFLAGS = $(MCU_FLAGS)

LDFLAGS = $(MCU_FLAGS) \
          -Tlinker.ld \
          -Wl,--gc-sections \
          --specs=nano.specs --specs=nosys.specs \
          -Wl,-Map=$(BUILD)/$(TARGET).map

# link-time optimisation (release builds):
CFLAGS  += -flto
LDFLAGS += -flto

HAL_SRCS  = $(wildcard src/$(FAMILY)/*.c)
CMN_SRCS  = $(wildcard src/common/*.c)
APP_SRCS  = $(wildcard examples/$(EXAMPLE)/*.c)

SRCS = $(HAL_SRCS) $(CMN_SRCS) $(APP_SRCS)

ASM = cmsis/startup/startup_stm32f103xb.s

OBJS  = $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
OBJS += $(BUILD)/startup_stm32f103xb.o

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex size

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup_stm32f103xb.o: $(ASM) | $(BUILD)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

$(BUILD):
	mkdir -p $(BUILD)

flash: $(BUILD)/$(TARGET).bin
	st-flash write $(BUILD)/$(TARGET).bin 0x8000000

# Flash via USB DFU (STM32duino/Maple bootloader: alt-setting 2, VID:PID 1EAF:0003)
dfu: $(BUILD)/$(TARGET).bin
	sudo dfu-util -w -a 2 -d 1EAF:0003 -D $(BUILD)/$(TARGET).bin

clean:
	rm -rf $(BUILD)


sections: $(BUILD)/$(TARGET).elf
	$(SIZE) -A -d $<

syms: $(BUILD)/$(TARGET).elf
	$(NM) --print-size --size-sort --radix=d $< | tail -25

disasm: $(BUILD)/$(TARGET).elf
	$(OBJDUMP) -d -S $< > $(BUILD)/$(TARGET).lst
	@echo "wrote $(BUILD)/$(TARGET).lst"

stackusage:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(MCU_FLAGS) $(DEFS) $(INCLUDES) -O1 -g3 -std=c11 -ffunction-sections -fdata-sections -fstack-usage" LDFLAGS="$(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs" all
	@echo "--- stack frames (bytes, largest first) ---"
	@find $(BUILD) -name '*.su' -exec cat {} + | sort -t$$'\t' -k2 -nr | head -25

# Requires 'bear'
compdb: clean
	bear -- $(MAKE) all

HOST_CC     = gcc
HOST_BUILD  = build/host
HOST_CFLAGS = -DHAL_TEST_HOST -Iinc -Icmsis/inc -Wall -Wextra -g -std=c11 -fsanitize=address

TEST_SRCS = $(wildcard test/host/*.c) \
            src/f1/hal_gpio_f1.c \
            src/f1/hal_clock_f1.c \
            src/f1/hal_timer_calc.c \
            src/f1/hal_uart_calc.c \
            $(wildcard src/common/*.c)

host-test:
	@mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SRCS) -o $(HOST_BUILD)/test_runner
	./$(HOST_BUILD)/test_runner
	@echo "--- Host tests passed ---"

.PHONY: all size sections syms disasm stackusage flash dfu clean compdb host-test
