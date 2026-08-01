# flex Hardware Abstraction Layer for STM32

A minimal, zero-cost-abstraction Hardware Abstraction Layer for STM32 microcontrollers.

## Supported Hardware

| Family | Chip          | Status  |
|--------|---------------|---------|
| F1     | STM32F103C8T6 | Active  |

## Project Structure

```
flex/
├── inc/
│   ├── hal_types.h
│   ├── hal_gpio.h
│   ├── hal_exti.h
│   ├── hal_timer.h
│   ├── hal_uart.h
│   ├── hal_clock.h
│   └── board.h
├── src/
│   ├── f1/
│   │   ├── hal_gpio_f1.c
│   │   ├── hal_exti_f1.c
│   │   ├── hal_clock_f1.c
│   │   ├── hal_timer_f1.c
│   │   ├── hal_timer_calc.c
│   │   ├── hal_uart_f1.c
│   │   └── hal_uart_calc.c
│   └── common/
│       └── hal_error.c
├── cmsis/
│   ├── inc/
│   └── startup/
├── examples/
│   ├── blink/
│   ├── button_interrupt/
│   ├── basic_timer/
│   └── uart_hello_world/
├── test/
│   └── host/
│       ├── test_assert.h
│       ├── test_main.c
│       ├── test_gpio.c
│       ├── test_clock.c
│       ├── test_timer.c
│       └── test_uart.c
├── Makefile
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
