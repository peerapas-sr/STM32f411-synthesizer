/*******************************************************************************
 * File Name   : bsp_gpio.h
 * Description : Board Support Package - 4 Push Buttons, Switch & Red LED
 *               - Key 1: PA10 (D2) [Key 1: Do / Sol] with EXTI Line 10 Interrupt
 *               - Key 2: PB3  (D3) [Key 2: Re / La]
 *               - Key 3: PB5  (D4) [Key 3: Mi / Ti]
 *               - Key 4: PB4  (D5) [Key 4: Fa / High Do]
 *               - SW   : PC2  (A2) [HW-504 Joystick Center Push Switch]
 *               - LED  : PA6  (D12) [Red Indicator LED]
 * Target MCU  : STM32F411RET6 (Nucleo-F411RE)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>

/* Hardware Pin Definitions */
#define KEY1_PIN          (10UL) /* PA10 (D2) [Key 1: Do / Sol] + EXTI10 */
#define KEY2_PIN          (3UL)  /* PB3  (D3) [Key 2: Re / La] */
#define KEY3_PIN          (5UL)  /* PB5  (D4) [Key 3: Mi / Ti] */
#define KEY4_PIN          (4UL)  /* PB4  (D5) [Key 4: Fa / High Do] */
#define LED_RED_PIN       (6UL)  /* PA6  (D12) - Red Status LED */
#define JOY_SW_PIN        (2UL)  /* PC2  (A2)  - Joystick Center Push Button */

/* Public API Functions */
void bsp_gpio_init(void);

uint8_t bsp_gpio_read_keys(void);
bool bsp_gpio_read_joystick_switch(void);

void bsp_gpio_led_red_set(bool b_state);

bool bsp_gpio_get_exti_flag(void);
void bsp_gpio_clear_exti_flag(void);

#endif /* BSP_GPIO_H */
