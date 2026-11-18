#ifndef _WUP_H_
#define _WUP_H_

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "regs.h"

#include "dma.h"
#include "spi.h"
#include "uic.h"
#include "uart.h"

void WUP_Init();

void WUP_DelayUS(int us);
void WUP_DelayMS(int ms);

// Flash operations
void Flash_WaitBusy();
void Flash_WriteEnable();
void Flash_Set4ByteAddr(int enable);
void Flash_ReadID(u8* id, int len);
void Flash_Read(u32 addr, void* data, int len);
void Flash_EraseSector(u32 addr);
void Flash_EraseSubsector(u32 addr);
void Flash_PageProgram(u32 addr, void* data, int len);

#endif // _WUP_H_
