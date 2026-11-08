# Minimal flex consumer Makefile.
# git submodule add https://github.com/antonykasera/STM32F1-HAL.git lib/flex

TARGET  ?= firmware
BUILD   ?= build
FLEX_DIR ?= lib/flex

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# Only the flex modules this project uses. See flex.mk for the dependencies.
FLEX_MODULES ?= gpio clock

# Pull in flex's exported variables
include $(FLEX_DIR)/flex.mk

# Your application sources.
APP_SRCS := main.c

# Compose flags. FLEX_CFLAGS carries the MCU flags, -DSTM32F103xB, and includes.
CFLAGS  = $(FLEX_CFLAGS) \
          -Wall -Wextra -Os -flto -g3 -std=c11 \
          -ffunction-sections -fdata-sections

LDFLAGS = $(FLEX_MCU_FLAGS) \
          -Tlinker.ld -flto \
          -Wl,--gc-sections \
          --specs=nano.specs --specs=nosys.specs \
          -Wl,-Map=$(BUILD)/$(TARGET).map

# flex library sources + app + CMSIS startup.
CSRCS := $(FLEX_SRCS) $(APP_SRCS)
OBJS  := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(CSRCS)))
OBJS  += $(BUILD)/startup.o

# let make find flex sources by basename
VPATH := $(sort $(dir $(CSRCS)))

all: $(BUILD)/$(TARGET).bin size

# Rebuild objects when a header changes. Must stay below `all`, which has to
# remain the default goal.
-include $(OBJS:.o=.d)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/startup.o: $(FLEX_STARTUP) | $(BUILD)
	$(CC) $(FLEX_MCU_FLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

# --reset: without it the old program's peripherals keep running
flash: $(BUILD)/$(TARGET).bin
	st-flash --reset write $< 0x8000000

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all size flash clean
