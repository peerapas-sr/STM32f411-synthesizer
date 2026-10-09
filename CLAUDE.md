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
3. **External Interrupt**: At least 1 EXTI interrupt. Currently configured: EXTI Line 2 on PC2 (Joystick SW) with `EXTI2_IRQHandler`.
4. **GPIO Configuration**: Clean Input Pull-Up (Keys, Joystick SW) and Output Push-Pull (PA6 Red LED).
5. **Additional Peripherals (5 Peripherals required)**:
   - TIM3: 1 kHz TRGO trigger for ADC
   - TIM4: PB7 TIM4_CH2 Hardware PWM for buzzer tone generation
   - I2C1: PB8 (SCL) & PB9 (SDA) Fast Mode ~400 kHz for 1.30" SH1106 OLED
   - DMA1: Stream 6 Channel 1 for I2C1 display page transfer
   - DMA2: Stream 0 Channel 0 for ADC1 circular transfer
6. **Toyota MISRA-C Compliance**: 100% compliant with the 22 Toyota Embedded MISRA-C rules (no `//` comments, no ternary `? :`, explicit `U` suffixes, braces on separate lines, mandatory default in switch, etc.).
7. **Software Architecture**: Clean separation between Application Layer (`app_synth`) and Driver Layer (`bsp_*`).

## Current Status (Pass 2 Condensation - Done, not yet tested on hardware)
Option A condensation is complete: **3,284 -> 1,994 lines (-40%)**, measured by `verify_misra.py` (Src + Inc, excluding syscalls/sysmem).
Build: 0 errors, 0 compiler warnings (4 newlib `_close/_write ... not implemented` linker notes are pre-existing). MISRA script: 0 violations.

| Module / File | Before | After | Notes |
|:---|:---:|:---:|:---|
| `Src/app_synth.c` | 1,030 | 637 | One mode FSM (`synth_set_mode` / `synth_toggle_mode`), merged debounce + combos (`synth_scan_keys`), UART telemetry kept |
| `Src/bsp_oled.c` | 1,029 | 501 | Font packed 4 glyphs/line, single bounded `oled_wait()`, primitives file-private; recovery, keep-alive, DMA watchdog kept |
| `Src/bsp_joystick.c` | 218 | 116 | Flat switch FSM, boot guard via initial state, no init function |
| `Src/bsp_gpio.c` | 156 | 103 | Branchless `bsp_gpio_read_keys()`, LED via BSRR |
| `Src/bsp_adc.c` | 195 | 85 | `bsp_adc_get_raw(ADC_IDX_*)` replaces `bsp_adc_get_joystick_raw()` |
| `Src/bsp_uart.c` | 189 | 137 | `bsp_uart_read_char(&c)` replaces `has_rx_char/get_rx_char`; `send_char` is private |
| buzzer + timer + main | 264 | 247 | Empty `else` blocks removed |
| Headers | 203 | 168 | Unused APIs removed |
| **Total** | **3,284** | **1,994** | **-40%** |

### Why not ~1,605
About 30% of every file is lone `{` / `}` lines required by the brace rule (e.g. 179 of `app_synth.c`'s lines before logs were restored).
The original targets were also computed from wrong baseline sizes. Going lower requires removing features
(key combos ~-45, vibrato ~-20, UART note triggers ~-10) or relaxing the brace rule. The user has not chosen to do either.

### Next Steps
1. Flash and test on hardware, especially long-running OLED stability (bus recovery / keep-alive paths were condensed).
2. Do not reintroduce UART/ADC polling; keep all MISRA rules above.
