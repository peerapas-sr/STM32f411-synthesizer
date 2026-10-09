/*******************************************************************************
 * File Name   : bsp_reg_fields.h
 * Description : Shared register field values, written in binary as the Reference Manual (RM0383) lists them
 *               Usage: REG &= ~FIELD;  REG |= (VALUE << FIELD_Pos);
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_REG_FIELDS_H
#define BSP_REG_FIELDS_H

/* GPIOx_MODER: 00 = input, 01 = output, 10 = alternate function, 11 = analog */
#define GPIO_MODE_OUTPUT        (0b01U)
#define GPIO_MODE_AF            (0b10U)
#define GPIO_MODE_ANALOG        (0b11U)

/* GPIOx_OSPEEDR: 00 = low, 01 = medium, 10 = fast, 11 = very high */
#define GPIO_SPEED_VERY_HIGH    (0b11U)

/* GPIOx_PUPDR: 00 = no pull, 01 = pull-up, 10 = pull-down */
#define GPIO_PULL_UP            (0b01U)

/* GPIOx_AFRL / AFRH: alternate function number (STM32F411 datasheet, Table 9) */
#define GPIO_AF2_TIM4           (0b0010U)    /* PB7 = TIM4_CH2 */
#define GPIO_AF4_I2C1           (0b0100U)    /* PB8 = I2C1_SCL, PB9 = I2C1_SDA */
#define GPIO_AF7_USART2         (0b0111U)    /* PA2 = USART2_TX, PA3 = USART2_RX */

/* DMA_SxCR */
#define DMA_CHANNEL_1           (0b001U)     /* CHSEL: DMA1 Stream 6 channel 1 = I2C1_TX */
#define DMA_PRIORITY_HIGH       (0b10U)      /* PL: 00 = low, 01 = medium, 10 = high, 11 = very high */
#define DMA_SIZE_16BIT          (0b01U)      /* PSIZE / MSIZE: 00 = byte, 01 = half-word, 10 = word */
#define DMA_DIR_MEM_TO_PERIPH   (0b01U)      /* DIR: 00 = peripheral-to-memory, 01 = memory-to-peripheral */

#endif /* BSP_REG_FIELDS_H */
