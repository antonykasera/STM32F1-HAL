TARGET  = firmware

FAMILY ?= f1

EXAMPLE ?= blink

# Each example builds into its own directory, so one example's image can never
# be flashed by mistake for another's.
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

# header deps; separate from CFLAGS, which debug/stackusage override
DEPFLAGS = -MMD -MP
EXTRA_CFLAGS ?=

ASFLAGS = $(MCU_FLAGS)

LDFLAGS = $(MCU_FLAGS) \
          -Tlinker.ld \
          -Wl,--gc-sections \
          --specs=nano.specs --specs=nosys.specs \
          -Wl,-Map=$(BUILD)/$(TARGET).map

# link-time optimisation (release builds):
# CFLAGS  += -flto
# LDFLAGS += -flto

HAL_SRCS  = $(wildcard src/$(FAMILY)/*.c)
CMN_SRCS  = $(wildcard src/common/*.c)
APP_SRCS  = $(wildcard examples/$(EXAMPLE)/*.c)

SRCS = $(HAL_SRCS) $(CMN_SRCS) $(APP_SRCS)

ASM = cmsis/startup/startup_stm32f103xb.s

OBJS  = $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
OBJS += $(BUILD)/startup_stm32f103xb.o

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex size

# Must stay below `all`: the .d files define targets, and the first target in
# the Makefile becomes the default goal.
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

flash: $(BUILD)/$(TARGET).bin
	st-flash write $(BUILD)/$(TARGET).bin 0x8000000

OPENOCD_CFG = -f interface/stlink.cfg -f target/stm32f1x.cfg

flash-ocd: $(BUILD)/$(TARGET).elf
	openocd $(OPENOCD_CFG) -c "program $< verify reset exit"

gdbserver:
	openocd $(OPENOCD_CFG)

# -nx stops gdb auto-loading ./.gdbinit (when it is on the auto-load safe path),
# so -x runs it exactly once: one connect, one load, one break at main.
gdb: $(BUILD)/$(TARGET).elf
	@openocd $(OPENOCD_CFG) >/tmp/flex-openocd.log 2>&1 & \
	OCD_PID=$$!; \
	sleep 1; \
	arm-none-eabi-gdb -nx $(BUILD)/$(TARGET).elf -x .gdbinit; \
	kill $$OCD_PID 2>/dev/null; true

# Flash via USB DFU (STM32duino/Maple bootloader: alt-setting 2, VID:PID 1EAF:0003)
dfu: $(BUILD)/$(TARGET).bin
	sudo dfu-util -w -a 2 -d 1EAF:0003 -D $(BUILD)/$(TARGET).bin

clean:
	rm -rf $(BUILD_ROOT)

# Build every example from clean with warnings as errors, so an object left over
# from an earlier build can't hide a warning.
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

# Requires 'bear'
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
            src/f1/hal_can_calc.c \
            src/f1/hal_can_f1.c \
            $(wildcard src/common/*.c)

host-test:
	@mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SRCS) -o $(HOST_BUILD)/test_runner
	./$(HOST_BUILD)/test_runner
	@echo "--- Host tests passed ---"

.PHONY: all size sections syms disasm stackusage flash flash-ocd gdbserver gdb dfu clean examples debug compdb host-test
