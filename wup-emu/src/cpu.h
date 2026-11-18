#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MODE_USR 0x10
#define MODE_FIQ 0x11
#define MODE_IRQ 0x12
#define MODE_SVC 0x13
#define MODE_ABT 0x17
#define MODE_UND 0x1B
#define MODE_SYS 0x1F

#define PSR_N (1u<<31)
#define PSR_Z (1u<<30)
#define PSR_C (1u<<29)
#define PSR_V (1u<<28)
#define PSR_Q (1u<<27)
#define PSR_I (1u<<7)
#define PSR_F (1u<<6)
#define PSR_T (1u<<5)

typedef struct CPU {
    uint32_t r[16];
    uint32_t cpsr, spsr;
    uint32_t bank_r13[6], bank_r14[6], bank_spsr[6];
    uint32_t bank_fiq[5], bank_usr[5]; /* r8-r12 */

    /* CP15 */
    uint32_t cp15_ctrl, cp15_ttb, cp15_dacr, cp15_dfsr, cp15_ifsr, cp15_far, cp15_fcse, cp15_ctx;

    bool irq_line, fiq_line;
    bool halted;          /* wait-for-interrupt */
    bool branched;
    bool dabort;
    bool reset_req;          /* data abort raised during current instruction */
    uint32_t cur_pc;
    uint64_t cycles;
} CPU;

void cpu_reset(CPU *c);
/* run for approximately n cycles; returns cycles executed */
int  cpu_run(CPU *c, int n);
void cpu_tlb_flush(void);
void cpu_dump(CPU *c);
void cpu_trace_dump(void);

/* debug helpers: virtual memory reads without side effects on faults */
bool cpu_virt_to_phys(CPU *c, uint32_t va, uint32_t *pa);
uint32_t cpu_peek32(CPU *c, uint32_t va);

extern int cpu_trace_enabled;
