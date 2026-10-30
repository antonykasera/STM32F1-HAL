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
│   ├── hal_types.h
│   ├── hal_error.h
│   ├── hal_gpio.h
│   ├── hal_exti.h
│   ├── hal_timer.h
│   ├── hal_pwm.h
│   ├── hal_uart.h
│   ├── hal_i2c.h
│   ├── hal_clock.h
│   └── board.h
├── src/
│   ├── f1/
│   │   ├── hal_gpio_f1.c
│   │   ├── hal_exti_f1.c
│   │   ├── hal_clock_f1.c
│   │   ├── hal_timer_f1.c
│   │   ├── hal_timer_calc.c
│   │   ├── hal_pwm_f1.c
│   │   ├── hal_uart_f1.c
│   │   ├── hal_uart_calc.c
│   │   ├── hal_i2c_f1.c
│   │   └── hal_i2c_calc.c
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
│   └── i2c_scan/
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

# Build a specific example
make EXAMPLE=button_interrupt

# Flash via ST-Link
make flash

# Flash via USB DFU (STM32duino/Maple bootloader)
make dfu

# Clean
make clean
```

## Debugging

```bash
# Build with debug symbols (-Og, no LTO)
make debug

# Start an on-chip GDB session over SWD (openocd + arm-none-eabi-gdb)
make gdb
```

## Using flex in Your Project

Add flex as a submodule and pull in `flex.mk`, which exports the include paths
and source lists. Your project includes `main()`, the linker script and
`SystemInit()`.

```bash
git submodule add https://github.com/<you>/flex.git lib/flex
```

```makefile
FLEX_DIR := lib/flex
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
