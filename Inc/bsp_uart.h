/*******************************************************************************
 * File Name   : bsp_uart.h
 * Description : USART2 Driver - 100% Interrupt-Driven TX & RX (Zero Polling)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>

void bsp_uart_init(void);
void bsp_uart_send_string(const char *p_str);
void bsp_uart_send_dec(uint32_t u4t_val);
bool bsp_uart_read_char(char *p_c);

#endif /* BSP_UART_H */
