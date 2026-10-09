/*******************************************************************************
 * File Name   : bsp_gpio.c
 * Description : 4 Keys (PA10 + EXTI10, PB3, PB5, PB4), Joystick SW (PC2), Red LED (PA6)
 * Target MCU  : STM32F411RET6 (Nucleo-F411RE)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_gpio.h"
#define STM32F411xE
#include "stm32f4xx.h"

/* Named Constants (Rule 5 & Rule 10) */
#define EXTI10_NVIC_PRIORITY    (2U)
#define EXTI_LINE_10_MASK       (1UL << KEY1_PIN)
#define EXTICR3_LINE10_MASK     (0x0FUL << 8U)     /* EXTICR[2] bits 11:8 = 0 selects Port A */
#define LED_RED_MODE_MASK       (3UL << (LED_RED_PIN * 2U))
#define LED_RED_OUTPUT_MODE     (1UL << (LED_RED_PIN * 2U))
#define KEY1_MODE_MASK          (3UL << (KEY1_PIN * 2U))
#define KEY1_PULL_UP            (1UL << (KEY1_PIN * 2U))
#define PORTB_KEYS_MODE_MASK    ((3UL << (KEY2_PIN * 2U)) | (3UL << (KEY3_PIN * 2U)) | (3UL << (KEY4_PIN * 2U)))
#define PORTB_KEYS_PULL_UP      ((1UL << (KEY2_PIN * 2U)) | (1UL << (KEY3_PIN * 2U)) | (1UL << (KEY4_PIN * 2U)))
#define JOY_SW_MODE_MASK        (3UL << (JOY_SW_PIN * 2U))
#define JOY_SW_PULL_UP          (1UL << (JOY_SW_PIN * 2U))

static volatile bool g_b_exti10_flag = false;

void bsp_gpio_init(void)
{
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN);
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* PA6 Red LED: push-pull output, starts OFF */
    GPIOA->BSRR = (1UL << (LED_RED_PIN + 16U));
    GPIOA->MODER = (GPIOA->MODER & ~LED_RED_MODE_MASK) | LED_RED_OUTPUT_MODE;
    GPIOA->OTYPER &= ~(1UL << LED_RED_PIN);
    GPIOA->PUPDR &= ~LED_RED_MODE_MASK;

    /* Inputs with pull-up: PA10 (Key 1), PB3/PB5/PB4 (Keys 2-4), PC2 (Joystick SW) */
    GPIOA->MODER &= ~KEY1_MODE_MASK;
    GPIOA->PUPDR = (GPIOA->PUPDR & ~KEY1_MODE_MASK) | KEY1_PULL_UP;
    GPIOB->MODER &= ~PORTB_KEYS_MODE_MASK;
    GPIOB->PUPDR = (GPIOB->PUPDR & ~PORTB_KEYS_MODE_MASK) | PORTB_KEYS_PULL_UP;
    GPIOC->MODER &= ~JOY_SW_MODE_MASK;
    GPIOC->PUPDR = (GPIOC->PUPDR & ~JOY_SW_MODE_MASK) | JOY_SW_PULL_UP;

    /* EXTI Line 10 on PA10: falling edge (key press, active low) */
    SYSCFG->EXTICR[2] &= ~EXTICR3_LINE10_MASK;
    EXTI->IMR  |= EXTI_LINE_10_MASK;
    EXTI->FTSR |= EXTI_LINE_10_MASK;
    EXTI->RTSR &= ~EXTI_LINE_10_MASK;
    NVIC_SetPriority(EXTI15_10_IRQn, EXTI10_NVIC_PRIORITY);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* Raw key bitmask, pressed = 1 (Bit 0: Key 1, Bit 1: Key 2, Bit 2: Key 3, Bit 3: Key 4) */
uint8_t bsp_gpio_read_keys(void)
{
    uint32_t u4t_a = ~GPIOA->IDR;    /* Active low: invert so pressed = 1 */
    uint32_t u4t_b = ~GPIOB->IDR;

    return (uint8_t)(((u4t_a >> KEY1_PIN) & 1U) |
                     (((u4t_b >> KEY2_PIN) & 1U) << 1U) |
                     (((u4t_b >> KEY3_PIN) & 1U) << 2U) |
                     (((u4t_b >> KEY4_PIN) & 1U) << 3U));
}

bool bsp_gpio_read_joystick_switch(void)
{
    return ((GPIOC->IDR & (1UL << JOY_SW_PIN)) == 0U);
}

void bsp_gpio_led_red_set(bool b_state)
{
    if (b_state == true)
    {
        GPIOA->BSRR = (1UL << LED_RED_PIN);
    }
    else
    {
        GPIOA->BSRR = (1UL << (LED_RED_PIN + 16U));
    }
}

bool bsp_gpio_get_exti_flag(void)
{
    return g_b_exti10_flag;
}

void bsp_gpio_clear_exti_flag(void)
{
    g_b_exti10_flag = false;
}

/* EXTI Lines 10..15 ISR: Key 1 (PA10) pressed */
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & EXTI_LINE_10_MASK) != 0U)
    {
        EXTI->PR = EXTI_LINE_10_MASK;    /* Write 1 to clear pending */
        g_b_exti10_flag = true;
    }
}
