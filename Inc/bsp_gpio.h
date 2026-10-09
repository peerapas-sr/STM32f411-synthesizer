/*******************************************************************************
 * File Name   : bsp_gpio.h
 * Description : 4 Push Buttons, Joystick Switch & 4 Board LEDs
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>

/* Pin map
 *   PA10 (D2) Key 1: Do / Sol         PA5 Blue LED (top)     - Key 1
 *   PB3  (D3) Key 2: Re / La          PA6 Red LED            - Key 2 / Recording
 *   PB5  (D4) Key 3: Mi / Ti          PA7 Yellow LED         - Key 3
 *   PB4  (D5) Key 4: Fa / High Do     PB6 Green LED (bottom) - Key 4
 *   PC2  (A2) Joystick center push button + EXTI2
 */

/* bsp_gpio_read_keys() result: bit n = Key n+1 pressed */
#define KEY_MASK_1        (0x01U)
#define KEY_MASK_2        (0x02U)
#define KEY_MASK_3        (0x04U)
#define KEY_MASK_4        (0x08U)

/* bsp_gpio_leds_set() mask: bit n = LED of Key n+1 */
#define LED_MASK_BLUE     (0x01U)
#define LED_MASK_RED      (0x02U)
#define LED_MASK_YELLOW   (0x04U)
#define LED_MASK_GREEN    (0x08U)

void bsp_gpio_init(void);
uint8_t bsp_gpio_read_keys(void);
bool bsp_gpio_read_joystick_switch(void);
void bsp_gpio_leds_set(uint8_t u1t_mask);
bool bsp_gpio_get_exti_flag(void);
void bsp_gpio_clear_exti_flag(void);

#endif /* BSP_GPIO_H */
