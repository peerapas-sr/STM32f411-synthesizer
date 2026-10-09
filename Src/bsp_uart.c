/*******************************************************************************
 * File Name   : bsp_uart.c
 * Description : USART2 Driver - 100% Interrupt-Driven TX & RX (Zero Polling)
 * Target MCU  : STM32F411RET6
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_uart.h"
#ifndef STM32F411xE
#define STM32F411xE
#endif
#include "stm32f4xx.h"

/* Named Constants (Rule 5 & Rule 10) */
#define UART_RX_BUFFER_SIZE     (64U)
#define UART_TX_BUFFER_SIZE     (256U)
#define USART2_BRR_115200       (139U)
#define USART2_NVIC_PRIORITY    (2U)

/* Circular RX buffer managed by Interrupt Service Routine */
static volatile char     g_u1t_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t  g_u1t_rx_head = 0U;
static volatile uint8_t  g_u1t_rx_tail = 0U;

/* Circular TX buffer managed by Interrupt Service Routine */
static volatile char     g_u1t_tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint16_t g_u2t_tx_head = 0U;
static volatile uint16_t g_u2t_tx_tail = 0U;

void bsp_uart_init(void)
{
    /* 1. Enable Clocks for GPIOA and USART2 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* 2. Configure PA2 (TX) and PA3 (RX) as Alternate Function AF7 */
    GPIOA->MODER &= ~((3UL << (2U * 2U)) | (3UL << (3U * 2U)));
    GPIOA->MODER |=  ((2UL << (2U * 2U)) | (2UL << (3U * 2U))); /* AF mode */

    GPIOA->AFR[0] &= ~((0xFUL << (2U * 4U)) | (0xFUL << (3U * 4U)));
    GPIOA->AFR[0] |=  ((7UL   << (2U * 4U)) | (7UL   << (3U * 4U))); /* AF7 (USART2) */

    GPIOA->OSPEEDR |= ((3UL << (2U * 2U)) | (3UL << (3U * 2U)));
    GPIOA->PUPDR   &= ~((3UL << (2U * 2U)) | (3UL << (3U * 2U)));
    GPIOA->PUPDR   |=  ((1UL << (2U * 2U)) | (1UL << (3U * 2U))); /* Pull-up */

    /* 3. Configure Baud Rate = 115200 at 16 MHz APB1 clock (BRR = 139) */
    USART2->BRR = USART2_BRR_115200;

    /* 4. Enable Transmitter, Receiver, and RXNE Interrupt */
    USART2->CR1 = (USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE);
    USART2->CR1 |= USART_CR1_UE; /* Enable USART */

    /* 5. Configure NVIC for USART2 Interrupt */
    NVIC_SetPriority(USART2_IRQn, USART2_NVIC_PRIORITY);
    NVIC_EnableIRQ(USART2_IRQn);
}

/* Transmit a single character via TX Ring Buffer and TXE Interrupt (Zero Polling) */
void bsp_uart_send_char(char c_val)
{
    uint16_t u2t_next_head = (uint16_t)((g_u2t_tx_head + 1U) % UART_TX_BUFFER_SIZE);

    /* Enqueue character if space is available */
    if (u2t_next_head != g_u2t_tx_tail)
    {
        g_u1t_tx_buffer[g_u2t_tx_head] = c_val;
        g_u2t_tx_head = u2t_next_head;

        /* Enable TXE interrupt: ISR will shift byte out to USART_DR automatically */
        USART2->CR1 |= USART_CR1_TXEIE;
    }
    else
    {
        /* Buffer full: drop byte to prevent corruption without blocking CPU */
    }
}

/* Transmit null-terminated string */
void bsp_uart_send_string(const char *p_str)
{
    if (p_str != (const char *)0)
    {
        const char *p_curr = p_str;
        while (*p_curr != '\0')
        {
            bsp_uart_send_char(*p_curr);
            p_curr++;
        }
    }
    else
    {
        /* Null pointer passed */
    }
}

/* Check if character available in RX buffer */
bool bsp_uart_has_rx_char(void)
{
    return (g_u1t_rx_head != g_u1t_rx_tail);
}

/* Read character from RX buffer */
char bsp_uart_get_rx_char(void)
{
    char c_char = '\0';
    if (g_u1t_rx_head != g_u1t_rx_tail)
    {
        c_char = g_u1t_rx_buffer[g_u1t_rx_tail];
        g_u1t_rx_tail = (uint8_t)((g_u1t_rx_tail + 1U) % UART_RX_BUFFER_SIZE);
    }
    else
    {
        /* Buffer empty */
    }
    return c_char;
}

/* USART2 Interrupt Service Routine: 100% Interrupt-Driven RX and TX */
void USART2_IRQHandler(void)
{
    /* 1. Handle Receive Data Register Not Empty (RXNE) */
    if ((USART2->SR & USART_SR_RXNE) != 0U)
    {
        char c_rx_byte = (char)(USART2->DR & 0xFFU);
        uint8_t u1t_next_head = (uint8_t)((g_u1t_rx_head + 1U) % UART_RX_BUFFER_SIZE);

        /* Prevent buffer overflow */
        if (u1t_next_head != g_u1t_rx_tail)
        {
            g_u1t_rx_buffer[g_u1t_rx_head] = c_rx_byte;
            g_u1t_rx_head = u1t_next_head;
        }
        else
        {
            /* Buffer full: drop byte to prevent corruption */
        }
    }
    else
    {
        /* No RX event */
    }

    /* 2. Handle Transmit Data Register Empty (TXE) */
    if (((USART2->SR & USART_SR_TXE) != 0U) && ((USART2->CR1 & USART_CR1_TXEIE) != 0U))
    {
        if (g_u2t_tx_head != g_u2t_tx_tail)
        {
            USART2->DR = (uint16_t)((uint8_t)g_u1t_tx_buffer[g_u2t_tx_tail]);
            g_u2t_tx_tail = (uint16_t)((g_u2t_tx_tail + 1U) % UART_TX_BUFFER_SIZE);
        }
        else
        {
            /* Buffer empty: disable TXE interrupt */
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    else
    {
        /* No TX event */
    }
}
