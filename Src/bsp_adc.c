/*******************************************************************************
 * File Name   : bsp_adc.c
 * Description : Board Support Package - 3-Channel Timer-Triggered ADC Driver
 *               - Channel 4  : PA4 (Volume Potentiometer)
 *               - Channel 10 : PC0 (HW-504 Joystick VRx)
 *               - Channel 11 : PC1 (HW-504 Joystick VRy)
 *               Hardware-Autonomous Architecture (Zero CPU Overhead):
 *               TIM3 TRGO (1 kHz) -> ADC1 Scan Sequence -> DMA2 Stream 0 Circular Buffer
 * Target MCU  : STM32F411RET6
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_adc.h"
#define STM32F411xE
#include "stm32f4xx.h"

/* Named Constants (Rule 5 & Rule 10) */
#define ADC_VOL_MUTE_THRESHOLD      (80U)
#define ADC_MAX_VALUE               (4095U)
#define PERCENT_MAX                 (100U)

#define ADC_NUM_CHANNELS            (3U)
#define ADC_CH_POT_INDEX            (0U)
#define ADC_CH_X_INDEX              (1U)
#define ADC_CH_Y_INDEX              (2U)

#define ADC_CH4_PA4                 (4U)
#define ADC_CH10_PC0                (10U)
#define ADC_CH11_PC1                (11U)

#define PIN_PA4                     (4U)
#define PIN_PC0                     (0U)
#define PIN_PC1                     (1U)

#define ADC_SAMPLING_TIME_480_CYC   (7UL)

/* DMA Circular Buffer populated automatically by DMA2 Stream 0 */
static volatile uint16_t g_u2t_adc_dma_buffer[ADC_NUM_CHANNELS] = {2048U, 2048U, 2048U};

/* Private Function Prototypes (Rule 11) */
static void adc_dma_init(void);

/* Initialize DMA2 Stream 0 Channel 0 for ADC1 */
static void adc_dma_init(void)
{
    /* 1. Enable DMA2 Peripheral Clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    /* 2. Disable DMA2 Stream 0 before configuration */
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while ((DMA2_Stream0->CR & DMA_SxCR_EN) != 0U)
    {
        /* Wait until stream is disabled */
    }

    /* 3. Clear all pending interrupt flags for Stream 0 */
    DMA2->LIFCR = (DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 |
                   DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0);

    /* 4. Configure Source (ADC1 Data Register) and Destination (RAM Buffer) */
    DMA2_Stream0->PAR = (uint32_t)(&(ADC1->DR));
    DMA2_Stream0->M0AR = (uint32_t)g_u2t_adc_dma_buffer;

    /* 5. Set Number of Data Items (3 conversions: CH4, CH10, CH11) */
    DMA2_Stream0->NDTR = ADC_NUM_CHANNELS;

    /* 6. Configure DMA Stream 0 Control Register:
     *    - Channel 0 (ADC1): (0UL << DMA_SxCR_CHSEL_Pos)
     *    - Priority High: DMA_SxCR_PL_1
     *    - Memory Increment: DMA_SxCR_MINC
     *    - Peripheral Increment: 0 (fixed to ADC1->DR)
     *    - Direction: Peripheral-to-Memory (0)
     *    - Peripheral Data Size: 16-bit Half-Word (DMA_SxCR_PSIZE_0)
     *    - Memory Data Size: 16-bit Half-Word (DMA_SxCR_MSIZE_0)
     *    - Circular Mode: DMA_SxCR_CIRC
     */
    DMA2_Stream0->CR = (DMA_SxCR_PL_1 |
                        DMA_SxCR_MINC |
                        DMA_SxCR_PSIZE_0 |
                        DMA_SxCR_MSIZE_0 |
                        DMA_SxCR_CIRC);

    /* 7. Direct Mode (FIFO disabled) */
    DMA2_Stream0->FCR = 0U;

    /* 8. Enable DMA2 Stream 0 */
    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

void bsp_adc_init(void)
{
    /* 1. Enable Clocks for GPIOA, GPIOC and ADC1 */
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN);
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* 2. Configure PA4 in Analog Mode (0b11) and No-Pull */
    GPIOA->MODER |= (3UL << (PIN_PA4 * 2U));
    GPIOA->PUPDR &= ~(3UL << (PIN_PA4 * 2U));

    /* 3. Configure PC0 and PC1 in Analog Mode (0b11) and No-Pull */
    GPIOC->MODER |= ((3UL << (PIN_PC0 * 2U)) | (3UL << (PIN_PC1 * 2U)));
    GPIOC->PUPDR &= ~((3UL << (PIN_PC0 * 2U)) | (3UL << (PIN_PC1 * 2U)));

    /* 4. Configure Sampling Time (480 cycles for maximum impedance stability) */
    ADC1->SMPR2 |= (ADC_SAMPLING_TIME_480_CYC << (ADC_CH4_PA4 * 3U));
    ADC1->SMPR1 |= ((ADC_SAMPLING_TIME_480_CYC << 0U) | (ADC_SAMPLING_TIME_480_CYC << 3U));

    /* 5. Configure ADC Common Prescaler: PCLK2 / 4 */
    ADC->CCR &= ~ADC_CCR_ADCPRE;
    ADC->CCR |= ADC_CCR_ADCPRE_0;

    /* 6. Configure Regular Sequence: 3 Conversions
     *    Length L = 3 - 1 = 2 (L[3:0] = 0b0010)
     *    1st: Channel 4  (PA4 - Volume)
     *    2nd: Channel 10 (PC0 - Joystick X)
     *    3rd: Channel 11 (PC1 - Joystick Y)
     */
    ADC1->SQR1 &= ~ADC_SQR1_L;
    ADC1->SQR1 |= (2UL << ADC_SQR1_L_Pos);

    ADC1->SQR3 = ((ADC_CH4_PA4  << 0U) |
                  (ADC_CH10_PC0 << 5U) |
                  (ADC_CH11_PC1 << 10U));

    /* 7. Enable Multi-Channel Scan Mode */
    ADC1->CR1 |= ADC_CR1_SCAN;

    /* 8. Configure Timer-Triggered Sampling (TIM3 TRGO)
     *    - Continuous Conversion Mode: Disabled (TIM3 determines each sample rate)
     *    - External Trigger Source: TIM3 TRGO (EXTSEL = 0b1000 = 8)
     *    - External Trigger Edge: Rising Edge (EXTEN = 0b01 = 1)
     *    - DMA Mode & Continuous DMA Requests: Enabled (DMA = 1, DDS = 1)
     */
    ADC1->CR2 &= ~ADC_CR2_CONT;

    ADC1->CR2 &= ~(ADC_CR2_EXTSEL | ADC_CR2_EXTEN);
    ADC1->CR2 |= ((8UL << ADC_CR2_EXTSEL_Pos) | (1UL << ADC_CR2_EXTEN_Pos));

    ADC1->CR2 |= (ADC_CR2_DMA | ADC_CR2_DDS);

    /* 9. Configure and Arm DMA2 Stream 0 (Prior to ADC Enable) */
    adc_dma_init();

    /* 10. Enable ADC Peripheral */
    ADC1->CR2 |= ADC_CR2_ADON;
}

