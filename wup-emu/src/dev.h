#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "bus.h"
#include "cpu.h"

extern CPU cpu;
extern uint8_t *flash;
extern uint32_t flash_size;

/* generic register-file stub with logging */
typedef struct {
    const char *name;
    uint32_t base;
    uint32_t size;
    uint32_t *regs;
} Stub;
void stub_register(Stub *s, const char *name, uint32_t base, uint32_t size);

void devices_init(void);
void devices_tick(int cycles);
