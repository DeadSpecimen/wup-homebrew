// Flash operations for wupflash
// Uses 4-byte addressing for Lang2 (0x1100000+)

#include <wup/wup.h>

static int Use4ByteAddr = 0;

void Flash_WaitBusy()
{
    // Wait for WIP (Write In Progress) bit to clear
    u8 cmd = 0x05;  // Read Status Register
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_8MHZ);
    SPI_Write(&cmd, 1);

    for (int timeout = 0; timeout < 1000000; timeout++)
    {
        u8 status;
        SPI_Read(&status, 1);
        if ((status & 0x01) == 0)  // WIP bit clear
            break;
    }

    SPI_Finish();
}

void Flash_WriteEnable()
{
    u8 cmd = 0x06;
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_8MHZ);
    SPI_Write(&cmd, 1);
    SPI_Finish();
}

void Flash_Set4ByteAddr(int enable)
{
    Flash_WaitBusy();
    Flash_WriteEnable();

    u8 cmd = enable ? 0xB7 : 0xE9;  // Enter/Exit 4-byte address mode
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_8MHZ);
    SPI_Write(&cmd, 1);
    SPI_Finish();

    Use4ByteAddr = enable;
}

void Flash_ReadID(u8* id, int len)
{
    u8 cmd = 0x9F;
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_8MHZ);
    SPI_Write(&cmd, 1);
    SPI_Read(id, len);
    SPI_Finish();
}

void Flash_Read(u32 addr, void* data, int len)
{
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_8MHZ);

    if (Use4ByteAddr)
    {
        u8 cmd[5];
        cmd[0] = 0x03;  // READ command
        cmd[1] = (addr >> 24) & 0xFF;
        cmd[2] = (addr >> 16) & 0xFF;
        cmd[3] = (addr >> 8) & 0xFF;
        cmd[4] = addr & 0xFF;
        SPI_Write(cmd, 5);
    }
    else
    {
        u8 cmd[4];
        cmd[0] = 0x03;  // READ command
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        SPI_Write(cmd, 4);
    }

    SPI_Read((u8*)data, len);
    SPI_Finish();
}

// Erase 64KB sector
void Flash_EraseSector(u32 addr)
{
    Flash_WaitBusy();
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_8MHZ);

    if (Use4ByteAddr)
    {
        u8 cmd[5];
        cmd[0] = 0xDC;  // 64KB sector erase (4-byte addr)
        cmd[1] = (addr >> 24) & 0xFF;
        cmd[2] = (addr >> 16) & 0xFF;
        cmd[3] = (addr >> 8) & 0xFF;
        cmd[4] = addr & 0xFF;
        SPI_Write(cmd, 5);
    }
    else
    {
        u8 cmd[4];
        cmd[0] = 0xD8;  // 64KB sector erase
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        SPI_Write(cmd, 4);
    }

    SPI_Finish();
    Flash_WaitBusy();  // Wait for erase to complete
}

// Erase 4KB subsector
void Flash_EraseSubsector(u32 addr)
{
    Flash_WaitBusy();
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_8MHZ);

    if (Use4ByteAddr)
    {
        u8 cmd[5];
        cmd[0] = 0x21;  // 4KB subsector erase (4-byte addr)
        cmd[1] = (addr >> 24) & 0xFF;
        cmd[2] = (addr >> 16) & 0xFF;
        cmd[3] = (addr >> 8) & 0xFF;
        cmd[4] = addr & 0xFF;
        SPI_Write(cmd, 5);
    }
    else
    {
        u8 cmd[4];
        cmd[0] = 0x20;  // 4KB subsector erase
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        SPI_Write(cmd, 4);
    }

    SPI_Finish();
    Flash_WaitBusy();
}

// Program up to 256 bytes (one page)
void Flash_PageProgram(u32 addr, void* data, int len)
{
    Flash_WaitBusy();
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_8MHZ);

    if (Use4ByteAddr)
    {
        u8 cmd[5];
        cmd[0] = 0x12;  // Page program (4-byte addr)
        cmd[1] = (addr >> 24) & 0xFF;
        cmd[2] = (addr >> 16) & 0xFF;
        cmd[3] = (addr >> 8) & 0xFF;
        cmd[4] = addr & 0xFF;
        SPI_Write(cmd, 5);
    }
    else
    {
        u8 cmd[4];
        cmd[0] = 0x02;  // Page program
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        SPI_Write(cmd, 4);
    }

    SPI_Write(data, len);
    SPI_Finish();

    Flash_WaitBusy();  // Wait for program to complete
}

// Write arbitrary length (handles page boundaries)
void Flash_Write(u32 addr, void* data, int len)
{
    u8* data8 = (u8*)data;
    for (int pos = 0; pos < len; )
    {
        int chunk = 0x100 - (addr & 0xFF);  // Bytes until page boundary
        if ((pos + chunk) > len)
            chunk = len - pos;

        Flash_PageProgram(addr, data8, chunk);
        addr += chunk;
        data8 += chunk;
        pos += chunk;
    }
}
