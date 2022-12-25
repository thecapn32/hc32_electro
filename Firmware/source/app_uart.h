#ifndef APP_UART_H_
#define APP_UART_H_
void uart_send_ok(void);
void uart_sn_print(void);
void uart_send_test(int i);
uint8_t uart_read(void);
void uart_sn_value(uint8_t *val);
#endif