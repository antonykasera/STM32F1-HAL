# Exports variables only: FLEX_CFLAGS, FLEX_SRCS, FLEX_STARTUP, FLEX_MCU_FLAGS.
# Your project provides main(), a linker script and SystemInit() (the startup
# calls it before main; an empty body is fine). See template/ and the README.

FLEX_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

FLEX_FAMILY ?= f1

FLEX_INC := -I$(FLEX_ROOT)/inc -I$(FLEX_ROOT)/cmsis/inc

FLEX_MCU_FLAGS := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
FLEX_DEFS      := -DSTM32F103xB

FLEX_CFLAGS := $(FLEX_MCU_FLAGS) $(FLEX_DEFS) $(FLEX_INC)

# IRQ handlers override the startup's weak ones, so --gc-sections can't drop an
# unused module: list only what you use. Deps (missing one = link error):
#   timer, uart, can, adc, spi, rtc, pwr -> clock   iwdg, wwdg -> reset, clock
#   pwm, encoder, capture -> timer, clock   tim1 -> pwm, timer, clock
#   i2c, usb -> gpio, clock   flash, crc, reset -> nothing
#   uart, adc, spi -> dma too, only if their *_dma / scan functions are used
# usb isn't in the default: it shares an IRQ handler and SRAM with can
FLEX_MODULES ?= gpio clock exti timer pwm uart i2c can iwdg adc dma spi encoder capture tim1 rtc pwr flash crc wwdg reset

FLEX_SRCS := $(wildcard $(foreach m,$(FLEX_MODULES),$(FLEX_ROOT)/src/$(FLEX_FAMILY)/hal_$(m)_*.c)) \
             $(wildcard $(FLEX_ROOT)/src/common/*.c)

# optional: bring your own startup instead
FLEX_STARTUP := $(FLEX_ROOT)/cmsis/startup/startup_stm32f103xb.s
