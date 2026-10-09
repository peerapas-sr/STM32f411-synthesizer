/*******************************************************************************
 * File Name   : bsp_buzzer.h
 * Description : Board Support Package - Hardware PWM Buzzer Driver (PB7 / TIM4_CH2)
 * Target MCU  : STM32F411RET6 (Nucleo-F411RE)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H

#include <stdint.h>

void bsp_buzzer_init(void);
void bsp_buzzer_set_tone(uint32_t u4t_freq_hz, uint16_t u2t_vol_adc);
void bsp_buzzer_off(void);

#endif /* BSP_BUZZER_H */
