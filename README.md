# flex Hardware Abstraction Layer for STM32

A minimal, zero-cost-abstraction Hardware Abstraction Layer for STM32 microcontrollers.

## Supported Hardware

| Family | Chip          | Status  |
|--------|---------------|---------|
| F1     | STM32F103C8T6 | Active  |

## Project Structure

```
flex/
├── inc/                    # Public HAL API headers
│   ├── hal_types.h         # Shared enums/structs
│   ├── hal_gpio.h          # GPIO API
│   ├── hal_exti.h          # EXTI / interrupt API
│   ├── hal_timer.h         # Timer (TIM2/3/4) API
│   ├── hal_uart.h          # UART (USART async) API
│   ├── hal_clock.h         # Clock configuration API
│   └── board.h             # Project pin definitions
├── src/
│   ├── f1/                 # STM32F1 family implementations
│   │   ├── hal_gpio_f1.c
│   │   ├── hal_exti_f1.c
│   │   ├── hal_clock_f1.c
│   │   ├── hal_timer_f1.c   # timer register driver
│   │   ├── hal_timer_calc.c # pure PSC/ARR math (host-testable)
│   │   ├── hal_uart_f1.c    # uart register driver
│   │   └── hal_uart_calc.c  # pure baud/BRR math (host-testable)
│   └── common/             # Family-independent code
│       └── hal_error.c
├── cmsis/                  # Vendor CMSIS headers & startup
│   ├── inc/
│   └── startup/
├── examples/
│   ├── blink/
│   ├── button_interrupt/
│   ├── basic_timer/
│   ├── uart_hello_world/
│   └── cycle_profile/      # DWT cycle-count profiler over UART
├── test/
│   └── host/               # Host-side unit tests
│       ├── test_assert.h   # assertion + runner macros
│       ├── test_main.c     # runner (counters, summary, main)
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
make sections     # per-section flash/RAM breakdown
make syms         # symbols ranked by size
make disasm       # source-interleaved disassembly -> build/firmware.lst
make stackusage   # per-function stack frame sizes (largest first)
```

## Version

v0.1.0

## License

MIT — see [LICENSE](LICENSE).
