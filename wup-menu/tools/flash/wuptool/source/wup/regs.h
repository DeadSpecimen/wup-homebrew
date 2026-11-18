#ifndef _REGS_H_
#define _REGS_H_

// --- General registers ------------------------------------------------------

#define REG_HARDWARE_ID         *(vu32*)0xF0000000
#define REG_CPU_RESET           *(vu32*)0xF0000004
#define REG_UNK08               *(vu32*)0xF0000008
#define REG_PLL_INPUTDIV        *(vu32*)0xF000000C
#define REG_PLL_MULTIPLIER      *(vu32*)0xF0000010
#define REG_PLL_PRIM_DIV        *(vu32*)0xF0000014
#define REG_PLL_SEC1_DIV        *(vu32*)0xF0000018
#define REG_PLL_SEC2_DIV        *(vu32*)0xF000001C
#define REG_PLL_SEC3_DIV        *(vu32*)0xF0000020
#define REG_PLL_UNK24           *(vu32*)0xF0000024
#define REG_PLL_UNK28           *(vu32*)0xF0000028
#define REG_PLL_UNK2C           *(vu32*)0xF000002C
#define REG_UNK30               *(vu32*)0xF0000030
#define REG_CLK_PIXELCLK        *(vu32*)0xF0000034
#define REG_CLK_CAMERA          *(vu32*)0xF0000038
#define REG_CLK_AUDIOAMP        *(vu32*)0xF000003C
#define REG_CLK_UART0           *(vu32*)0xF0000040
#define REG_CLK_UART1           *(vu32*)0xF0000044
#define REG_CLK_UART2           *(vu32*)0xF0000048
#define REG_CLK_I2C             *(vu32*)0xF000004C
#define REG_CLK_UNK50           *(vu32*)0xF0000050
#define REG_CLK_SDIO            *(vu32*)0xF0000054
#define REG_HARDWARE_RESET      *(vu32*)0xF0000058
#define REG_UNK64               *(vu32*)0xF0000064

#define PLL_DIV_2               0x005
#define PLL_DIV_2_5             0x101
#define PLL_DIV_N(n)            ((n) & 0x1FE)

// REG_CLK_xxx settings
#define CLK_SOURCE(i)           ((i) << 8)
#define CLK_DIVIDER(i)          (((i)-1) & 0xFF)

#define CLKSRC_32MHZ            0
#define CLKSRC_CAMERACLK        1
#define CLKSRC_UNK2             2
#define CLKSRC_AUDIO_BCLK       3
#define CLKSRC_PLL_PRIM         4
#define CLKSRC_PLL_SEC1         5
#define CLKSRC_PLL_SEC2         6
#define CLKSRC_PLL_SEC3         7

// REG_HARDWARE_RESET settings
#define RESET_IRQ               (1<<0)
#define RESET_TIMERS            (1<<1)
#define RESET_RAM               (1<<2)
#define RESET_DMA               (1<<3)
#define RESET_SPI               (1<<4)
#define RESET_SDIO              (1<<6)
#define RESET_UART0             (1<<7)
#define RESET_UART1             (1<<8)
#define RESET_UART2             (1<<9)
#define RESET_I2C0              (1<<10)
#define RESET_I2C1              (1<<11)
#define RESET_I2C2              (1<<12)
#define RESET_I2C3              (1<<13)
#define RESET_I2C4              (1<<14)
#define RESET_AUDIO             (1<<15)
#define RESET_VIDEO             (1<<21)


// --- Device ID --------------------------------------------------------------

#define REG_DEVICE_ID(i)        *(vu32*)(0xF0000100 + ((i)<<2))


// --- IRQ --------------------------------------------------------------------

#define REG_IRQ_ENABLEMASK0     *(vu32*)0xF0001200
#define REG_IRQ_ENABLEMASK1     *(vu32*)0xF0001204
#define REG_IRQ_ENABLE(i)       *(vu32*)(0xF0001208 + ((i)<<2))
#define REG_IRQ_TRIGGER(i)      *(vu32*)(0xF0001420 + ((i)<<2))
#define REG_IRQ_CURRENT         *(vu32*)0xF00013F0
#define REG_IRQ_ACK             *(vu32*)0xF00013F8
#define REG_IRQ_ACK_KEY         *(vu32*)0xF00013FC

#define IRQ_TIMER0              0x00
#define IRQ_TIMER1              0x01
#define IRQ_SDIO                0x02
#define IRQ_UART0               0x03
#define IRQ_UART1               0x04
#define IRQ_UART2               0x05
#define IRQ_SPI                 0x06
#define IRQ_SPI_UNK             0x07
#define IRQ_SPDMA0              0x08
#define IRQ_SPDMA1              0x09
#define IRQ_GPDMA2              0x0C
#define IRQ_GPDMA0              0x0D
#define IRQ_GPDMA1              0x0E
#define IRQ_I2C                 0x0F
#define IRQ_VBLANK_END          0x15
#define IRQ_VBLANK              0x16
#define IRQ_AUDIO               0x17
#define IRQ_MIC                 0x18
#define IRQ_VMATCH              0x1E


