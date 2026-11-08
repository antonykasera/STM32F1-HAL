# flex.mk — include fragment for consuming flex as a library.
#
# Add flex to your project (as a git submodule under lib/flex) and, in your
# own Makefile:
#
#     FLEX_DIR := lib/flex
#     FLEX_MODULES := gpio clock uart  # optional, defaults to every module
#     include $(FLEX_DIR)/flex.mk
#
#     CFLAGS  += $(FLEX_CFLAGS)      # MCU flags + -DSTM32F103xB + flex/CMSIS includes
#     SRCS    += $(FLEX_SRCS)        # flex HAL + common C sources
#     ASM     += $(FLEX_STARTUP)     # CMSIS startup (optional, bring your own if you prefer)
#
# This fragment exports VARIABLES only so your Makefile keeps
# full control of objects, linking, optimisation level and the linker script.
#
# Your project must provide: main(), a linker script, and a SystemInit() function
# (the CMSIS startup calls `bl SystemInit` before main, an empty body is fine:
# `void SystemInit(void) {}`).

# Directory this fragment lives in.
FLEX_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

FLEX_FAMILY ?= f1

# --- includes ---
FLEX_INC := -I$(FLEX_ROOT)/inc -I$(FLEX_ROOT)/cmsis/inc

# --- target flags flex is built/tested against ---
FLEX_MCU_FLAGS := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
FLEX_DEFS      := -DSTM32F103xB

FLEX_CFLAGS := $(FLEX_MCU_FLAGS) $(FLEX_DEFS) $(FLEX_INC)

# --- library sources  ---
# IRQ handlers override the startup's weak ones, so --gc-sections can't drop an
# unused module: list only what you use. Deps (missing one = link error):
#   timer, uart, can, adc, spi, rtc, pwr -> clock   iwdg, wwdg -> reset, clock
#   pwm, encoder, capture -> timer, clock   tim1 -> pwm, timer, clock
#   i2c, usb -> gpio, clock   flash, crc, reset -> nothing
#   uart, adc, spi -> dma too, only if their *_dma / scan functions are used
# usb is left out of the default: it and can share an IRQ handler (and SRAM),
# so linking both is a duplicate-symbol error
FLEX_MODULES ?= gpio clock exti timer pwm uart i2c can iwdg adc dma spi encoder capture tim1 rtc pwr flash crc wwdg reset

FLEX_SRCS := $(wildcard $(foreach m,$(FLEX_MODULES),$(FLEX_ROOT)/src/$(FLEX_FAMILY)/hal_$(m)_*.c)) \
             $(wildcard $(FLEX_ROOT)/src/common/*.c)

# --- CMSIS startup (optional, consumers may supply their own) ---
FLEX_STARTUP := $(FLEX_ROOT)/cmsis/startup/startup_stm32f103xb.s
