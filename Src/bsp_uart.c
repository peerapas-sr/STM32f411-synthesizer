/*******************************************************************************
 * File Name   : bsp_uart.c
 * Description : USART2 (PA2 TX / PA3 RX, 115200 bps) - 100% Interrupt-Driven TX & RX
 *               Ring buffers filled/drained by USART2_IRQHandler (RXNE / TXE): Zero Polling
 * Target MCU  : STM32F411RET6
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_uart.h"
#define STM32F411xE
#include "stm32f4xx.h"

/* Named Constants (Rule 5 & Rule 10) */
#define UART_RX_BUFFER_SIZE     (64U)
#define UART_TX_BUFFER_SIZE     (256U)
#define USART2_BRR_115200       (139U)     /* 16 MHz / 115200 */
#define USART2_NVIC_PRIORITY    (2U)
#define UART_DEC_MAX_DIGITS     (10U)
#define UART_DECIMAL_BASE       (10U)
#define UART_DR_DATA_MASK       (0xFFU)    /* 8 data bits */
#define GPIO_AF7_USART2         (7UL)      /* AF7 = USART2 on PA2 / PA3 */

static volatile char     g_c_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t  g_u1t_rx_head = 0U;
static volatile uint8_t  g_u1t_rx_tail = 0U;
static volatile char     g_c_tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint16_t g_u2t_tx_head = 0U;
static volatile uint16_t g_u2t_tx_tail = 0U;

void bsp_uart_init(void)
{
    /* --- Setup peripheral clock --- */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* --- Setup GPIO PA2 (TX), PA3 (RX): AF7, very high speed, pull-up --- */
    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOA->MODER |= (GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1);    /* 10 = alternate function */
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
    GPIOA->AFR[0] |= ((GPIO_AF7_USART2 << GPIO_AFRL_AFSEL2_Pos) | (GPIO_AF7_USART2 << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->OSPEEDR |= (GPIO_OSPEEDR_OSPEED2 | GPIO_OSPEEDR_OSPEED3);    /* 11 = very high speed */
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD2 | GPIO_PUPDR_PUPD3);
    GPIOA->PUPDR |= (GPIO_PUPDR_PUPD2_0 | GPIO_PUPDR_PUPD3_0);    /* 01 = pull-up */

    /* --- Setup USART2: 115200 8N1, TX + RX enabled, RXNE interrupt (TXE interrupt enabled on demand) --- */
    USART2->BRR = USART2_BRR_115200;
    USART2->CR1 = (USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE);
    NVIC_SetPriority(USART2_IRQn, USART2_NVIC_PRIORITY);
    NVIC_EnableIRQ(USART2_IRQn);
}

/* Queue one character; the TXE interrupt shifts it out. Drops the byte if the buffer is full. */
static void uart_send_char(char c_val)
{
    uint16_t u2t_next = (uint16_t)((g_u2t_tx_head + 1U) % UART_TX_BUFFER_SIZE);

    if (u2t_next != g_u2t_tx_tail)
    {
        g_c_tx_buffer[g_u2t_tx_head] = c_val;
        g_u2t_tx_head = u2t_next;
        USART2->CR1 |= USART_CR1_TXEIE;
    }
    else
    {
        /* No action required */
    }
}

void bsp_uart_send_string(const char *p_str)
{
    const char *p_ch = p_str;

    while (*p_ch != '\0')
    {
        uart_send_char(*p_ch);
        p_ch++;
    }
}

/* Unsigned decimal: digits are produced least-significant first, then sent in reverse */
void bsp_uart_send_dec(uint32_t u4t_val)
{
    char c_buf[UART_DEC_MAX_DIGITS];
    uint32_t u4t_rem = u4t_val;
    uint8_t u1t_len = 0U;

    if (u4t_rem == 0U)
    {
        uart_send_char('0');
    }
    else
    {
        /* No action required */
    }
    while (u4t_rem > 0U)
    {
        c_buf[u1t_len] = (char)('0' + (u4t_rem % UART_DECIMAL_BASE));
        u4t_rem /= UART_DECIMAL_BASE;
        u1t_len++;
    }
    while (u1t_len > 0U)
    {
        u1t_len--;
        uart_send_char(c_buf[u1t_len]);
    }
}

/* Pop one received character; returns false when the RX buffer is empty */
bool bsp_uart_read_char(char *p_c)
{
    bool b_have = (g_u1t_rx_head != g_u1t_rx_tail);

    if (b_have == true)
    {
        *p_c = g_c_rx_buffer[g_u1t_rx_tail];
        g_u1t_rx_tail = (uint8_t)((g_u1t_rx_tail + 1U) % UART_RX_BUFFER_SIZE);
    }
    else
    {
        /* No action required */
    }
    return b_have;
}

/* USART2 ISR: RXNE -> push into RX ring, TXE -> pop from TX ring (disable TXE when empty) */
void USART2_IRQHandler(void)
{
    if ((USART2->SR & USART_SR_RXNE) != 0U)
    {
        char c_rx = (char)(USART2->DR & UART_DR_DATA_MASK);
        uint8_t u1t_next = (uint8_t)((g_u1t_rx_head + 1U) % UART_RX_BUFFER_SIZE);
        if (u1t_next != g_u1t_rx_tail)
        {
            g_c_rx_buffer[g_u1t_rx_head] = c_rx;
            g_u1t_rx_head = u1t_next;
        }
        else
        {
            /* No action required */
        }
    }
    else
    {
        /* No action required */
    }

    if (((USART2->SR & USART_SR_TXE) != 0U) && ((USART2->CR1 & USART_CR1_TXEIE) != 0U))
    {
        if (g_u2t_tx_head != g_u2t_tx_tail)
        {
            USART2->DR = (uint16_t)((uint8_t)g_c_tx_buffer[g_u2t_tx_tail]);
            g_u2t_tx_tail = (uint16_t)((g_u2t_tx_tail + 1U) % UART_TX_BUFFER_SIZE);
        }
        else
        {
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    else
    {
        /* No action required */
    }
}