// --- Timers -----------------------------------------------------------------

#define REG_TIMER_PRESCALER     *(vu32*)0xF0000400
#define REG_COUNTUP_PRESCALER   *(vu32*)0xF0000404
#define REG_COUNTUP_VALUE       *(vu32*)0xF0000408
#define REG_TIMER_CNT(i)        *(vu32*)(0xF0000410 + ((i)*0x10))
#define REG_TIMER_VALUE(i)      *(vu32*)(0xF0000414 + ((i)*0x10))
#define REG_TIMER_TARGET(i)     *(vu32*)(0xF0000418 + ((i)*0x10))

// REG_TIMER_CNT settings
#define TIMER_UNK0              (1<<0)
#define TIMER_ENABLE            (1<<1)
#define TIMER_COUNT_UP          (0<<2)
#define TIMER_COUNT_DOWN        (1<<2)
#define TIMER_DIV_2             (0<<4)
#define TIMER_DIV_4             (1<<4)
#define TIMER_DIV_8             (2<<4)
#define TIMER_DIV_16            (3<<4)
#define TIMER_DIV_32            (4<<4)
#define TIMER_DIV_64            (5<<4)
#define TIMER_DIV_128           (6<<4)
#define TIMER_DIV_256           (7<<4)


// --- DMA --------------------------------------------------------------------

#define REG_DMA_CNT             *(vu32*)0xF0004000

#define REG_SPDMA_START(i)      *(vu32*)(0xF0004040 + ((i)*0x20))
#define REG_SPDMA_CNT(i)        *(vu32*)(0xF0004044 + ((i)*0x20))
#define REG_SPDMA_CHUNKLEN(i)   *(vu32*)(0xF0004048 + ((i)*0x20))
#define REG_SPDMA_MEMSTRIDE(i)  *(vu32*)(0xF000404C + ((i)*0x20))
#define REG_SPDMA_LEN(i)        *(vu32*)(0xF0004050 + ((i)*0x20))
#define REG_SPDMA_MEMADDR(i)    *(vu32*)(0xF0004054 + ((i)*0x20))

#define SPDMA_START             (1<<0)
#define SPDMA_STOP              (1<<1)
#define SPDMA_BUSY              (1<<0)

#define SPDMA_DIR_READ          (0<<0)
#define SPDMA_DIR_WRITE         (1<<0)
#define SPDMA_PERI_SPI          (2<<1)
#define SPDMA_PERI_UART0        (4<<1)
#define SPDMA_PERI_UART1        (5<<1)
#define SPDMA_PERI_UART2        (6<<1)


// --- SPI --------------------------------------------------------------------

#define REG_SPI_CLOCK           *(vu32*)0xF0004400
#define REG_SPI_CNT             *(vu32*)0xF0004404
#define REG_SPI_IRQ_STATUS      *(vu32*)0xF0004408
#define REG_SPI_FIFO_LVL        *(vu32*)0xF000440C
#define REG_SPI_DATA            *(vu32*)0xF0004410
#define REG_SPI_UNK14           *(vu32*)0xF0004414
#define REG_SPI_IRQ_ENABLE      *(vu32*)0xF0004418
#define REG_SPI_READ_LEN        *(vu32*)0xF0004420
#define REG_SPI_DEVICE_SEL      *(vu32*)0xF0004424

#define SPI_CLK_ENABLE          (1<<15)
#define SPI_CLK_SOURCE(i)       (i)
#define SPI_CLK_DIVIDER(i)      ((((i)-1) & 0xFF) << 3)

#define SPI_CLK_48MHZ           (SPI_CLK_ENABLE | SPI_CLK_SOURCE(CLKSRC_PLL_PRIM) | SPI_CLK_DIVIDER(18))
#define SPI_CLK_8MHZ            (SPI_CLK_ENABLE | SPI_CLK_SOURCE(CLKSRC_32MHZ)    | SPI_CLK_DIVIDER(4))
#define SPI_CLK_248KHZ          (SPI_CLK_ENABLE | SPI_CLK_SOURCE(CLKSRC_32MHZ)    | SPI_CLK_DIVIDER(129))

#define SPI_DIR_READ            (1<<1)
#define SPI_DIR_WRITE           (0<<1)
#define SPI_DIR_MASK            (1<<1)
#define SPI_CSMODE_AUTO         (0<<8)
#define SPI_CSMODE_MANUAL       (1<<8)
#define SPI_CS_SELECT           (0<<9)
#define SPI_CS_RELEASE          (1<<9)
#define SPI_CS_MASK             (3<<8)

#define SPI_IRQ_READ            (1<<6)
#define SPI_IRQ_WRITE           (1<<7)

#define SPI_READ_FIFO_LVL       ((REG_SPI_FIFO_LVL >> 8) & 0x1F)
#define SPI_WRITE_FIFO_LVL      (REG_SPI_FIFO_LVL & 0x1F)

