/*******************************************************************************
 * File Name   : bsp_adc.h
 * Description : 3-Channel ADC1 (TIM3 TRGO 1 kHz -> scan -> DMA2 Stream 0 circular)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>

/* Scan slot indexes into the DMA buffer */
#define ADC_IDX_VOLUME      (0U)    /* PA4 potentiometer */
#define ADC_IDX_JOY_X       (1U)    /* PC0 VRx */
#define ADC_IDX_JOY_Y       (2U)    /* PC1 VRy */

void bsp_adc_init(void);
uint16_t bsp_adc_get_raw(uint8_t u1t_idx);
uint8_t bsp_adc_get_volume_percent(void);

#endif /* BSP_ADC_H */
