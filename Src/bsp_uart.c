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
#define UART_PINS_MODE_MASK     ((3UL << 4U) | (3UL << 6U))      /* PA2, PA3 */
#define UART_PINS_AF_MODE       ((2UL << 4U) | (2UL << 6U))
#define UART_PINS_PULL_UP       ((1UL << 4U) | (1UL << 6U))
#define UART_PINS_AF_MASK       ((0xFUL << 8U) | (0xFUL << 12U))
#define UART_PINS_AF7           ((7UL << 8U) | (7UL << 12U))

static volatile char     g_c_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t  g_u1t_rx_head = 0U;
static volatile uint8_t  g_u1t_rx_tail = 0U;
static volatile char     g_c_tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint16_t g_u2t_tx_head = 0U;
static volatile uint16_t g_u2t_tx_tail = 0U;

void bsp_uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2 (TX) / PA3 (RX): AF7, high speed, pull-up */
    GPIOA->MODER = (GPIOA->MODER & ~UART_PINS_MODE_MASK) | UART_PINS_AF_MODE;
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~UART_PINS_AF_MASK) | UART_PINS_AF7;
    GPIOA->OSPEEDR |= UART_PINS_MODE_MASK;
    GPIOA->PUPDR = (GPIOA->PUPDR & ~UART_PINS_MODE_MASK) | UART_PINS_PULL_UP;

    /* 115200 8N1, TX + RX enabled, RXNE interrupt (TXE interrupt enabled on demand) */
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
    while (u4t_rem > 0U)
    {
        c_buf[u1t_len] = (char)('0' + (u4t_rem % 10U));
        u4t_rem /= 10U;
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
    return b_have;
}

/* USART2 ISR: RXNE -> push into RX ring, TXE -> pop from TX ring (disable TXE when empty) */
void USART2_IRQHandler(void)
{
    if ((USART2->SR & USART_SR_RXNE) != 0U)
    {
        char c_rx = (char)(USART2->DR & 0xFFU);
        uint8_t u1t_next = (uint8_t)((g_u1t_rx_head + 1U) % UART_RX_BUFFER_SIZE);
        if (u1t_next != g_u1t_rx_tail)
        {
            g_c_rx_buffer[g_u1t_rx_head] = c_rx;
            g_u1t_rx_head = u1t_next;
        }
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
}
