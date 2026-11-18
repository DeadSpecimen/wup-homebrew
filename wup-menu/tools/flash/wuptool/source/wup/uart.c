#include <wup/wup.h>

#define UART_NUM 1

void UART_Init()
{
    // Configure GPIO pins for UART1
    REG_GPIO_UART1_TX = 0x8001;  // alt function + fast slew
    REG_GPIO_UART1_RX = 0x0801;  // alt func + input mode

    // Full reset like original wupbootdump
    u32 resetbit = RESET_UART0 << UART_NUM;
    REG_HARDWARE_RESET |= resetbit;
    REG_CLK_UART1 = CLK_SOURCE(CLKSRC_32MHZ) | CLK_DIVIDER(8);
    REG_HARDWARE_RESET &= ~resetbit;

    REG_UART_CNT2(UART_NUM) = UART_ENABLE;

    // clock parameters for 115200 baud
    REG_UART_CLK_DIV1(UART_NUM) = 1;
    REG_UART_CLK_MULT(UART_NUM) = 288;
    REG_UART_CLK_DIV2(UART_NUM) = 625;

    REG_UART_UNK10(UART_NUM) = 0;
    REG_UART_CNT1(UART_NUM) = UART_DATA_8BIT | UART_STOP_1BIT | UART_PARITY_NONE;
    REG_UART_UNK18(UART_NUM) = 0;

    // Enable both TX and RX
    REG_UART_IRQ_ENABLE(UART_NUM) = UART_IRQEN_TX | UART_IRQEN_RX;

    while (REG_UART_IRQ_STATUS(UART_NUM) != UART_IRQST_TX_READY);
}

void UART_Send(u8* buf, int len)
{
    SPDMA_Transfer(1, buf, SPDMA_PERI_UART1, SPDMA_DIR_WRITE, len);
    SPDMA_Wait(1);

    while (REG_UART_IRQ_STATUS(UART_NUM) != UART_IRQST_TX_READY);
}

void UART_SendByte(u8 byte)
{
    while (!(REG_UART_STATUS(UART_NUM) & UART_STAT_TX_READY));
    REG_UART_TX(UART_NUM) = byte;
    while (!(REG_UART_STATUS(UART_NUM) & UART_STAT_TX_READY));
}

int UART_DataAvailable()
{
    return (REG_UART_STATUS(UART_NUM) & UART_STAT_RX) ? 1 : 0;
}

u8 UART_RecvByte()
{
    while (!(REG_UART_STATUS(UART_NUM) & UART_STAT_RX));
    return (u8)REG_UART_RX(UART_NUM);
}

int UART_RecvLine(char* buf, int maxlen)
{
    int i = 0;
    while (i < maxlen - 1)
    {
        while (!(REG_UART_STATUS(UART_NUM) & UART_STAT_RX));
        u8 c = (u8)REG_UART_RX(UART_NUM);

        if (c == '\r' || c == '\n')
            break;
        else if (c == 0x7F || c == 0x08)
        {
            if (i > 0)
                i--;
        }
        else if (c >= 0x20 && c < 0x7F)
            buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

void UART_Print(const char* str)
{
    // Count length and send via DMA
    int len = 0;
    const char* p = str;
    while (*p++) len++;
    if (len > 0)
        UART_Send((u8*)str, len);
}

static const char hex_chars[] = "0123456789ABCDEF";

void UART_PrintHex32(u32 val)
{
    // Use DMA send instead of SendByte
    static char buf[8];
    for (int i = 0; i < 8; i++)
    {
        buf[i] = hex_chars[(val >> ((7-i) * 4)) & 0xF];
    }
    UART_Send((u8*)buf, 8);
}

void UART_PrintHex8(u8 val)
{
    static char buf[2];
    buf[0] = hex_chars[(val >> 4) & 0xF];
    buf[1] = hex_chars[val & 0xF];
    UART_Send((u8*)buf, 2);
}
