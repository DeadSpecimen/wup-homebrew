#ifndef _UART_H_
#define _UART_H_

void UART_Init();

// TX functions
void UART_Send(u8* buf, int len);
void UART_SendByte(u8 byte);

// RX functions
u8 UART_RecvByte();
int UART_RecvLine(char* buf, int maxlen);
int UART_DataAvailable();

// Print helpers
void UART_Print(const char* str);
void UART_PrintHex32(u32 val);
void UART_PrintHex8(u8 val);

#endif //_UART_H_
