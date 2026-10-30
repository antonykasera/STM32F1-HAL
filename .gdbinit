# gdb init for SWD debugging via openocd + ST-Link.
# Used by `make gdb` (which starts openocd, then launches gdb with this file).
# Terminal debugging: breakpoints, stepping, register/peripheral inspection.
#
# Run `make debug EXAMPLE=<name>` first so build/<name>/firmware.elf has full -Og
# symbols, then `make gdb EXAMPLE=<name>` with the same name.

set print pretty on
set pagination off
set confirm off

# connect to the openocd gdb server (started by `make gdb`)
target extended-remote localhost:3333

# flash the freshly built image and reset+halt at the entry
monitor reset halt
load
monitor reset halt

# a couple of handy breakpoints to get started (edit freely)
break main

echo \n--- flex debug session ---\n
echo Program flashed and halted at main.\n
echo   c  = continue      n = next (step over)      s = step (into)\n
echo   b hal_gpio_pin_init = breakpoint on a function\n
echo   p/x *(uint32_t*)0x40000428 = read TIM3->PSC live\n
echo   info registers     = CPU regs      layout src = TUI source view\n\n
