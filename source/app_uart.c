#include "ddl.h"
#include "uart.h"
#include "app_uart.h"

const uint8_t sn[10] = {'E','n','t','e','r','S','/','N','\n','\r'};

void uart_send_ok(void)
{
    Uart_SendDataPoll(M0P_UART0,'O');
    Uart_SendDataPoll(M0P_UART0,'K');
    Uart_SendDataPoll(M0P_UART0,'\n');
    Uart_SendDataPoll(M0P_UART0,'\r');
}

void uart_sn_print(void)
{
    for (int i = 0; i < 10; i++)
    {
        Uart_SendDataPoll(M0P_UART0,sn[i]);
    }
}

void uart_sn_value(uint8_t *val)
{
    Uart_SendDataPoll(M0P_UART0,'S');
    Uart_SendDataPoll(M0P_UART0,'/');
    Uart_SendDataPoll(M0P_UART0,'N');
    Uart_SendDataPoll(M0P_UART0,':');
    Uart_SendDataPoll(M0P_UART0,' ');
    for (int i = 0; i < 8; i++)
    {
        Uart_SendDataPoll(M0P_UART0, val[i]);
    }
    Uart_SendDataPoll(M0P_UART0,'\n');
    Uart_SendDataPoll(M0P_UART0,'\r');
}

void uart_send_test(int i)
{
    Uart_SendDataPoll(M0P_UART0,'T');
    Uart_SendDataPoll(M0P_UART0,'E');
    Uart_SendDataPoll(M0P_UART0,'S');
    Uart_SendDataPoll(M0P_UART0,'T');
    Uart_SendDataPoll(M0P_UART0, i + '0');
    Uart_SendDataPoll(M0P_UART0,'\n');
    Uart_SendDataPoll(M0P_UART0,'\r');
}

uint8_t uart_read(void)
{
    if((Uart_GetStatus(M0P_UART0, UartFE))||(Uart_GetStatus(M0P_UART0, UartPE)))
    {
        Uart_ClrStatus(M0P_UART0, UartFE);
        Uart_ClrStatus(M0P_UART0, UartPE);
    }
    if(Uart_GetStatus(M0P_UART0,UartRC))
    {
        Uart_ClrStatus(M0P_UART0,UartRC);
        return Uart_ReceiveData(M0P_UART0);
    }
    return 0xff;
}
