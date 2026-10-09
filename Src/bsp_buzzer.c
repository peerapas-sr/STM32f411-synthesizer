/*******************************************************************************
 * File Name   : bsp_buzzer.c
 * Description : Board Support Package - Hardware PWM Buzzer Driver (PB7 / TIM4_CH2)
 * Target MCU  : STM32F411RET6 (Nucleo-F411RE)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_buzzer.h"
#define STM32F411xE
#include "stm32f4xx.h"

/* Named Constants (Rule 5 & Rule 10) */
#define BUZZER_PIN_PB7          (7U)       /* PB7: TIM4_CH2 (AF2) */
#define BUZZER_VOL_MIN_THRESH   (80U)
#define SEC_TO_US_FACTOR        (1000000U)
#define ADC_MAX_VAL             (4095U)
#define BUZZER_FREQ_MIN_HZ      (50U)
#define BUZZER_FREQ_MAX_HZ      (5000U)
#define TIM4_PRESCALER_1MHZ     (15U)      /* 16 MHz / (15 + 1) = 1 MHz (1 tick = 1 us) */
#define BUZZER_MIN_PULSE_TICKS  (2U)

void bsp_buzzer_init(void)
{
    /* 1. Enable GPIOB and TIM4 Peripheral Clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    /* 2. Configure PB7 as Alternate Function AF2 (TIM4_CH2, Push-Pull, High Speed) */
    GPIOB->MODER &= ~(3UL << (BUZZER_PIN_PB7 * 2U));
    GPIOB->MODER |=  (2UL << (BUZZER_PIN_PB7 * 2U));

    GPIOB->OTYPER &= ~(1UL << BUZZER_PIN_PB7);
    GPIOB->OSPEEDR |= (3UL << (BUZZER_PIN_PB7 * 2U));
    GPIOB->PUPDR &= ~(3UL << (BUZZER_PIN_PB7 * 2U));

    GPIOB->AFR[0] &= ~(15UL << (BUZZER_PIN_PB7 * 4U));
    GPIOB->AFR[0] |=  (2UL  << (BUZZER_PIN_PB7 * 4U));

    /* 3. Configure TIM4 Timebase: 1 MHz Counter Rate (1 tick = 1 us) */
    TIM4->PSC = TIM4_PRESCALER_1MHZ;
    TIM4->CR1 = TIM_CR1_ARPE;

    /* 4. Configure Channel 2 for Hardware PWM Mode 1 with Preload Enabled */
    TIM4->CCMR1 &= ~TIM_CCMR1_OC2M;
    TIM4->CCMR1 |=  ((6UL << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE);
    TIM4->CCER  |=  TIM_CCER_CC2E;

    /* 5. Initialize Output in Muted State */
    TIM4->CCR2 = 0U;
    TIM4->CR1 &= ~TIM_CR1_CEN;
}

void bsp_buzzer_off(void)
{
    TIM4->CR1 &= ~TIM_CR1_CEN;
    TIM4->CCR2 = 0U;
}

void bsp_buzzer_set_tone(uint32_t u4t_freq_hz, uint16_t u2t_vol_adc)
{
    if ((u4t_freq_hz < BUZZER_FREQ_MIN_HZ) || (u4t_freq_hz > BUZZER_FREQ_MAX_HZ) || (u2t_vol_adc < BUZZER_VOL_MIN_THRESH))
    {
        bsp_buzzer_off();
    }
    else
    {
        uint32_t u4t_period_us = SEC_TO_US_FACTOR / u4t_freq_hz;
        uint32_t u4t_max_high = u4t_period_us / 2U;

        if (u4t_max_high == 0U)
        {
            u4t_max_high = 1U;
        }
        else
        {
            /* Period is valid */
        }

        /* Cubic perceptual volume curve: (vol_adc / 4095)^3 to match human hearing */
        uint32_t u4t_v = (uint32_t)u2t_vol_adc;
        uint32_t u4t_v_cube = (u4t_v * u4t_v) / ADC_MAX_VAL;
        u4t_v_cube = (u4t_v_cube * u4t_v) / ADC_MAX_VAL;

        uint32_t u4t_high = (u4t_v_cube * u4t_max_high) / ADC_MAX_VAL;
        if (u4t_high < BUZZER_MIN_PULSE_TICKS)
        {
            u4t_high = BUZZER_MIN_PULSE_TICKS;
        }
        else if (u4t_high >= u4t_period_us)
        {
            u4t_high = u4t_max_high;
        }
        else
        {
            /* Valid high pulse duration */
        }

        /* Update Hardware Timer Reload (Frequency) and Compare (Duty Cycle) */
        TIM4->ARR  = u4t_period_us - 1U;
        TIM4->CCR2 = u4t_high;

        if ((TIM4->CR1 & TIM_CR1_CEN) == 0U)
        {
            TIM4->EGR = TIM_EGR_UG;
            TIM4->CR1 |= TIM_CR1_CEN;
        }
        else
        {
            /* Timer is already running; shadow preload registers update seamlessly */
        }
    }
}

void bsp_buzzer_play_chunk(uint32_t u4t_freq_hz, uint16_t u2t_vol_adc)
{
    bsp_buzzer_set_tone(u4t_freq_hz, u2t_vol_adc);
}
