/*******************************************************************************
 * File Name   : bsp_adc.h
 * Description : Board Support Package - 3-Channel Timer-Triggered ADC Driver
 *               Hardware-Autonomous Architecture (Zero CPU Overhead):
 *               TIM3 TRGO (1 kHz) -> ADC1 Scan Sequence -> DMA2 Stream 0 Circular
 * Target MCU  : STM32F411RET6
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>
#include <stdbool.h>

void bsp_adc_init(void);
uint8_t bsp_adc_get_volume_percent(void);
bool bsp_adc_get_joystick_raw(uint16_t *p_x_raw, uint16_t *p_y_raw);

#endif /* BSP_ADC_H */
