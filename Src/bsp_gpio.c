/*******************************************************************************
 * File Name   : bsp_gpio.c
 * Description : 4 Keys (PA10, PB3, PB5, PB4), Joystick SW (PC2 + EXTI2), 4 LEDs (PA5, PA6, PA7, PB6)
 * Target MCU  : STM32F411RET6 (Nucleo-F411RE)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_gpio.h"
#define STM32F411xE
#include "stm32f4xx.h"

/* Named Constants (Rule 5 & Rule 10) */
#define EXTI2_NVIC_PRIORITY     (2U)

static volatile bool g_b_joy_sw_exti_flag = false;

void bsp_gpio_init(void)
{
    /* --- Setup peripheral clock --- */
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN);
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* --- Setup GPIO PA5 (blue), PA6 (red), PA7 (yellow) LED: output push-pull, no pull, start OFF --- */
    GPIOA->BSRR = (GPIO_BSRR_BR5 | GPIO_BSRR_BR6 | GPIO_BSRR_BR7);
    GPIOA->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOA->MODER |= (GPIO_MODER_MODER5_0 | GPIO_MODER_MODER6_0 | GPIO_MODER_MODER7_0);    /* 01 = output */
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT5 | GPIO_OTYPER_OT6 | GPIO_OTYPER_OT7);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD5 | GPIO_PUPDR_PUPD6 | GPIO_PUPDR_PUPD7);

    /* --- Setup GPIO PB6 (green) LED: output push-pull, no pull, start OFF --- */
    GPIOB->BSRR = GPIO_BSRR_BR6;
    GPIOB->MODER &= ~GPIO_MODER_MODER6;
    GPIOB->MODER |= GPIO_MODER_MODER6_0;    /* 01 = output */
    GPIOB->OTYPER &= ~GPIO_OTYPER_OT6;
    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD6;

    /* --- Setup GPIO PA10 (Key 1): input, pull-up --- */
    GPIOA->MODER &= ~GPIO_MODER_MODER10;    /* 00 = input */
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD10;
    GPIOA->PUPDR |= GPIO_PUPDR_PUPD10_0;    /* 01 = pull-up */

    /* --- Setup GPIO PB3, PB5, PB4 (Keys 2-4): input, pull-up --- */
    GPIOB->MODER &= ~(GPIO_MODER_MODER3 | GPIO_MODER_MODER4 | GPIO_MODER_MODER5);
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD3 | GPIO_PUPDR_PUPD4 | GPIO_PUPDR_PUPD5);
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPD3_0 | GPIO_PUPDR_PUPD4_0 | GPIO_PUPDR_PUPD5_0);

    /* --- Setup GPIO PC2 (Joystick SW): input, pull-up --- */
    GPIOC->MODER &= ~GPIO_MODER_MODER2;
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD2;
    GPIOC->PUPDR |= GPIO_PUPDR_PUPD2_0;

    /* --- Setup EXTI Line 2 on PC2: falling edge (press, active low) --- */
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI2;
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI2_PC;
    EXTI->IMR |= EXTI_IMR_MR2;
    EXTI->FTSR |= EXTI_FTSR_TR2;
    EXTI->RTSR &= ~EXTI_RTSR_TR2;
    NVIC_SetPriority(EXTI2_IRQn, EXTI2_NVIC_PRIORITY);
    NVIC_EnableIRQ(EXTI2_IRQn);
}

/* Key bitmask, pressed = 1 (KEY_MASK_1..4). Keys are active low: pin reads 0 when pressed. */
uint8_t bsp_gpio_read_keys(void)
{
    uint8_t u1t_keys = 0U;

    if ((GPIOA->IDR & GPIO_IDR_ID10) == 0U)
    {
        u1t_keys |= KEY_MASK_1;
    }
    else
    {
        /* No action required */
    }
    if ((GPIOB->IDR & GPIO_IDR_ID3) == 0U)
    {
        u1t_keys |= KEY_MASK_2;
    }
    else
    {
        /* No action required */
    }
    if ((GPIOB->IDR & GPIO_IDR_ID5) == 0U)
    {
        u1t_keys |= KEY_MASK_3;
    }
    else
    {
        /* No action required */
    }
    if ((GPIOB->IDR & GPIO_IDR_ID4) == 0U)
    {
        u1t_keys |= KEY_MASK_4;
    }
    else
    {
        /* No action required */
    }
    return u1t_keys;
}

bool bsp_gpio_read_joystick_switch(void)
{
    return ((GPIOC->IDR & GPIO_IDR_ID2) == 0U);
}

/* Set all 4 LEDs from a mask (LED_MASK_*): BSx turns the pin on, BRx turns it off */
void bsp_gpio_leds_set(uint8_t u1t_mask)
{
    if ((u1t_mask & LED_MASK_BLUE) != 0U)
    {
        GPIOA->BSRR = GPIO_BSRR_BS5;
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR5;
    }
    if ((u1t_mask & LED_MASK_RED) != 0U)
    {
        GPIOA->BSRR = GPIO_BSRR_BS6;
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR6;
    }
    if ((u1t_mask & LED_MASK_YELLOW) != 0U)
    {
        GPIOA->BSRR = GPIO_BSRR_BS7;
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR7;
    }
    if ((u1t_mask & LED_MASK_GREEN) != 0U)
    {
        GPIOB->BSRR = GPIO_BSRR_BS6;
    }
    else
    {
        GPIOB->BSRR = GPIO_BSRR_BR6;
    }
}

bool bsp_gpio_get_exti_flag(void)
{
    return g_b_joy_sw_exti_flag;
}

void bsp_gpio_clear_exti_flag(void)
{
    g_b_joy_sw_exti_flag = false;
}

/* EXTI Line 2 ISR: Joystick SW (PC2) pressed */
void EXTI2_IRQHandler(void)
{
    if ((EXTI->PR & EXTI_PR_PR2) != 0U)
    {
        EXTI->PR = EXTI_PR_PR2;    /* Write 1 to clear pending */
        g_b_joy_sw_exti_flag = true;
    }
    else
    {
        /* No action required */
    }
}
