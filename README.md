# flex Hardware Abstraction Layer for STM32

A minimal Hardware Abstraction Layer for STM32 microcontrollers.

## Supported Hardware

| Family | Chip          | Status  |
|--------|---------------|---------|
| F1     | STM32F103C8T6 | Active  |

## Project Structure

```
flex/
├── inc/
│   ├── hal_error.h
│   ├── hal_gpio.h
│   ├── hal_exti.h
│   ├── hal_timer.h
│   ├── hal_pwm.h
│   ├── hal_uart.h
│   ├── hal_i2c.h
│   ├── hal_can.h
│   └── hal_clock.h
├── src/
│   ├── f1/
│   │   ├── hal_gpio_f1.c
│   │   ├── hal_exti_f1.c
│   │   ├── hal_clock_f1.c
│   │   ├── hal_timer_f1.c
│   │   ├── hal_timer_calc.c
│   │   ├── hal_pwm_f1.c
│   │   ├── hal_pwm_calc.c
│   │   ├── hal_uart_f1.c
│   │   ├── hal_uart_calc.c
│   │   ├── hal_i2c_f1.c
│   │   ├── hal_i2c_calc.c
│   │   ├── hal_can_f1.c
│   │   └── hal_can_calc.c
│   └── common/
│       └── hal_error.c
├── cmsis/
│   ├── inc/
│   └── startup/
├── examples/
│   ├── blink/
│   ├── button_interrupt/
│   ├── basic_timer/
│   ├── pwm_led/
│   ├── uart_hello_world/
│   ├── uart_echo/
│   ├── cycle_profile/
│   ├── i2c_scan/
│   ├── i2c_as5600/
│   ├── i2c_as5600_it/
│   ├── can_loopback/
│   └── can_rx_irq/
├── test/
│   └── host/
│       ├── test_assert.h
│       ├── test_main.c
│       ├── test_gpio.c
│       ├── test_clock.c
│       ├── test_timer.c
│       ├── test_pwm.c
│       ├── test_uart.c
│       ├── test_i2c.c
│       ├── test_can.c
│       └── test_error.c
├── template/
├── Makefile
├── flex.mk
├── linker.ld
├── LICENSE
└── README.md
```

## Building

```bash
# Build the blink example for F1 family (default)
make

# Build a specific example (output goes to build/<example>/)
make EXAMPLE=button_interrupt

# Flash via ST-Link. EXAMPLE selects the image, and defaults to blink.
make flash EXAMPLE=button_interrupt

# Flash via USB DFU (STM32duino/Maple bootloader)
make dfu

# Build every example from clean, with warnings as errors
make examples

# Clean
make clean
```

## Debugging

```bash
# Build with debug symbols (-Og, no LTO)
make debug EXAMPLE=button_interrupt

# Start an on-chip GDB session over SWD (openocd + arm-none-eabi-gdb)
make gdb EXAMPLE=button_interrupt
```

## Using flex in Your Project

Add flex as a submodule and pull in `flex.mk`, which exports the include paths
and source lists. Your project includes `main()`, the linker script and
`SystemInit()`.

```bash
git submodule add https://github.com/antonykasera/STM32F1-HAL.git lib/flex
```

```makefile
FLEX_DIR := lib/flex
FLEX_MODULES := gpio clock uart  # optional, defaults to every module
include $(FLEX_DIR)/flex.mk

CFLAGS += $(FLEX_CFLAGS)     # MCU flags, -DSTM32F103xB, flex + CMSIS includes
SRCS   += $(FLEX_SRCS)       # flex HAL + common sources
ASM    += $(FLEX_STARTUP)    # CMSIS startup (or bring your own)
```

See [`template/`](template/) for a minimal working consumer.

## Host-Side Testing

```bash
# Run unit tests
make host-test
```

## Build Analysis

Static analysis of the built ELF

```bash
make sections  
make syms      
make disasm   
make stackusage 
```

## Version

v0.1.0

## License

MIT — see [LICENSE](LICENSE).
