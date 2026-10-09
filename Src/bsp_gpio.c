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
#define EXTI_LINE_2_MASK        (1UL << JOY_SW_PIN)
#define EXTICR1_LINE2_MASK      (0x0FUL << 8U)     /* EXTICR[0] bits 11:8 select the port of line 2 */
#define EXTICR1_LINE2_PORTC     (0x02UL << 8U)     /* 0x2 = Port C */
#define PORTA_LEDS_BITS         ((1UL << LED_BLUE_PIN) | (1UL << LED_RED_PIN) | (1UL << LED_YELLOW_PIN))
#define PORTA_LEDS_MODE_MASK    ((3UL << (LED_BLUE_PIN * 2U)) | (3UL << (LED_RED_PIN * 2U)) | (3UL << (LED_YELLOW_PIN * 2U)))
#define PORTA_LEDS_OUTPUT_MODE  ((1UL << (LED_BLUE_PIN * 2U)) | (1UL << (LED_RED_PIN * 2U)) | (1UL << (LED_YELLOW_PIN * 2U)))
#define PORTB_LED_BIT           (1UL << LED_GREEN_PIN)
#define PORTB_LED_MODE_MASK     (3UL << (LED_GREEN_PIN * 2U))
#define PORTB_LED_OUTPUT_MODE   (1UL << (LED_GREEN_PIN * 2U))
#define BSRR_RESET_SHIFT        (16U)
#define KEYMASK_POS_KEY2        (1U)       /* Bit positions in bsp_gpio_read_keys() result */
#define KEYMASK_POS_KEY3        (2U)
#define KEYMASK_POS_KEY4        (3U)
#define KEY1_MODE_MASK          (3UL << (KEY1_PIN * 2U))
#define KEY1_PULL_UP            (1UL << (KEY1_PIN * 2U))
#define PORTB_KEYS_MODE_MASK    ((3UL << (KEY2_PIN * 2U)) | (3UL << (KEY3_PIN * 2U)) | (3UL << (KEY4_PIN * 2U)))
#define PORTB_KEYS_PULL_UP      ((1UL << (KEY2_PIN * 2U)) | (1UL << (KEY3_PIN * 2U)) | (1UL << (KEY4_PIN * 2U)))
#define JOY_SW_MODE_MASK        (3UL << (JOY_SW_PIN * 2U))
#define JOY_SW_PULL_UP          (1UL << (JOY_SW_PIN * 2U))

static volatile bool g_b_joy_sw_exti_flag = false;

void bsp_gpio_init(void)
{
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN);
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* 4 LEDs (active high): PA5 blue, PA6 red, PA7 yellow, PB6 green - push-pull outputs, start OFF */
    GPIOA->BSRR = (PORTA_LEDS_BITS << BSRR_RESET_SHIFT);
    GPIOB->BSRR = (PORTB_LED_BIT << BSRR_RESET_SHIFT);
    GPIOA->MODER = (GPIOA->MODER & ~PORTA_LEDS_MODE_MASK) | PORTA_LEDS_OUTPUT_MODE;
    GPIOB->MODER = (GPIOB->MODER & ~PORTB_LED_MODE_MASK) | PORTB_LED_OUTPUT_MODE;
    GPIOA->OTYPER &= ~PORTA_LEDS_BITS;
    GPIOB->OTYPER &= ~PORTB_LED_BIT;
    GPIOA->PUPDR &= ~PORTA_LEDS_MODE_MASK;
    GPIOB->PUPDR &= ~PORTB_LED_MODE_MASK;

    /* Inputs with pull-up: PA10 (Key 1), PB3/PB5/PB4 (Keys 2-4), PC2 (Joystick SW) */
    GPIOA->MODER &= ~KEY1_MODE_MASK;
    GPIOA->PUPDR = (GPIOA->PUPDR & ~KEY1_MODE_MASK) | KEY1_PULL_UP;
    GPIOB->MODER &= ~PORTB_KEYS_MODE_MASK;
    GPIOB->PUPDR = (GPIOB->PUPDR & ~PORTB_KEYS_MODE_MASK) | PORTB_KEYS_PULL_UP;
    GPIOC->MODER &= ~JOY_SW_MODE_MASK;
    GPIOC->PUPDR = (GPIOC->PUPDR & ~JOY_SW_MODE_MASK) | JOY_SW_PULL_UP;

    /* EXTI Line 2 on PC2 (Joystick SW): falling edge (press, active low) */
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~EXTICR1_LINE2_MASK) | EXTICR1_LINE2_PORTC;
    EXTI->IMR  |= EXTI_LINE_2_MASK;
    EXTI->FTSR |= EXTI_LINE_2_MASK;
    EXTI->RTSR &= ~EXTI_LINE_2_MASK;
    NVIC_SetPriority(EXTI2_IRQn, EXTI2_NVIC_PRIORITY);
    NVIC_EnableIRQ(EXTI2_IRQn);
}

/* Raw key bitmask, pressed = 1 (Bit 0: Key 1, Bit 1: Key 2, Bit 2: Key 3, Bit 3: Key 4) */
uint8_t bsp_gpio_read_keys(void)
{
    uint32_t u4t_a = ~GPIOA->IDR;    /* Active low: invert so pressed = 1 */
    uint32_t u4t_b = ~GPIOB->IDR;

    return (uint8_t)(((u4t_a >> KEY1_PIN) & 1U) |
                     (((u4t_b >> KEY2_PIN) & 1U) << KEYMASK_POS_KEY2) |
                     (((u4t_b >> KEY3_PIN) & 1U) << KEYMASK_POS_KEY3) |
                     (((u4t_b >> KEY4_PIN) & 1U) << KEYMASK_POS_KEY4));
}

bool bsp_gpio_read_joystick_switch(void)
{
    return ((GPIOC->IDR & (1UL << JOY_SW_PIN)) == 0U);
}

/* Turn one LED on or off through its port's BSRR (low half = set pin, high half = reset pin) */
static void gpio_led_write(GPIO_TypeDef *p_port, uint32_t u4t_pin, bool b_on)
{
    if (b_on == true)
    {
        p_port->BSRR = (1UL << u4t_pin);
    }
    else
    {
        p_port->BSRR = (1UL << (u4t_pin + BSRR_RESET_SHIFT));
    }
}

/* Set all 4 LEDs from a mask (LED_MASK_*): a set bit turns its LED on, a cleared bit turns it off */
void bsp_gpio_leds_set(uint8_t u1t_mask)
{
    gpio_led_write(GPIOA, LED_BLUE_PIN, ((u1t_mask & LED_MASK_BLUE) != 0U));
    gpio_led_write(GPIOA, LED_RED_PIN, ((u1t_mask & LED_MASK_RED) != 0U));
    gpio_led_write(GPIOA, LED_YELLOW_PIN, ((u1t_mask & LED_MASK_YELLOW) != 0U));
    gpio_led_write(GPIOB, LED_GREEN_PIN, ((u1t_mask & LED_MASK_GREEN) != 0U));
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
    if ((EXTI->PR & EXTI_LINE_2_MASK) != 0U)
    {
        EXTI->PR = EXTI_LINE_2_MASK;    /* Write 1 to clear pending */
        g_b_joy_sw_exti_flag = true;
    }
    else
    {
        /* No action required */
    }
}
