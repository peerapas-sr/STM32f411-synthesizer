/*******************************************************************************
 * File Name   : bsp_gpio.h
 * Description : 4 Push Buttons, Joystick Switch & 4 Board LEDs
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
#define JOY_SW_PIN        (2UL)  /* PC2  (A2) Joystick center push button + EXTI2 */
#define LED_BLUE_PIN      (5UL)  /* PA5  Blue LED (top)    - Key 1 */
#define LED_RED_PIN       (6UL)  /* PA6  Red LED           - Key 2 / Recording */
#define LED_YELLOW_PIN    (7UL)  /* PA7  Yellow LED        - Key 3 */
#define LED_GREEN_PIN     (6UL)  /* PB6  Green LED (bottom) - Key 4 */

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
