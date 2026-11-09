TARGET  = firmware

FAMILY ?= f1

EXAMPLE ?= blink

BUILD_ROOT = build
BUILD      = $(BUILD_ROOT)/$(EXAMPLE)

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
          -O0 -g3 -std=c11 \
          -ffunction-sections -fdata-sections

# separate from CFLAGS, which debug/stackusage replace
DEPFLAGS = -MMD -MP
EXTRA_CFLAGS ?=

ASFLAGS = $(MCU_FLAGS)

LDFLAGS = $(MCU_FLAGS) \
          -Tlinker.ld \
          -Wl,--gc-sections \
          --specs=nano.specs --specs=nosys.specs \
          -Wl,-Map=$(BUILD)/$(TARGET).map

# per-example module list keeps unused IRQ handlers out of the image
-include examples/$(EXAMPLE)/modules.mk
FLEX_FAMILY := $(FAMILY)
include flex.mk

HAL_SRCS  = $(patsubst ./%,%,$(FLEX_SRCS))
APP_SRCS  = $(wildcard examples/$(EXAMPLE)/*.c)

SRCS = $(HAL_SRCS) $(APP_SRCS)

ASM = cmsis/startup/startup_stm32f103xb.s

OBJS  = $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
OBJS += $(BUILD)/startup_stm32f103xb.o

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex size

# after `all`, the default goal
-include $(OBJS:.o=.d)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(DEPFLAGS) -c $< -o $@

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

# --reset: or the old program's peripherals keep running
flash: $(BUILD)/$(TARGET).bin
	st-flash --reset write $(BUILD)/$(TARGET).bin 0x8000000

OPENOCD_CFG = -f interface/stlink.cfg -f target/stm32f1x.cfg

flash-ocd: $(BUILD)/$(TARGET).elf
	openocd $(OPENOCD_CFG) -c "program $< verify reset exit"

gdbserver:
	openocd $(OPENOCD_CFG)

# -nx: no auto-loaded .gdbinit, so -x runs it once
gdb: $(BUILD)/$(TARGET).elf
	@openocd $(OPENOCD_CFG) >/tmp/flex-openocd.log 2>&1 & \
	OCD_PID=$$!; \
	sleep 1; \
	arm-none-eabi-gdb -nx $(BUILD)/$(TARGET).elf -x .gdbinit; \
	kill $$OCD_PID 2>/dev/null; true

clean:
	rm -rf $(BUILD_ROOT)

TPL = $(BUILD_ROOT)/template-check
template-check:
	@rm -rf $(TPL) && mkdir -p $(TPL)/src $(TPL)/drivers
	@cp template/Makefile template/linker.ld template/main.c $(TPL)/
	@printf 'int util_a(void);\nint util_a(void) { return 1; }\n' > $(TPL)/src/util.c
	@printf 'int util_b(void);\nint util_b(void) { return 2; }\n' > $(TPL)/drivers/util.c
	$(MAKE) --no-print-directory -C $(TPL) FLEX_DIR=$(CURDIR) EXTRA_CFLAGS=-Werror
	$(MAKE) --no-print-directory -C $(TPL) FLEX_DIR=../.. BUILD_ROOT=build-rel EXTRA_CFLAGS=-Werror
	@test -f $(TPL)/build/release/src/util.o && test -f $(TPL)/build/release/drivers/util.o
	@echo "--- template ok ---"

EXAMPLES = $(notdir $(patsubst %/,%,$(wildcard examples/*/)))

examples: clean
	@set -e; for ex in $(EXAMPLES); do \
		echo "--- $$ex ---"; \
		$(MAKE) --no-print-directory EXAMPLE=$$ex EXTRA_CFLAGS=-Werror all; \
	done

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

debug: clean
	$(MAKE) CFLAGS="$(MCU_FLAGS) $(DEFS) $(INCLUDES) -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion -Og -g3 -std=c11 -ffunction-sections -fdata-sections" LDFLAGS="$(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs -Wl,-Map=$(BUILD)/$(TARGET).map" all

# needs bear
compdb: clean
	bear -- $(MAKE) all

HOST_CC     = gcc
HOST_BUILD  = $(BUILD_ROOT)/host
HOST_CFLAGS = -DHAL_TEST_HOST -Iinc -Icmsis/inc -Wall -Wextra -g -std=c11 -fsanitize=address

TEST_SRCS = $(wildcard test/host/*.c) \
            src/f1/hal_gpio_f1.c \
            src/f1/hal_clock_f1.c \
            src/f1/hal_timer_calc.c \
            src/f1/hal_pwm_calc.c \
            src/f1/hal_pwm_f1.c \
            src/f1/hal_uart_calc.c \
            src/f1/hal_uart_f1.c \
            src/f1/hal_i2c_calc.c \
            src/f1/hal_i2c_f1.c \
            src/f1/hal_can_calc.c \
            src/f1/hal_can_f1.c \
            src/f1/hal_iwdg_calc.c \
            src/f1/hal_iwdg_f1.c \
            src/f1/hal_adc_calc.c \
            src/f1/hal_adc_f1.c \
            src/f1/hal_dma_calc.c \
            src/f1/hal_dma_f1.c \
            src/f1/hal_spi_calc.c \
            src/f1/hal_spi_f1.c \
            src/f1/hal_encoder_f1.c \
            src/f1/hal_capture_f1.c \
            src/f1/hal_tim1_calc.c \
            src/f1/hal_tim1_f1.c \
            src/f1/hal_rtc_f1.c \
            src/f1/hal_crc_calc.c \
            src/f1/hal_crc_f1.c \
            src/f1/hal_flash_f1.c \
            src/f1/hal_wwdg_calc.c \
            src/f1/hal_wwdg_f1.c \
            src/f1/hal_reset_calc.c \
            src/f1/hal_usb_calc.c \
            src/f1/hal_usb_desc.c \
            $(wildcard src/common/*.c)

host-test:
	@mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SRCS) -o $(HOST_BUILD)/test_runner
	./$(HOST_BUILD)/test_runner
	@echo "--- Host tests passed ---"

.PHONY: template-check all size sections syms disasm stackusage flash flash-ocd gdbserver gdb clean examples debug compdb host-test