#define SPI_DEVICE_FLASH        (1<<0)
#define SPI_DEVICE_UIC          (1<<1)


// --- UART -------------------------------------------------------------------

// 0x00 = RX data (read received bytes here)
// 0x04 = TX data (write bytes to transmit here)
#define REG_UART_RX(i)          *(vu32*)(0xF0004C00 + ((i)*0x40))
#define REG_UART_TX(i)          *(vu32*)(0xF0004C04 + ((i)*0x40))
#define REG_UART_IRQ_ENABLE(i)  *(vu32*)(0xF0004C08 + ((i)*0x40))
#define REG_UART_IRQ_STATUS(i)  *(vu32*)(0xF0004C0C + ((i)*0x40))
#define REG_UART_UNK10(i)       *(vu32*)(0xF0004C10 + ((i)*0x40))
#define REG_UART_CNT1(i)        *(vu32*)(0xF0004C14 + ((i)*0x40))
#define REG_UART_UNK18(i)       *(vu32*)(0xF0004C18 + ((i)*0x40))
#define REG_UART_STATUS(i)      *(vu32*)(0xF0004C1C + ((i)*0x40))
#define REG_UART_UNK20(i)       *(vu32*)(0xF0004C20 + ((i)*0x40))
#define REG_UART_CLK_DIV1(i)    *(vu32*)(0xF0004C24 + ((i)*0x40))
#define REG_UART_CLK_MULT(i)    *(vu32*)(0xF0004C28 + ((i)*0x40))
#define REG_UART_CLK_DIV2(i)    *(vu32*)(0xF0004C2C + ((i)*0x40))
#define REG_UART_CNT2(i)        *(vu32*)(0xF0004C30 + ((i)*0x40))
#define REG_UART_RX_STAT(i)     *(vu32*)(0xF0004C34 + ((i)*0x40))
#define REG_UART_TX_STAT(i)     *(vu32*)(0xF0004C38 + ((i)*0x40))
#define REG_UART_UNK3C(i)       *(vu32*)(0xF0004C3C + ((i)*0x40))

#define UART_IRQEN_RX           (1<<0)
#define UART_IRQEN_TX           (1<<1)
#define UART_IRQEN_RXERR        (1<<2)

#define UART_IRQST_NONE         1
#define UART_IRQST_TX_READY     2
#define UART_IRQST_RX           4
#define UART_IRQST_RXERR        6
#define UART_IRQST_UNK12        12

#define UART_DATA_5BIT          (0<<0)
#define UART_DATA_6BIT          (1<<0)
#define UART_DATA_7BIT          (2<<0)
#define UART_DATA_8BIT          (3<<0)
#define UART_STOP_1BIT          (0<<2)
#define UART_STOP_2BIT          (1<<2)
#define UART_PARITY_NONE        (0<<3)
#define UART_PARITY_ODD         (1<<3)
#define UART_PARITY_EVEN        (3<<3)

#define UART_STAT_RX            (1<<0)
#define UART_STAT_RX_OVF        (1<<1)
#define UART_STAT_PARITY_ERR    (1<<2)
#define UART_STAT_RX_UNK        (1<<3)
#define UART_STAT_RX_LINE_ERR   (1<<4)
#define UART_STAT_TX_READY      (1<<5)
#define UART_STAT_TX_READY2     (1<<6)
#define UART_STAT_RX_ERR        (1<<7)

#define UART_ENABLE             (1<<0)
#define UART_SWAP_BITS          (1<<1)
#define UART_INVERT_LINES       (1<<2)

#define UART_TRX_STATUS(r)      ((r) & 0xF)
#define UART_TRX_FIFO_LVL(r)    (((r) >> 8) & 0x1F)


// --- GPIO -------------------------------------------------------------------

#define REG_GPIO_UART1_TX       *(vu32*)0xF00050D4
#define REG_GPIO_UART1_RX       *(vu32*)0xF00050D8
#define REG_GPIO_SPI_CLOCK      *(vu32*)0xF00050EC
#define REG_GPIO_SPI_MISO       *(vu32*)0xF00050F0
#define REG_GPIO_SPI_MOSI       *(vu32*)0xF00050F4
#define REG_GPIO_SPI_CS0        *(vu32*)0xF00050F8
#define REG_GPIO_SPI_CS1        *(vu32*)0xF00050FC
#define REG_GPIO_RUMBLE         *(vu32*)0xF0005114

#define GPIO_ALT_FUNCTION       (1<<0)
#define GPIO_OUTPUT_MODE        (1<<9)
#define GPIO_INPUT_MODE         (1<<11)
#define GPIO_PULL_UP            (1<<12)
#define GPIO_PULL_DOWN          (1<<13)

#define GPIO_OUTPUT_LOW         (GPIO_OUTPUT_MODE | (0<<8))
#define GPIO_OUTPUT_HIGH        (GPIO_OUTPUT_MODE | (1<<8))

#endif // _REGS_H_
