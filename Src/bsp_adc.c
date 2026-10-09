/*******************************************************************************
 * File Name   : bsp_adc.c
 * Description : 3-Channel Timer-Triggered ADC (Zero CPU Polling)
 *               TIM3 TRGO (1 kHz) -> ADC1 scan [PA4 Vol, PC0 VRx, PC1 VRy] -> DMA2 Stream 0 circular
 * Target MCU  : STM32F411RET6
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_adc.h"
#define STM32F411xE
#include "stm32f4xx.h"

/* Named Constants (Rule 5 & Rule 10) */
#define ADC_NUM_CHANNELS            (3U)
#define ADC_CH4_PA4                 (4UL)
#define ADC_CH10_PC0                (10UL)
#define ADC_CH11_PC1                (11UL)
#define ADC_SMP_480_CYCLES          (7UL)
#define ADC_EXTSEL_TIM3_TRGO        (8UL)
#define ADC_EXTEN_RISING            (1UL)
#define ADC_VOL_MUTE_THRESHOLD      (80U)
#define ADC_MAX_VALUE               (4095U)
#define PERCENT_MAX                 (100U)
#define ADC_MID_SCALE               (2048U)    /* Joystick center before the first conversion */
#define ADC_SMPR2_CH4_POS           (12U)      /* SMPR2 bits 14:12 = channel 4 */
#define ADC_SMPR1_CH10_POS          (0U)       /* SMPR1 bits 2:0   = channel 10 */
#define ADC_SMPR1_CH11_POS          (3U)       /* SMPR1 bits 5:3   = channel 11 */
#define ADC_SQR3_RANK2_POS          (5U)
#define ADC_SQR3_RANK3_POS          (10U)
#define PA4_2BIT_MASK               (3UL << 8U)
#define PA4_MODE_ANALOG             (3UL << 8U)                     /* MODER = 11: analog */
#define PC0_PC1_2BIT_MASK           ((3UL << 0U) | (3UL << 2U))
#define PC0_PC1_MODE_ANALOG         ((3UL << 0U) | (3UL << 2U))

/* Filled continuously by DMA2 Stream 0 (index order = ADC scan order) */
static volatile uint16_t g_u2t_adc_dma_buffer[ADC_NUM_CHANNELS] = {ADC_MID_SCALE, ADC_MID_SCALE, ADC_MID_SCALE};

void bsp_adc_init(void)
{
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_DMA2EN);
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* PA4, PC0, PC1: analog mode, no pull */
    GPIOA->MODER |= PA4_MODE_ANALOG;
    GPIOA->PUPDR &= ~PA4_2BIT_MASK;
    GPIOC->MODER |= PC0_PC1_MODE_ANALOG;
    GPIOC->PUPDR &= ~PC0_PC1_2BIT_MASK;

    /* DMA2 Stream 0 Channel 0: ADC1->DR to buffer, 16-bit, memory increment, circular */
    DMA2_Stream0->CR = 0U;    /* Stream is idle after reset: configure directly */
    DMA2->LIFCR = (DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0);
    DMA2_Stream0->PAR = (uint32_t)(&(ADC1->DR));
    DMA2_Stream0->M0AR = (uint32_t)g_u2t_adc_dma_buffer;
    DMA2_Stream0->NDTR = ADC_NUM_CHANNELS;
    DMA2_Stream0->FCR = 0U;
    DMA2_Stream0->CR = (DMA_SxCR_PL_1 | DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 | DMA_SxCR_CIRC);
    DMA2_Stream0->CR |= DMA_SxCR_EN;

    /* ADC1: PCLK2/4, 480-cycle sampling, scan CH4 -> CH10 -> CH11, TIM3 TRGO rising edge, DMA continuous */
    ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0;
    ADC1->SMPR2 |= (ADC_SMP_480_CYCLES << ADC_SMPR2_CH4_POS);
    ADC1->SMPR1 |= ((ADC_SMP_480_CYCLES << ADC_SMPR1_CH10_POS) | (ADC_SMP_480_CYCLES << ADC_SMPR1_CH11_POS));
    ADC1->SQR1 = ((ADC_NUM_CHANNELS - 1UL) << ADC_SQR1_L_Pos);
    ADC1->SQR3 = (ADC_CH4_PA4 | (ADC_CH10_PC0 << ADC_SQR3_RANK2_POS) | (ADC_CH11_PC1 << ADC_SQR3_RANK3_POS));
    ADC1->CR1 |= ADC_CR1_SCAN;
    ADC1->CR2 = ((ADC_EXTSEL_TIM3_TRGO << ADC_CR2_EXTSEL_Pos) | (ADC_EXTEN_RISING << ADC_CR2_EXTEN_Pos) |
                 ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON);
}

/* Latest raw sample (0..4095) of a scan slot: ADC_IDX_VOLUME / ADC_IDX_JOY_X / ADC_IDX_JOY_Y */
uint16_t bsp_adc_get_raw(uint8_t u1t_idx)
{
    uint16_t u2t_val = 0U;

    if (u1t_idx < ADC_NUM_CHANNELS)
    {
        u2t_val = g_u2t_adc_dma_buffer[u1t_idx];
    }
    else
    {
        /* No action required */
    }
    return u2t_val;
}

/* Volume 0..100 % from the potentiometer (bottom 80 counts = mute) */
uint8_t bsp_adc_get_volume_percent(void)
{
    uint32_t u4t_val = (uint32_t)g_u2t_adc_dma_buffer[ADC_IDX_VOLUME];
    uint32_t u4t_pct = 0U;

    if (u4t_val > ADC_VOL_MUTE_THRESHOLD)
    {
        u4t_pct = ((u4t_val - ADC_VOL_MUTE_THRESHOLD) * PERCENT_MAX) / (ADC_MAX_VALUE - ADC_VOL_MUTE_THRESHOLD);
    }
    else
    {
        /* No action required */
    }
    return (uint8_t)u4t_pct;
}
