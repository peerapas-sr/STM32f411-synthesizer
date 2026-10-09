/*******************************************************************************
 * File Name   : main.c
 * Description : Entry Point - Synthesizer with Sequence Recorder & Playback
 * Target MCU  : STM32 Nucleo-F411RE (STM32F411RET6)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 *
 * Requirements Satisfied:
 *   [1] GPIO                  : 4 Keys In (PA10, PB3, PB5, PB4), HW-504 SW (PC2), Red LED (PA6)
 *   [2] UART (Interrupt/DMA)  : USART2 115200 bps via RX Interrupt (NO Polling)
 *   [3] ADC (Interrupt/DMA)   : 3-Channel ADC1 via TIM3 TRGO & DMA2 Stream 0 (Zero CPU)
 *   [4] External Interrupt    : EXTI Line 10 on PA10 [Key 1]
 *   [5] Additional Peripheral : TIM3 Hardware Timer 1ms Periodic Interrupt & TRGO Output
 *   [6] MISRA-C Compliance    : 22 Toyota Rules fully enforced
 *   [7] Software Structure    : Clean Separation of Application and BSP Drivers
 *   [8] I2C DMA               : I2C1 DMA1 Stream 6 Channel 1 for OLED Framebuffer
 ******************************************************************************/

#include <stdint.h>
#define STM32F411xE
#include "stm32f4xx.h"

#include "bsp_gpio.h"
#include "bsp_adc.h"
#include "bsp_uart.h"
#include "bsp_buzzer.h"
#include "bsp_joystick.h"
#include "bsp_timer.h"
#include "bsp_oled.h"
#include "app_synth.h"

/* System Clock definition required by CMSIS */
uint32_t SystemCoreClock = 16000000U;

int main(void)
{
    /* 0. Enable Cortex-M4 Hardware FPU Coprocessors CP10 & CP11 (Full Access) */
    SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
    __DSB();
    __ISB();

    /* 1. Initialize Board Support Package (Drivers) */
    bsp_gpio_init();     /* 4 Keys, HW-504 SW (PC2) & EXTI10 on PA10 */
    bsp_buzzer_init();   /* Hardware PWM Buzzer on PB7 (TIM4_CH2) */
    bsp_adc_init();      /* 3-Channel ADC1 via TIM3 TRGO & DMA2 Stream 0 */
    bsp_joystick_init(); /* HW-504 Dual-Axis Joystick Driver */
    bsp_uart_init();     /* USART2 with RXNE Interrupt (No Polling) */
    bsp_timer_init();    /* TIM3 1ms Periodic Interrupt & TRGO Trigger */
    bsp_oled_init();     /* 1.30" I2C OLED with DMA1 Stream 6 Channel 1 */

    /* 2. Initialize and Run Main Synthesizer Application Layer */
    app_synth_init();
    app_synth_run();

    /* Should never reach here */
    while (1 != 0)
    {
        /* Infinite loop for safety */
    }
}
