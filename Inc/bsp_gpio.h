/*******************************************************************************
 * File Name   : bsp_gpio.h
 * Description : 4 Push Buttons, Joystick Switch & Red LED
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#define KEY1_PIN          (10UL) /* PA10 (D2) Key 1: Do / Sol */
#define KEY2_PIN          (3UL)  /* PB3  (D3) Key 2: Re / La */
#define KEY3_PIN          (5UL)  /* PB5  (D4) Key 3: Mi / Ti */
#define KEY4_PIN          (4UL)  /* PB4  (D5) Key 4: Fa / High Do */
#define LED_RED_PIN       (6UL)  /* PA6  (D12) Red status LED */
#define JOY_SW_PIN        (2UL)  /* PC2  (A2) Joystick center push button + EXTI2 */

void bsp_gpio_init(void);
uint8_t bsp_gpio_read_keys(void);
bool bsp_gpio_read_joystick_switch(void);
void bsp_gpio_led_red_set(bool b_state);
bool bsp_gpio_get_exti_flag(void);
void bsp_gpio_clear_exti_flag(void);

#endif /* BSP_GPIO_H */
