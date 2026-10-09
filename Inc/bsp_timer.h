/*******************************************************************************
 * File Name   : bsp_timer.h
 * Description : Board Support Package - TIM3 Hardware Timer Driver & Delay Utilities
 * Target MCU  : STM32F411RET6
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include <stdint.h>

void bsp_timer_init(void);
uint32_t bsp_timer_get_ms(void);
void bsp_delay_us(uint32_t u4t_us);
void bsp_delay_ms(uint32_t u4t_ms);

#endif /* BSP_TIMER_H */
