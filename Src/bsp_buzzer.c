/*******************************************************************************
 * File Name   : bsp_buzzer.c
 * Description : Board Support Package - Hardware PWM Buzzer Driver (PB7 / TIM4_CH2)
 * Target MCU  : STM32F411RET6 (Nucleo-F411RE)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_buzzer.h"
#define STM32F411xE
#include "stm32f4xx.h"
#include "bsp_reg_fields.h"

/* Named Constants (Rule 5 & Rule 10) */
#define BUZZER_VOL_MIN_THRESH   (80U)
#define SEC_TO_US_FACTOR        (1000000U)
#define ADC_MAX_VAL             (4095U)
#define BUZZER_FREQ_MIN_HZ      (50U)
#define BUZZER_FREQ_MAX_HZ      (5000U)
#define TIM4_PRESCALER_1MHZ     (15U)      /* 16 MHz / (15 + 1) = 1 MHz (1 tick = 1 us) */
#define BUZZER_MIN_PULSE_TICKS  (2U)
#define BUZZER_HALF_PERIOD_DIV  (2U)       /* Max duty = 50 % */
#define TIM_OC_MODE_PWM1        (0b110U)   /* OCxM: 110 = PWM mode 1 */

void bsp_buzzer_init(void)
{
    /* --- Setup peripheral clock --- */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    /* --- Setup GPIO PB7: AF2 (TIM4_CH2), push-pull, very high speed, no pull --- */
    GPIOB->MODER &= ~GPIO_MODER_MODER7;
    GPIOB->MODER |= (GPIO_MODE_AF << GPIO_MODER_MODER7_Pos);
    GPIOB->OTYPER &= ~GPIO_OTYPER_OT7;
    GPIOB->OSPEEDR |= (GPIO_SPEED_VERY_HIGH << GPIO_OSPEEDR_OSPEED7_Pos);
    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD7;
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL7;
    GPIOB->AFR[0] |= (GPIO_AF2_TIM4 << GPIO_AFRL_AFSEL7_Pos);

    /* --- Setup TIM4 timebase: 1 MHz counter rate (1 tick = 1 us) --- */
    TIM4->PSC = TIM4_PRESCALER_1MHZ;
    TIM4->CR1 = TIM_CR1_ARPE;

    /* --- Setup TIM4 CH2: PWM mode 1 with preload --- */
    TIM4->CCMR1 &= ~TIM_CCMR1_OC2M;
    TIM4->CCMR1 |= ((TIM_OC_MODE_PWM1 << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE);
    TIM4->CCER |= TIM_CCER_CC2E;

    /* --- Start muted --- */
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
        uint32_t u4t_max_high = u4t_period_us / BUZZER_HALF_PERIOD_DIV;

        /* Quadratic perceptual volume curve: (vol_adc / 4095)^2 */
        uint32_t u4t_v = (uint32_t)u2t_vol_adc;
        uint32_t u4t_v_scaled = (u4t_v * u4t_v) / ADC_MAX_VAL;

        /* High time never exceeds half a period because u4t_v_scaled <= ADC_MAX_VAL */
        uint32_t u4t_high = (u4t_v_scaled * u4t_max_high) / ADC_MAX_VAL;
        if (u4t_high < BUZZER_MIN_PULSE_TICKS)
        {
            u4t_high = BUZZER_MIN_PULSE_TICKS;
        }
        else
        {
            /* High time already within 2 ticks .. half period */
        }

        /* Update Hardware Timer Reload (Frequency) and Compare (Duty Cycle) */
        TIM4->ARR  = u4t_period_us - 1U;
        TIM4->CCR2 = u4t_high;

        /* Start once; while running, preloaded ARR/CCR2 update seamlessly at the next period */
        if ((TIM4->CR1 & TIM_CR1_CEN) == 0U)
        {
            TIM4->EGR = TIM_EGR_UG;
            TIM4->CR1 |= TIM_CR1_CEN;
        }
        else
        {
            /* No action required */
        }
    }
}
