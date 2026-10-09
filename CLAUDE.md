# CLAUDE.md - Project Context & Guidelines for Claude Code

## Project Overview
- **Project Name**: STM32F411RE Synthesizer & Hardware Sequencer
- **Target MCU**: STMicroelectronics STM32F411RET6 (ARM Cortex-M4F @ 16 MHz, Hardware FPU Enabled)
- **Toolchain**: GNU Arm Embedded Toolchain 13.3.rel1 (`arm-none-eabi-gcc`), GNU Make
- **Architecture**: 2-Tier Architecture: Application (`Src/app_synth.c`) & Driver BSP (`Src/bsp_*.c`)

## Build & Test Commands
To build the firmware in PowerShell:
```powershell
$env:PATH = "Z:\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin;Z:\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin;" + $env:PATH
cd Debug
make clean
make -j4 all
```
To run the MISRA-C 22 rules verification:
```powershell
py C:\Users\books\.gemini\antigravity\brain\e254b936-2c29-49a3-84b3-f1b8ce12efe6\scratch\verify_misra.py
```

## NEXTY 7 Evaluation Criteria (CRITICAL - DO NOT VIOLATE)
1. **UART (Zero Polling)**: Strictly interrupt/DMA driven. `USART2_IRQHandler` uses circular ring buffers for RX (`RXNE`) and TX (`TXE`). ZERO polling `while(!(USART2->SR ...))` permitted.
2. **ADC (Zero Polling)**: Strictly hardware timer triggered + DMA. ADC1 (PA4 Vol, PC0 VRx, PC1 VRy) triggered by TIM3 TRGO @ 1 kHz into DMA2 Stream 0 circular buffer. ZERO CPU polling.
3. **External Interrupt**: At least 1 EXTI interrupt. Currently configured: EXTI Line 2 on PC2 (Joystick SW), both edges, with `EXTI2_IRQHandler`. The joystick switch works only through EXTI: each edge sets a flag, `bsp_joystick_service()` restarts a 30 ms debounce window, then reads the settled level once (no continuous pin polling). Do not turn EXTI back into an unused flag.
4. **GPIO Configuration**: Clean Input Pull-Up (Keys, Joystick SW) and Output Push-Pull (4 LEDs: PA5 blue, PA6 red, PA7 yellow, PB6 green). Live = LED of the sounding key (K1 blue, K2 red, K3 yellow, K4 green), Recording = red blink, Playback = LED of the played note's key.
5. **Additional Peripherals (5 Peripherals required)**:
   - TIM3: 1 kHz TRGO trigger for ADC
   - TIM4: PB7 TIM4_CH2 Hardware PWM for buzzer tone generation
   - I2C1: PB8 (SCL) & PB9 (SDA) Fast Mode ~400 kHz for 1.30" SH1106 OLED
   - DMA1: Stream 6 Channel 1 for I2C1 display page transfer
   - DMA2: Stream 0 Channel 0 for ADC1 circular transfer
6. **Toyota MISRA-C Compliance**: 100% compliant with the 22 Toyota Embedded MISRA-C rules (no `//` comments, no ternary `? :`, explicit `U` suffixes, braces on separate lines, mandatory default in switch, etc.).
7. **Software Architecture**: Clean separation between Application Layer (`app_synth`) and Driver Layer (`bsp_*`).

## Current Status (Pass 2 + Toyota rule fixes + CMSIS register names - not yet tested on hardware)
Codebase: **3,284 -> 2,361 lines (-28%)**, measured by `verify_misra.py` (Src + Inc, excluding syscalls/sysmem).
Build: 0 errors, 0 compiler warnings (4 newlib `_close/_write ... not implemented` linker notes are pre-existing).
`verify_misra.py`: 0 violations. `.agents/skills/toyota-misra-c/scripts/audit_misra.py`: all 9 source files PASS.

| Module / File | Before | After | Notes |
|:---|:---:|:---:|:---|
| `Src/app_synth.c` | 1,030 | 792 | One mode FSM (`synth_set_mode` / `synth_toggle_mode`), debounce + combos (`synth_scan_keys`), UART telemetry, LED display |
| `Src/bsp_oled.c` | 1,029 | 585 | Font packed 4 glyphs/line, bounded `oled_wait_set()` / `oled_wait_clear()`, primitives file-private; recovery, keep-alive, DMA watchdog kept |
| `Src/bsp_joystick.c` | 218 | 137 | Switch FSM driven by EXTI2 edges (30 ms debounce, then one level read), boot guard via initial state |
| `Src/bsp_gpio.c` | 156 | 166 | Plain if/else `bsp_gpio_read_keys()` (`KEY_MASK_*`), 4 LEDs via `bsp_gpio_leds_set()` (BSx / BRx) |
| `Src/bsp_adc.c` | 195 | 92 | `bsp_adc_get_raw(ADC_IDX_*)` replaces `bsp_adc_get_joystick_raw()` |
| `Src/bsp_uart.c` | 189 | 163 | `bsp_uart_read_char(&c)` replaces `has_rx_char/get_rx_char`; `send_char` is private |
| buzzer + timer + main | 264 | 245 | CMSIS register names, dead branch removed |
| Headers | 203 | 181 | Unused APIs removed |
| **Total** | **3,284** | **2,361** | **-28%** |

### Toyota rule interpretation used in this project (from the course skill file)
- **Rule 19**: every `if` ends with an `else` (single `if` too, not only `if ... else if`). Empty branches use `else { /* No action required */ }`.
- **Rule 5**: no bare numbers in code; register field values, shifts, screen coordinates and timings are `#define`s. Data tables (note frequencies, Q12 ratios, font, SH1106 command lists) are allowed.
- **Rule 7**: no unreachable code (e.g. nothing after `app_synth_run()` in `main`).
- **Register style (as taught in the course labs)**: use the CMSIS names from `stm32f411xe.h` (`GPIO_MODER_MODER5_0`, `GPIO_BSRR_BS5`, `SYSCFG_EXTICR1_EXTI2_PC`, `EXTI_IMR_MR2`, `ADC_SMPR2_SMP4`, ...). Clear the field with `&= ~FIELD`, then set it with `|= FIELD_0 / FIELD_1`. Only values that have no CMSIS name get a `#define` (AF numbers like `GPIO_AF7_USART2`, ADC channel numbers, CPACR CP10/CP11). Do not use `0b` literals (GCC extension).

### Why not ~1,605
About 30% of every file is lone `{` / `}` lines (brace rule), and Rule 19 adds an `else` block to every `if`.
Going lower requires removing features (key combos ~-45, vibrato ~-20, UART note triggers ~-10). The user has not chosen to do so.

### Next Steps
1. Flash and test on hardware, especially long-running OLED stability (bus recovery / keep-alive paths were condensed).
2. Do not reintroduce UART/ADC polling; keep all MISRA rules above.