/* Service function maintained for interface compatibility (Zero CPU work) */
void bsp_adc_service(uint32_t u4t_now)
{
    (void)u4t_now; /* DMA transfers autonomously in hardware without CPU polling */
}

/* Retrieve raw snapshot of Joystick X and Y (atomic single-cycle reads from DMA buffer) */
bool bsp_adc_get_joystick_raw(uint16_t *p_x_raw, uint16_t *p_y_raw)
{
    bool b_success = false;

    if ((p_x_raw != (uint16_t *)0) && (p_y_raw != (uint16_t *)0))
    {
        *p_x_raw = g_u2t_adc_dma_buffer[ADC_CH_X_INDEX];
        *p_y_raw = g_u2t_adc_dma_buffer[ADC_CH_Y_INDEX];
        b_success = true;
    }
    else
    {
        b_success = false;
    }

    return b_success;
}

/* Calculate Volume percentage (0 to 100%) directly from Potentiometer DMA buffer */
uint8_t bsp_adc_get_volume_percent(void)
{
    uint8_t u1t_vol_pct = 0U;
    uint16_t u2t_pot_val = g_u2t_adc_dma_buffer[ADC_CH_POT_INDEX];
    uint32_t u4t_val = (uint32_t)u2t_pot_val;

    if (u4t_val <= ADC_VOL_MUTE_THRESHOLD)
    {
        u1t_vol_pct = 0U; /* Mute when turned to the bottom */
    }
    else
    {
        /* Rescale smoothly from MUTE_THRESHOLD..ADC_MAX_VALUE to 1..100% */
        uint32_t u4t_span = (uint32_t)(ADC_MAX_VALUE - ADC_VOL_MUTE_THRESHOLD);
        uint32_t u4t_adj = u4t_val - (uint32_t)ADC_VOL_MUTE_THRESHOLD;
        uint32_t u4t_pct = (u4t_adj * PERCENT_MAX) / u4t_span;

        if (u4t_pct > PERCENT_MAX)
        {
            u4t_pct = PERCENT_MAX;
        }
        else if (u4t_pct == 0U)
        {
            u4t_pct = 1U;
        }
        else
        {
            /* In range 1 to 100 */
        }
        u1t_vol_pct = (uint8_t)u4t_pct;
    }

    return u1t_vol_pct;
}
