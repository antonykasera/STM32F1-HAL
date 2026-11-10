# Copy this folder, add flex as a submodule in lib/flex; `make help` for targets

TARGET   ?= firmware
FLEX_DIR ?= lib/flex

APP_DIRS ?= .
APP_INC  ?= .

# dependencies: see flex.mk
FLEX_MODULES ?= gpio clock

# optional flexlib: utils and/or math
FLEXLIB_DIR  ?= lib/flexlib
FLEXLIB_LIBS ?=

# DEBUG=1: -Og, no LTO
DEBUG ?= 0
BUILD_ROOT ?= build
BUILD := $(BUILD_ROOT)/$(if $(filter 1,$(DEBUG)),debug,release)

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
NM      = arm-none-eabi-nm
SIZE    = arm-none-eabi-size
GDB     = arm-none-eabi-gdb
OPENOCD_CFG = -f interface/stlink.cfg -f target/stm32f1x.cfg
PORT ?= /dev/ttyACM0
BAUD ?= 115200

include $(FLEX_DIR)/flex.mk
ifneq ($(strip $(FLEXLIB_LIBS)),)
include $(foreach l,$(FLEXLIB_LIBS),$(FLEXLIB_DIR)/flex$(l).mk)
endif

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
LIB_ROOTS := $(FLEX_ROOT) $(FLEXLIB_DIR) $(FLEXLIB_ROOT) $(BUILD_ROOT)
APP_SRCS  := $(filter-out $(foreach r,$(LIB_ROOTS),$(r)/%),\
               $(patsubst ./%,%,$(call rwildcard,$(APP_DIRS),*.c)))
CSRCS     := $(FLEX_SRCS) $(sort $(FU_SRCS) $(FM_SRCS)) $(APP_SRCS)

WARN = -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
ifeq ($(DEBUG),1)
OPT = -Og
LTO =
else
OPT = -Os
LTO = -flto
endif

CFLAGS  = $(FLEX_CFLAGS) $(FLEXLIB_CFLAGS) $(addprefix -I,$(APP_INC)) \
          $(WARN) $(OPT) $(LTO) -g3 -std=c11 \
          -ffunction-sections -fdata-sections -fstack-usage $(EXTRA_CFLAGS)
LDFLAGS = $(FLEX_MCU_FLAGS) -Tlinker.ld $(LTO) \
          -Wl,--gc-sections -Wl,--print-memory-usage \
          --specs=nano.specs --specs=nosys.specs \
          -Wl,-Map=$(BUILD)/$(TARGET).map

# objects keep their folders; '../' becomes '_up/' to stay under $(BUILD)
obj  = $(BUILD)/$(subst ../,_up/,$(1:.c=.o))
OBJS := $(foreach s,$(CSRCS),$(call obj,$s)) $(BUILD)/startup.o
ELF  := $(BUILD)/$(TARGET).elf

all: $(BUILD)/$(TARGET).bin

# after `all`, the default goal
-include $(OBJS:.o=.d)

$(foreach s,$(CSRCS),$(eval $(call obj,$s): $s ; @mkdir -p $$(@D) && echo "CC $$<" && $$(CC) $$(CFLAGS) -MMD -MP -c $$< -o $$@))

$(BUILD)/startup.o: $(FLEX_STARTUP)
	@mkdir -p $(@D)
	$(CC) $(FLEX_MCU_FLAGS) -c $< -o $@

$(ELF): $(OBJS) Makefile linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(BUILD)/$(TARGET).bin: $(ELF)
	$(OBJCOPY) -O binary $< $@

# --reset: or the old program's peripherals keep running
flash: $(BUILD)/$(TARGET).bin
	st-flash --reset write $< 0x8000000

flash-ocd: $(ELF)
	openocd $(OPENOCD_CFG) -c "program $< verify reset exit"

gdbserver:
	openocd $(OPENOCD_CFG)

gdb: $(ELF)
	@openocd $(OPENOCD_CFG) >$(BUILD)/openocd.log 2>&1 & OCD=$$!; sleep 1; \
	$(GDB) -nx -q $< -ex "target extended-remote :3333" -ex "monitor reset halt" \
	  -ex load -ex "monitor reset halt" -ex "break main" -ex continue; \
	kill $$OCD 2>/dev/null; true

term:
	picocom -b $(BAUD) --imap lfcrlf $(PORT)

size: $(ELF)
	$(SIZE) $<

syms: $(ELF)
	$(NM) --print-size --size-sort --radix=d $< | tail -25

disasm: $(ELF)
	$(OBJDUMP) -d -S $< > $(BUILD)/$(TARGET).lst
	@echo "wrote $(BUILD)/$(TARGET).lst"

# DEBUG=1 gives per-function frames (LTO merges them)
stack: $(ELF)
	@find $(BUILD) -name '*.su' -exec cat {} + | sort -t"$$(printf '\t')" -k2 -nr | head -20

# for clangd, without bear
compdb:
	@{ echo '['; sep=''; for f in $(CSRCS); do \
	  printf '%s{"directory":"%s","file":"%s","command":"%s %s -c %s"}\n' \
	    "$$sep" "$(CURDIR)" "$$f" "$(CC)" "$(CFLAGS)" "$$f"; sep=','; \
	done; echo ']'; } > compile_commands.json
	@echo "wrote compile_commands.json ($(words $(CSRCS)) files)"

sources:
	@printf '%s\n' $(CSRCS)

clean:
	rm -rf $(BUILD_ROOT)

help:
	@echo "make [DEBUG=1]     build $(BUILD)/$(TARGET).bin (prints flash/RAM use)"
	@echo "make flash         flash with st-flash"
	@echo "make flash-ocd     flash and verify with openocd"
	@echo "make gdb           openocd + gdb, stopped at main"
	@echo "make term          serial monitor on PORT=$(PORT)"
	@echo "make size | syms | disasm | stack   inspect the image"
	@echo "make compdb        compile_commands.json for clangd"
	@echo "make sources       list every source that gets built"
	@echo "make clean"

.PHONY: all flash flash-ocd gdbserver gdb term size syms disasm stack compdb sources clean help
