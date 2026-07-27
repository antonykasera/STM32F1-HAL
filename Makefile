TARGET  = firmware
BUILD   = build

FAMILY ?= f1

EXAMPLE ?= blink

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

MCU_FLAGS = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFS = -DSTM32F103xB
INCLUDES = -Iinc -Icmsis/inc

CFLAGS  = $(MCU_FLAGS) $(DEFS) $(INCLUDES) \
          -Wall -Wextra -Wshadow \
          -O0 -g3 -std=c11 \
          -ffunction-sections -fdata-sections

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

clean:
	rm -rf $(BUILD)

# Requires 'bear' 
compdb: clean
	bear -- $(MAKE) all

HOST_CC     = gcc
HOST_BUILD  = build/host
HOST_CFLAGS = -DHAL_TEST_HOST -Iinc -Icmsis/inc -Wall -Wextra -g -std=c11 -fsanitize=address

TEST_SRCS = $(wildcard test/host/*.c) \
            $(wildcard src/$(FAMILY)/*.c) \
            $(wildcard src/common/*.c)

host-test:
	@mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SRCS) -o $(HOST_BUILD)/test_runner
	./$(HOST_BUILD)/test_runner
	@echo "--- Host tests passed ---"

.PHONY: all size flash clean compdb host-test
