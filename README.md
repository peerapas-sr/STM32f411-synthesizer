# STM32F411RE Real-Time Synthesizer & Hardware Sequencer

[![Target](https://img.shields.io/badge/Target-STM32F411RET6%20(Nucleo--64)-blue.svg)](https://www.st.com/en/evaluation-tools/nucleo-f411re.html)
[![Architecture](https://img.shields.io/badge/Architecture-Bare--Metal%20CMSIS-green.svg)]()

A high-performance, bare-metal **Real-Time 8-Voice Diatonic Synthesizer and 64-Step Hardware Sequencer** developed for the **STMicroelectronics STM32F411RET6** (Nucleo-F411RE, ARM Cortex-M4F @ 16 MHz). 

Built without heavy vendor HAL libraries using direct CMSIS register manipulation, featuring autonomous **Zero-CPU DMA subsystems**, an interrupt-driven audio engine, a real-time 1.30" I2C OLED virtual piano interface

---

## Key Highlights

- **8-Voice High Diatonic Synthesizer**: Generates octave 7–8 notes (C7 to C8: 2093 Hz – 4186 Hz) with microsecond precision.
- **64-Step Sequence Recorder & Looper**: Records notes, active durations, and rest gaps with seamless infinite looping playback.
- **Note Release Sustain Engine**: Features a 150 ms acoustic decay tail upon note release and zero-latency monophonic legato preemption.
- **Dual-Axis Analog Joystick Controls**:
  - **Pitch Bend**: Smooth $\pm 200$ Cents modulation using a fast integer Q16 polynomial calculation.
  - **Octave Bank Switching**: Instant toggle between Low Bank (C7–F7) and High Bank (G7–C8).
  - **Center Switch**: Short-click / long-press debounce FSM for playback and recording toggles.
- **1.30" I2C OLED Virtual Piano UI (SH1106 / SSD1306)**:
  - 1024-byte framebuffer with real-time piano key highlights, pitch deviation gauge, and volume bar.
  - Non-blocking DMA streaming (129 bytes/page every 5 ms) achieving ~25 FPS.
  - Silicon errata 2.13.7 workaround: 9-pulse SCL bit-banged hardware bus recovery.
- **Autonomous Zero-CPU ADC Subsystem**:
  - TIM3 TRGO hardware trigger (1 kHz) scans Volume Potentiometer (PA4), Joystick VRx (PC0), and VRy (PC1).
  - DMA2 Stream 0 circular buffer feeds SRAM directly without CPU polling.
- **Serial Interactive Console**:
  - USART2 @ 115,200 bps with an interrupt-driven RX ring buffer.
  - Supports remote piano keyboard input (`'1'`–`'8'`), status telemetry, and recording controls.

---

## System Architecture

The firmware enforces a strict **Two-Tier Architecture**:
1. **Application Layer (`app_*`)**: Sequencer Finite State Machine (FSM), key debounce & combo detection, note release sustain engine, and UI dispatching.
2. **Board Support Package / Driver Layer (`bsp_*`)**: Hardware drivers directly manipulating STM32 CMSIS registers with non-blocking service slices and autonomous DMA/hardware interrupt routines.

```mermaid
graph TD
    subgraph Application_Layer [Application Layer - app_synth.c]
        MainLoop[Super-Loop Engine]
        SeqFSM[Sequencer FSM: Idle / Record / Playback]
        SustainEng[Note Release Sustain Engine: 150 ms Tail]
        KeyDebounce[Key Debounce & Combo Engine]
    end

    subgraph BSP_Drivers [Board Support Package - bsp_*]
        BSP_Buzzer[bsp_buzzer.c: Tone & Pulse Duty Engine]
        BSP_OLED[bsp_oled.c: 1024B Framebuffer + DMA Streamer]
        BSP_ADC[bsp_adc.c: 3-Channel Scan Driver]
        BSP_Joy[bsp_joystick.c: EMA Filter + Normalization]
        BSP_UART[bsp_uart.c: USART2 115.2k Ring Buffer]
        BSP_Timer[bsp_timer.c: 1ms Timebase & TRGO Engine]
        BSP_GPIO[bsp_gpio.c: Keys, SW, LED & EXTI10]
    end

    subgraph STM32F411_Hardware [STM32F411RE Hardware Peripherals]
        TIM4[TIM4: 1 MHz Microsecond Tone Generator]
        I2C1_DMA1[I2C1 + DMA1 Stream 6 Ch 1]
        ADC1_DMA2[ADC1 + DMA2 Stream 0 Ch 0]
        TIM3[TIM3: 1 kHz Master TRGO & SysTick]
        USART2[USART2 RXNE Interrupt]
        EXTI10[EXTI Line 10 on PA10]
    end

    MainLoop --> SeqFSM
    MainLoop --> SustainEng
    MainLoop --> KeyDebounce
    MainLoop --> BSP_OLED
    MainLoop --> BSP_Buzzer
    MainLoop --> BSP_Joy
    MainLoop --> BSP_UART

    BSP_Buzzer --> TIM4
    BSP_OLED --> I2C1_DMA1
    BSP_ADC --> ADC1_DMA2
    BSP_Timer --> TIM3
    BSP_UART --> USART2
    BSP_GPIO --> EXTI10
```

---

## Hardware Pinout & Wiring

| Peripheral / Signal | Pin | Mode / Configuration | Hardware Subsystem Function |
| :--- | :---: | :--- | :--- |
| **I2C1_SCL** | **PB8** | AF4 (Open-Drain, Pull-Up, Very High Speed) | 1.30" OLED Clock (~380–400 kHz Fast Mode) |
| **I2C1_SDA** | **PB9** | AF4 (Open-Drain, Pull-Up, Very High Speed) | 1.30" OLED Data (DMA1 Stream 6 Channel 1) |
| **ADC1_IN4** | **PA4** | Analog Mode | Master Volume Potentiometer (0–4095 raw) |
| **ADC1_IN10** | **PC0** | Analog Mode | HW-504 Joystick VRx (Pitch Bend $\pm 200$ Cents) |
| **ADC1_IN11** | **PC1** | Analog Mode | HW-504 Joystick VRy (Octave Bank Switch) |
| **USART2_TX** | **PA2** | AF7 (Push-Pull, Very High Speed) | Serial Telemetry & Console @ 115,200 bps |
| **USART2_RX** | **PA3** | AF7 (Pull-Up) | Serial Remote Commands (RXNE Interrupt) |
| **Key 1** | **PA10** | Input Pull-Up + EXTI10 Falling Edge | Note 1 (DO / SOL) & Record Combo Key |
| **Key 2** | **PB3** | Input Pull-Up | Note 2 (RE / LA) & Playback Combo Key |
| **Key 3** | **PB5** | Input Pull-Up | Note 3 (MI / TI) & Playback Combo Key |
| **Key 4** | **PB4** | Input Pull-Up | Note 4 (FA / HIGH DO) & Record Combo Key |
| **Joystick SW** | **PC2** | Input Pull-Up | HW-504 Center Push Switch (Short/Long click) |
| **Buzzer Out** | **PB7** | AF2 (TIM4_CH2, Push-Pull, High Speed) | Hardware PWM Audio Output with Cubic Volume Pulse Shaping |
| **Red LED** | **PA6** | Output Push-Pull | Recording / Sustain / Combo Status Indicator |

---

## Interrupt & DMA Priority Matrix

To ensure zero audio glitching and eliminate CPU stalls, interrupts are strictly prioritized:

| Vector | ISR Handler | Priority | Trigger Source | Functionality |
| :--- | :--- | :---: | :--- | :--- |
| **`DMA1_Stream6_IRQn`** | `DMA1_Stream6_IRQHandler` | **2** | DMA1 Transfer Complete | Releases I2C DMA lock, halts DMA, issues hardware STOP condition. |
| **`USART2_IRQn`** | `USART2_IRQHandler` | **2** | USART2 `RXNE` | Pushes incoming bytes into a 64-byte circular ring buffer. |
| **`EXTI15_10_IRQn`** | `EXTI15_10_IRQHandler` | **2** | PA10 Falling Edge | Latches Key 1 press event flag for the main application loop. |
| **`TIM3_IRQn`** | `TIM3_IRQHandler` | **3** | TIM3 Update (1 kHz) | Increments system millisecond counter `g_u4t_system_ms`. |
| *DMA2 Stream 0* | *(No Interrupt)* | — | TIM3 TRGO Pulse | **Circular Mode**: Transfers 3 ADC conversions directly into SRAM. |
| *TIM4 Channel 2* | *(No Interrupt)* | — | Hardware Counter | **Hardware PWM**: Autonomous square-wave audio on PB7 (Zero-CPU). |

---

## Operating Instructions

### 1. Playing Notes (Live Synthesizer Mode)
- **Keys 1 to 4**: Triggers notes in the active octave bank.
- **Octave Bank Switching (Joystick VRy)**:
  - **Center / Up** (`VRy > -350`): **Low Bank** $\rightarrow$ C7 (2093 Hz), D7 (2349 Hz), E7 (2637 Hz), F7 (2794 Hz).
  - **Down** (`VRy <= -350`): **High Bank** $\rightarrow$ G7 (3136 Hz), A7 (3520 Hz), B7 (3951 Hz), C8 (4186 Hz).
- **Pitch Bending (Joystick VRx)**:
  - Push Left/Right to bend pitch continuously by up to $\pm 200$ Cents.

### 2. Sequence Recording & Looping Playback
- **Toggle Recording Mode**:
  - **Hardware Combo**: Press and hold **Key 1 + Key 4** for 600 ms.
  - **Joystick**: Long-press center switch for 600 ms.
  - **UART**: Send `'r'` or `'R'`.
  - *Feedback*: Red LED blinks every 200 ms; pitch table logs steps to UART.
- **Toggle Playback Mode**:
  - **Hardware Combo**: Press and hold **Key 2 + Key 3** for 600 ms.
  - **Joystick**: Short-click center switch.
  - **UART**: Send `'p'` or `'P'`.
  - *Behavior*: Plays recorded steps and **seamlessly loops back to step 0** indefinitely until stopped.
- **Clear Sequence**:
  - Send `'c'` or `'C'` via UART.

### 3. UART Console Protocol (115,200 bps, 8-N-1)
Connect a serial terminal (PuTTY, Tera Term, minicom) to the Nucleo Virtual COM port:
- `'1'` to `'8'`: Trigger note C7 through C8 remotely (300 ms gate duration).
- `'r'` / `'R'`: Toggle Recording mode.
- `'p'` / `'P'`: Toggle Playback mode.
- `'c'` / `'C'`: Clear sequence memory.
- `'?'`: Print real-time system status and help menu.

---

## Project Structure

```
├── Inc/
│   ├── app_synth.h        # Synthesizer FSM & sequencer declarations
│   ├── bsp_adc.h          # 3-Channel ADC & DMA2 driver interface
│   ├── bsp_buzzer.h       # Audio pulse shaper & tone engine interface
│   ├── bsp_gpio.h         # GPIO pin configurations & EXTI10 interface
│   ├── bsp_joystick.h     # HW-504 EMA filter & debounce FSM interface
│   ├── bsp_oled.h         # OLED graphics, virtual piano & I2C DMA interface
│   ├── bsp_timer.h        # TIM3 timebase & delay utilities
│   └── bsp_uart.h         # USART2 interrupt-driven ring buffer interface
├── Src/
│   ├── main.c             # System entry point & FPU coprocessor init
│   ├── app_synth.c        # Synthesizer application logic & sequencer FSM
│   ├── bsp_adc.c          # ADC1 + TIM3 TRGO + DMA2 circular engine
│   ├── bsp_buzzer.c       # Hardware PWM tone & volume pulse shaper (PB7 / TIM4_CH2)
│   ├── bsp_gpio.c         # GPIO setup, LED control & EXTI15_10 ISR
│   ├── bsp_joystick.c     # Joystick EMA filtering, calibration & switch FSM
│   ├── bsp_oled.c         # 1024B Framebuffer, I2C1 bus recovery & DMA ISR
│   ├── bsp_timer.c        # TIM3 1ms tick & microsecond delays
│   └── bsp_uart.c         # USART2 RXNE interrupt driver & ring buffer
├── Startup/
│   └── startup_stm32f411retx.s  # Vector table & reset handler
├── STM32F411RETX_FLASH.ld # Linker script (Flash: 512KB, SRAM: 128KB)
└── README.md
```

---

## Build & Flash

### Prerequisites
- **Toolchain**: ARM GNU Toolchain (`arm-none-eabi-gcc` v10+)
- **Build System**: GNU Make
- **Debugger / Programmer**: ST-Link v2-1 (integrated on Nucleo-F411RE)

### Building via Command Line
```bash
# Navigate to Debug directory
cd Debug

# Compile all source files
make -j4 all
```

### Compiler Flags
The project is strictly compiled with:
```bash
-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -O2 -Wall -Wextra -Werror -std=c99
```

---

## Toyota MISRA-C Compliance

All C source code strictly adheres to the **MISRA-C Rules**:
- **Zero single-line comments**: Enforces standard `/* ... */` block comments exclusively.
- **Strict Brace Discipline**: Opening and closing braces on their own separate lines.
- **No Magic Numbers**: All constants are defined using explicit `#define` with typed suffixes (`U`, `UL`).
- **No Ternary Operator**: The `? :` construct is completely banned in favor of explicit `if / else`.
- **Complete Branch Coverage**: All `if ... else if` chains terminate with an explicit `else` block; all `switch` statements include a mandatory `default:` clause.
- **Strict Hungarian Notation**: Variables prefixed by type (`u1t_`, `u2t_`, `u4t_`, `s1t_`, `s2t_`, `s4t_`, `b_`, `c_`, `g_`, `p_`).
