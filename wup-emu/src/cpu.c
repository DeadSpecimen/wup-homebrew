/* ARM926EJ-S (ARMv5TE) interpreter: ARM + Thumb, CP15 with MMU. */
#include "cpu.h"
#include "bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cpu_trace_enabled = 0;
uint32_t cpu_break_addr = 0xffffffff;

/* ---------------------------------------------------------------- trace */
#define TRACE_N 256
static uint32_t trace_pc[TRACE_N];
static uint32_t trace_cpsr[TRACE_N];
static unsigned trace_pos;

#define CALLN 128
typedef struct { uint32_t t, r0, r1, r2, lr, sp; uint64_t cyc; } CallRec;
static CallRec pend[CALLN];
void cpu_calls_dump(void) {
    fprintf(stderr, "--- pending (blocked) kernel calls ---\n");
    for (unsigned i = 0; i < CALLN; i++) {
        CallRec *k = &pend[i];
        if (k->t) fprintf(stderr, "%10llu call %05x(%08x, %08x, %08x) from %08x sp=%08x\n", (unsigned long long)k->cyc, k->t, k->r0, k->r1, k->r2, k->lr, k->sp);
    }
}
void cpu_trace_dump(void) {
    cpu_calls_dump();
    fprintf(stderr, "--- last %d PCs ---\n", TRACE_N);
    for (unsigned i = 0; i < TRACE_N; i++) {
        unsigned j = (trace_pos + i) % TRACE_N;
        fprintf(stderr, "%08x%s ", trace_pc[j], (trace_cpsr[j] & PSR_T) ? "t" : "a");
        if (i % 8 == 7) fprintf(stderr, "\n");
    }
}

/* ---------------------------------------------------------------- modes */
static int bank_index(uint32_t mode) {
    switch (mode & 0x1f) {
    case MODE_FIQ: return 1;
    case MODE_IRQ: return 2;
    case MODE_SVC: return 3;
    case MODE_ABT: return 4;
    case MODE_UND: return 5;
    default: return 0;
    }
}

static void switch_mode(CPU *c, uint32_t newmode) {
    uint32_t old = c->cpsr & 0x1f;
    newmode &= 0x1f;
    if (old == newmode) return;
    int ob = bank_index(old), nb = bank_index(newmode);
    if (ob != nb) {
        c->bank_r13[ob] = c->r[13];
        c->bank_r14[ob] = c->r[14];
        if (ob) c->bank_spsr[ob] = c->spsr;
        if (ob == 1 || nb == 1) {
            if (ob == 1) {
                memcpy(c->bank_fiq, &c->r[8], 20);
                memcpy(&c->r[8], c->bank_usr, 20);
            } else {
                memcpy(c->bank_usr, &c->r[8], 20);
                memcpy(&c->r[8], c->bank_fiq, 20);
            }
        }
        c->r[13] = c->bank_r13[nb];
        c->r[14] = c->bank_r14[nb];
        c->spsr = nb ? c->bank_spsr[nb] : c->cpsr;
    }
    c->cpsr = (c->cpsr & ~0x1fu) | newmode;
}

static void set_cpsr(CPU *c, uint32_t v) {
    switch_mode(c, v & 0x1f);
    c->cpsr = v;
}

/* ---------------------------------------------------------------- MMU */
#define TLB_SIZE 4096
typedef struct { uint32_t tag; uint32_t ppage; } TLBEnt;
static TLBEnt tlb[TLB_SIZE];

static uint32_t fetch_vpage;
void cpu_tlb_flush(void) { memset(tlb, 0, sizeof tlb); fetch_vpage = 0xffffffff; }

/* fault status codes */
#define FS_SECT_XLATE 0x5
#define FS_PAGE_XLATE 0x7
#define FS_SECT_DOMAIN 0x9
#define FS_PAGE_DOMAIN 0xB

static int walk(CPU *c, uint32_t va, uint32_t *pa, int *cacheable) {
    uint32_t l1 = bus_read32((c->cp15_ttb & 0xffffc000) | ((va >> 18) & 0x3ffc));
    uint32_t dom = (l1 >> 5) & 15;
    uint32_t dacc = (c->cp15_dacr >> (dom * 2)) & 3;
    *cacheable = 1;
    switch (l1 & 3) {
    case 0: return FS_SECT_XLATE | (dom << 4);
    case 2: /* section */
        if (dacc == 0 || dacc == 2) return FS_SECT_DOMAIN | (dom << 4);
        *pa = (l1 & 0xfff00000) | (va & 0x000fffff);
        return 0;
    case 1: { /* coarse */
        uint32_t l2 = bus_read32((l1 & 0xfffffc00) | ((va >> 10) & 0x3fc));
        if (dacc == 0 || dacc == 2) return FS_PAGE_DOMAIN | (dom << 4);
        switch (l2 & 3) {
        case 0: return FS_PAGE_XLATE | (dom << 4);
        case 1: *pa = (l2 & 0xffff0000) | (va & 0xffff); return 0;
        case 2: *pa = (l2 & 0xfffff000) | (va & 0xfff); return 0;
        case 3: *pa = (l2 & 0xfffffc00) | (va & 0x3ff); *cacheable = 0; return 0; /* tiny (not valid in coarse, treat) */
        }
        break;
    }
    case 3: { /* fine */
        uint32_t l2 = bus_read32((l1 & 0xfffff000) | ((va >> 8) & 0xffc));
        if (dacc == 0 || dacc == 2) return FS_PAGE_DOMAIN | (dom << 4);
        switch (l2 & 3) {
        case 0: return FS_PAGE_XLATE | (dom << 4);
        case 1: *pa = (l2 & 0xffff0000) | (va & 0xffff); return 0;
        case 2: *pa = (l2 & 0xfffff000) | (va & 0xfff); return 0;
        case 3: *pa = (l2 & 0xfffffc00) | (va & 0x3ff); *cacheable = 0; return 0;
        }
        break;
    }
    }
    return FS_SECT_XLATE;
}

static inline int xlate(CPU *c, uint32_t va, uint32_t *pa) {
    if (!(c->cp15_ctrl & 1)) { *pa = va; return 0; }
    if (va < 0x02000000) va |= c->cp15_fcse;
    uint32_t vp = va >> 12;
    TLBEnt *e = &tlb[vp & (TLB_SIZE - 1)];
    if (e->tag == vp + 1) { *pa = (e->ppage << 12) | (va & 0xfff); return 0; }
    int cacheable;
    int fs = walk(c, va, pa, &cacheable);
    if (fs) return fs;
    if (cacheable) { e->tag = vp + 1; e->ppage = *pa >> 12; }
    return 0;
}

bool cpu_virt_to_phys(CPU *c, uint32_t va, uint32_t *pa) { return xlate(c, va, pa) == 0; }
uint32_t cpu_peek32(CPU *c, uint32_t va) {
    uint32_t pa;
    if (xlate(c, va, &pa)) return 0xdeadbeef;
    uint8_t *p = page_r[pa >> 12];
    return p ? *(uint32_t *)(p + (pa & 0xffc)) : 0xdeadbeef;
}

static void data_fault(CPU *c, uint32_t va, int fs, bool write) {
    if (!c->dabort) {
        c->dabort = true;
        c->cp15_far = va;
        c->cp15_dfsr = fs | (write ? 0x800 : 0);
        if (getenv("DRC_DEBUG")) fprintf(stderr, "[cpu] data abort va=%08x fs=%x pc=%08x\n", va, fs, c->cur_pc);
    }
}

static uint32_t watch_lo = 1, watch_hi = 0;
static uint32_t wwatch_lo = 1, wwatch_hi = 0;
static void watch_hit(CPU *c, uint32_t va, int size) {
    static uint32_t seen[4096]; static int n;
    uint32_t key = c->cur_pc ^ ((va - watch_lo) << 24);
    for (int i = 0; i < n; i++) if (seen[i] == key) return;
    if (n < 4096) seen[n++] = key;
    fprintf(stderr, "[watch] pc=%08x read%d +%02x (lr=%08x)\n", c->cur_pc, size * 8, va - watch_lo, c->r[14]);
}
void cpu_watch_init(void) {
    const char *e = getenv("WATCH");
    if (e) sscanf(e, "%x-%x", &watch_lo, &watch_hi);
    e = getenv("WWATCH");
    if (e) sscanf(e, "%x-%x", &wwatch_lo, &wwatch_hi);
}
#define WATCH(va, sz) do { if ((va) - watch_lo <= watch_hi - watch_lo && watch_hi) watch_hit(c, va, sz); } while (0)

static inline uint32_t rd32(CPU *c, uint32_t va) {
    WATCH(va, 4);
    uint32_t pa; int fs = xlate(c, va, &pa);
    if (fs) { data_fault(c, va, fs, false); return 0; }
    return bus_read32(pa);
}
static inline uint32_t rd16(CPU *c, uint32_t va) {
    WATCH(va, 2);
    uint32_t pa; int fs = xlate(c, va, &pa);
    if (fs) { data_fault(c, va, fs, false); return 0; }
    return bus_read16(pa);
}
static inline uint32_t rd8(CPU *c, uint32_t va) {
    WATCH(va, 1);
    uint32_t pa; int fs = xlate(c, va, &pa);
    if (fs) { data_fault(c, va, fs, false); return 0; }
    return bus_read8(pa);
}
static void wwatch_hit(CPU *c, uint32_t va, uint32_t v, int sz) {
    fprintf(stderr, "[wwatch] pc=%08x write%d %08x <- %08x (lr=%08x)\n", c->cur_pc, sz * 8, va, v, c->r[14]);
}
#define WWATCH(va, v, sz) do { if (wwatch_hi && (va) + (sz) > wwatch_lo && (va) <= wwatch_hi) wwatch_hit(c, va, v, sz); } while (0)
static inline void wr32(CPU *c, uint32_t va, uint32_t v) {
    WWATCH(va, v, 4);
    uint32_t pa; int fs = xlate(c, va, &pa);
    if (fs) { data_fault(c, va, fs, true); return; }
    bus_write32(pa, v);
}
static inline void wr16(CPU *c, uint32_t va, uint32_t v) {
    WWATCH(va, v, 2);
    uint32_t pa; int fs = xlate(c, va, &pa);
    if (fs) { data_fault(c, va, fs, true); return; }
    bus_write16(pa, v);
}
static inline void wr8(CPU *c, uint32_t va, uint32_t v) {
    WWATCH(va, v, 1);
    uint32_t pa; int fs = xlate(c, va, &pa);
    if (fs) { data_fault(c, va, fs, true); return; }
    bus_write8(pa, v);
}

/* instruction fetch with a one-entry page cache */
static uint8_t *fetch_host;

static inline bool fetch_setup(CPU *c, uint32_t va, uint32_t *fs_out) {
    uint32_t pa; int fs = xlate(c, va, &pa);
    if (fs) { *fs_out = fs; return false; }
    uint8_t *p = page_r[pa >> 12];
    if (!p) { *fs_out = 0x100; return false; }
    fetch_vpage = va >> 12;
    fetch_host = p;
    return true;
}

/* ---------------------------------------------------------------- exceptions */
static void exception(CPU *c, uint32_t mode, uint32_t vector, uint32_t lr, bool disable_fiq) {
    uint32_t old = c->cpsr;
    switch_mode(c, mode);
    c->spsr = old;
    c->r[14] = lr;
    c->cpsr &= ~PSR_T;
    c->cpsr |= PSR_I;
    if (disable_fiq) c->cpsr |= PSR_F;
    if (c->cp15_ctrl & (1 << 13)) vector |= 0xffff0000;
    c->r[15] = vector;
    c->branched = true;
}

void cpu_reset(CPU *c) {
    cpu_watch_init();
    memset(c, 0, sizeof *c);
    c->cpsr = MODE_SVC | PSR_I | PSR_F;
    c->cp15_ctrl = 0x00050078;
    c->r[15] = 0;
    cpu_tlb_flush();
    fetch_vpage = 0xffffffff;
}

/* ---------------------------------------------------------------- helpers */
#define FLAG_N(c) (((c)->cpsr >> 31) & 1)
#define FLAG_Z(c) (((c)->cpsr >> 30) & 1)
#define FLAG_C(c) (((c)->cpsr >> 29) & 1)
#define FLAG_V(c) (((c)->cpsr >> 28) & 1)

static inline bool cond_pass(CPU *c, uint32_t cond) {
    uint32_t f = c->cpsr >> 28;
    bool n = f & 8, z = f & 4, cy = f & 2, v = f & 1;
    switch (cond) {
    case 0x0: return z;
    case 0x1: return !z;
    case 0x2: return cy;
    case 0x3: return !cy;
    case 0x4: return n;
    case 0x5: return !n;
    case 0x6: return v;
    case 0x7: return !v;
    case 0x8: return cy && !z;
    case 0x9: return !cy || z;
    case 0xA: return n == v;
    case 0xB: return n != v;
    case 0xC: return !z && n == v;
    case 0xD: return z || n != v;
    case 0xE: return true;
    default: return false;
    }
}

static inline void set_nz(CPU *c, uint32_t r) {
    c->cpsr = (c->cpsr & ~(PSR_N | PSR_Z)) | (r & PSR_N) | (r == 0 ? PSR_Z : 0);
}
static inline void set_nzcv(CPU *c, uint32_t r, int cy, int v) {
    c->cpsr = (c->cpsr & 0x0fffffff) | (r & PSR_N) | (r == 0 ? PSR_Z : 0) | (cy ? PSR_C : 0) | (v ? PSR_V : 0);
}
static inline void set_c(CPU *c, int cy) {
    c->cpsr = (c->cpsr & ~PSR_C) | (cy ? PSR_C : 0);
}

static inline uint32_t add_flags(CPU *c, uint32_t a, uint32_t b, uint32_t cin, bool s) {
    uint64_t r = (uint64_t)a + b + cin;
    uint32_t res = (uint32_t)r;
    if (s) set_nzcv(c, res, (r >> 32) & 1, (~(a ^ b) & (a ^ res)) >> 31);
    return res;
}
static inline uint32_t sub_flags(CPU *c, uint32_t a, uint32_t b, uint32_t cin, bool s) {
    /* a - b - !cin */
    uint64_t r = (uint64_t)a + (uint32_t)~b + cin;
    uint32_t res = (uint32_t)r;
    if (s) set_nzcv(c, res, (r >> 32) & 1, ((a ^ b) & (a ^ res)) >> 31);
    return res;
}

static inline void branch_to(CPU *c, uint32_t addr) {
    c->r[15] = addr;
    c->branched = true;
}
static inline void branch_interwork(CPU *c, uint32_t addr) {
    if (addr & 1) { c->cpsr |= PSR_T; c->r[15] = addr & ~1u; }
    else { c->cpsr &= ~PSR_T; c->r[15] = addr & ~3u; }
    c->branched = true;
}

static inline uint32_t ror32(uint32_t v, unsigned s) { s &= 31; return s ? (v >> s) | (v << (32 - s)) : v; }

static inline int32_t sat32(int64_t v, CPU *c) {
    if (v > 0x7fffffffLL) { c->cpsr |= PSR_Q; return 0x7fffffff; }
    if (v < -0x80000000LL) { c->cpsr |= PSR_Q; return (int32_t)0x80000000; }
    return (int32_t)v;
}

/* shifter */
static inline uint32_t shift_op(CPU *c, uint32_t v, int type, uint32_t amt, bool reg, int *cout) {
    int cy = FLAG_C(c);
    if (reg) {
        amt &= 0xff;
        if (amt == 0) { *cout = cy; return v; }
        switch (type) {
        case 0:
            if (amt < 32) { *cout = (v >> (32 - amt)) & 1; return v << amt; }
            *cout = amt == 32 ? (v & 1) : 0; return 0;
        case 1:
            if (amt < 32) { *cout = (v >> (amt - 1)) & 1; return v >> amt; }
            *cout = amt == 32 ? (v >> 31) : 0; return 0;
        case 2:
            if (amt < 32) { *cout = (v >> (amt - 1)) & 1; return (uint32_t)((int32_t)v >> amt); }
            *cout = v >> 31; return (v >> 31) ? 0xffffffff : 0;
        default:
            amt &= 31;
            if (amt == 0) { *cout = v >> 31; return v; }
            *cout = (v >> (amt - 1)) & 1; return ror32(v, amt);
        }
    }
    switch (type) {
    case 0:
        if (amt == 0) { *cout = cy; return v; }
        *cout = (v >> (32 - amt)) & 1; return v << amt;
    case 1:
        if (amt == 0) { *cout = v >> 31; return 0; }
        *cout = (v >> (amt - 1)) & 1; return v >> amt;
    case 2:
        if (amt == 0) { *cout = v >> 31; return (v >> 31) ? 0xffffffff : 0; }
        *cout = (v >> (amt - 1)) & 1; return (uint32_t)((int32_t)v >> amt);
    default:
        if (amt == 0) { *cout = v & 1; return (v >> 1) | ((uint32_t)cy << 31); }
        *cout = (v >> (amt - 1)) & 1; return ror32(v, amt);
    }
}

/* ---------------------------------------------------------------- CP15 */
static void cp15_write(CPU *c, int crn, int crm, int op1, int op2, uint32_t v) {
    (void)op1;
    switch (crn) {
    case 1:
        if ((v ^ c->cp15_ctrl) & 1) { cpu_tlb_flush(); }
        c->cp15_ctrl = v;
        fetch_vpage = 0xffffffff;
        break;
    case 2: c->cp15_ttb = v; cpu_tlb_flush(); fetch_vpage = 0xffffffff; break;
    case 3: c->cp15_dacr = v; cpu_tlb_flush(); fetch_vpage = 0xffffffff; break;
    case 5: if (op2 == 0) c->cp15_dfsr = v; else c->cp15_ifsr = v; break;
    case 6: c->cp15_far = v; break;
    case 7:
        if (crm == 0 && op2 == 4) { c->halted = true; } /* wait for interrupt */
        else if (crm == 8 && op2 == 2) { c->halted = true; }
        break;
    case 8: cpu_tlb_flush(); fetch_vpage = 0xffffffff; break;
    case 13:
        if (op2 == 0) { c->cp15_fcse = v & 0xfe000000; cpu_tlb_flush(); fetch_vpage = 0xffffffff; }
        else c->cp15_ctx = v;
        break;
    default: break; /* caches, TCM, lockdown: ignore */
    }
}

static uint32_t cp15_read(CPU *c, int crn, int crm, int op1, int op2) {
    (void)op1;
    switch (crn) {
    case 0:
        if (op2 == 1) return 0x1d152152; /* cache type */
        if (op2 == 2) return 0;          /* TCM status */
        return 0x41069265;               /* ARM926EJ-S */
    case 1: return c->cp15_ctrl;
    case 2: return c->cp15_ttb;
    case 3: return c->cp15_dacr;
    case 5: return op2 == 0 ? c->cp15_dfsr : c->cp15_ifsr;
    case 6: return c->cp15_far;
    case 7:
        if (crm == 10 && op2 == 3) return 1u << 30; /* test-and-clean: done */
        if (crm == 14 && op2 == 3) return 1u << 30;
        return 0;
    case 13: return op2 == 0 ? c->cp15_fcse : c->cp15_ctx;
    default: return 0;
    }
    (void)crm;
}

/* ---------------------------------------------------------------- ARM */
static void undefined(CPU *c, uint32_t instr, bool thumb) {
    fprintf(stderr, "[cpu] undefined instruction %08x at %08x (%s)\n", instr, c->cur_pc, thumb ? "thumb" : "arm");
    exception(c, MODE_UND, 0x04, c->cur_pc + (thumb ? 2 : 4), false);
}

static inline uint32_t reg_read(CPU *c, int r) { return c->r[r]; }
static inline void reg_write(CPU *c, int r, uint32_t v) {
    if (r == 15) branch_to(c, v & ~3u);
    else c->r[r] = v;
}

static void arm_data_proc(CPU *c, uint32_t in) {
    int op = (in >> 21) & 15;
    bool s = (in >> 20) & 1;
    int rn = (in >> 16) & 15, rd = (in >> 12) & 15;
    uint32_t op2; int sc;
    if (in & (1 << 25)) {
        uint32_t imm = in & 0xff, rot = ((in >> 8) & 15) * 2;
        op2 = ror32(imm, rot);
        sc = rot ? (op2 >> 31) : FLAG_C(c);
    } else {
        int rm = in & 15, type = (in >> 5) & 3;
        uint32_t v = c->r[rm];
        if (in & 0x10) {
            if (rm == 15) v += 4;
            uint32_t amt = c->r[(in >> 8) & 15];
            op2 = shift_op(c, v, type, amt, true, &sc);
        } else {
            op2 = shift_op(c, v, type, (in >> 7) & 31, false, &sc);
        }
    }
    uint32_t a = c->r[rn];
    if (rn == 15 && !(in & (1 << 25)) && (in & 0x10)) a += 4;
    uint32_t res = 0;
    bool logical = false, write = true;
    bool sflags = s && rd != 15;
    switch (op) {
    case 0x0: res = a & op2; logical = true; break;
    case 0x1: res = a ^ op2; logical = true; break;
    case 0x2: res = sub_flags(c, a, op2, 1, sflags); break;
    case 0x3: res = sub_flags(c, op2, a, 1, sflags); break;
    case 0x4: res = add_flags(c, a, op2, 0, sflags); break;
    case 0x5: res = add_flags(c, a, op2, FLAG_C(c), sflags); break;
    case 0x6: res = sub_flags(c, a, op2, FLAG_C(c), sflags); break;
    case 0x7: res = sub_flags(c, op2, a, FLAG_C(c), sflags); break;
    case 0x8: res = a & op2; logical = true; write = false; break;
    case 0x9: res = a ^ op2; logical = true; write = false; break;
    case 0xA: res = sub_flags(c, a, op2, 1, true); write = false; break;
    case 0xB: res = add_flags(c, a, op2, 0, true); write = false; break;
    case 0xC: res = a | op2; logical = true; break;
    case 0xD: res = op2; logical = true; break;
    case 0xE: res = a & ~op2; logical = true; break;
    case 0xF: res = ~op2; logical = true; break;
    }
    if (logical && (sflags || (!write && s))) {
        set_nz(c, res); set_c(c, sc);
    }
    if (write) {
        if (rd == 15) {
            if (s) {
                uint32_t sp = c->spsr;
                set_cpsr(c, sp);
                if (sp & PSR_T) branch_to(c, res & ~1u); else branch_to(c, res & ~3u);
            } else {
                branch_to(c, res & ~3u);
            }
        } else {
            c->r[rd] = res;
        }
    } else if (rd == 15 && s) {
        /* TSTP etc (legacy) - ignore */
    }
}

static void arm_msr(CPU *c, uint32_t in) {
    uint32_t v;
    if (in & (1 << 25)) v = ror32(in & 0xff, ((in >> 8) & 15) * 2);
    else v = c->r[in & 15];
    uint32_t mask = 0;
    if (in & (1 << 16)) mask |= 0x000000ff;
    if (in & (1 << 17)) mask |= 0x0000ff00;
    if (in & (1 << 18)) mask |= 0x00ff0000;
    if (in & (1 << 19)) mask |= 0xff000000;
    if (in & (1 << 22)) {
        if (bank_index(c->cpsr)) c->spsr = (c->spsr & ~mask) | (v & mask);
    } else {
        if ((c->cpsr & 0x1f) == MODE_USR) mask &= 0xff000000;
        mask &= ~PSR_T;
        uint32_t nv = (c->cpsr & ~mask) | (v & mask);
        set_cpsr(c, nv);
    }
}

static void arm_ldm_stm(CPU *c, uint32_t in) {
    bool p = (in >> 24) & 1, u = (in >> 23) & 1, s = (in >> 22) & 1, w = (in >> 21) & 1, l = (in >> 20) & 1;
    int rn = (in >> 16) & 15;
    uint32_t list = in & 0xffff;
    uint32_t base = c->r[rn];
    int n = __builtin_popcount(list);
    uint32_t bytes = n ? n * 4 : 0x40;
    if (!list) list = 1 << 15; /* ARMv5 quirk */
    uint32_t addr, newbase;
    if (u) { addr = base + (p ? 4 : 0); newbase = base + bytes; }
    else { addr = base - bytes + (p ? 0 : 4); newbase = base - bytes; }

    bool user_bank = s && !(l && (list & 0x8000));
    uint32_t oldmode = c->cpsr & 0x1f;
    if (user_bank) switch_mode(c, MODE_USR);

    if (l) {
        if (w) c->r[rn] = newbase;
        for (int i = 0; i < 16; i++) {
            if (!(list & (1 << i))) continue;
            uint32_t v = rd32(c, addr);
            addr += 4;
            if (c->dabort) break;
            if (i == 15) {
                if (s) {
                    uint32_t sp = c->spsr;
                    set_cpsr(c, sp);
                    if (sp & PSR_T) branch_to(c, v & ~1u); else branch_to(c, v & ~3u);
                } else branch_interwork(c, v);
            } else c->r[i] = v;
        }
    } else {
        bool first = true;
        for (int i = 0; i < 16; i++) {
            if (!(list & (1 << i))) continue;
            uint32_t v = (i == 15) ? c->cur_pc + 12 : c->r[i];
            if (i == rn && !first && w) v = newbase;
            wr32(c, addr, v);
            addr += 4;
            first = false;
        }
        if (w) c->r[rn] = newbase;
    }
    if (user_bank) switch_mode(c, oldmode);
}

static void arm_single(CPU *c, uint32_t in) {
    bool p = (in >> 24) & 1, u = (in >> 23) & 1, b = (in >> 22) & 1, w = (in >> 21) & 1, l = (in >> 20) & 1;
    int rn = (in >> 16) & 15, rd = (in >> 12) & 15;
    uint32_t off;
    if (in & (1 << 25)) {
        int sc;
        off = shift_op(c, c->r[in & 15], (in >> 5) & 3, (in >> 7) & 31, false, &sc);
    } else off = in & 0xfff;
    uint32_t base = c->r[rn];
    uint32_t ea = u ? base + off : base - off;
    uint32_t addr = p ? ea : base;
    if (l) {
        uint32_t v;
        if (b) v = rd8(c, addr);
        else v = ror32(rd32(c, addr & ~3u), (addr & 3) * 8);
        if (c->dabort) return;
        if (!p || w) c->r[rn] = ea;
        if (rd == 15) branch_interwork(c, v);
        else c->r[rd] = v;
    } else {
        uint32_t v = rd == 15 ? c->cur_pc + 12 : c->r[rd];
        if (b) wr8(c, addr, v); else wr32(c, addr & ~3u, v);
        if (c->dabort) return;
        if (!p || w) c->r[rn] = ea;
    }
}

static void arm_halfword(CPU *c, uint32_t in) {
    bool p = (in >> 24) & 1, u = (in >> 23) & 1, w = (in >> 21) & 1, l = (in >> 20) & 1;
    int rn = (in >> 16) & 15, rd = (in >> 12) & 15;
    int sh = (in >> 5) & 3;
    uint32_t off = (in & (1 << 22)) ? (((in >> 4) & 0xf0) | (in & 0xf)) : c->r[in & 15];
    uint32_t base = c->r[rn];
    uint32_t ea = u ? base + off : base - off;
    uint32_t addr = p ? ea : base;
    bool wb = !p || w;
    if (l) {
        uint32_t v;
        switch (sh) {
        case 1: v = rd16(c, addr & ~1u); if (addr & 1) v = ror32(v, 8); break;
        case 2: v = (uint32_t)(int8_t)rd8(c, addr); break;
        default:
            if (addr & 1) v = (uint32_t)(int8_t)rd8(c, addr);
            else v = (uint32_t)(int16_t)rd16(c, addr);
            break;
        }
        if (c->dabort) return;
        if (wb) c->r[rn] = ea;
        reg_write(c, rd, v);
    } else {
        switch (sh) {
        case 1: wr16(c, addr & ~1u, rd == 15 ? c->cur_pc + 12 : c->r[rd]); break;
        case 2: { /* LDRD */
            uint32_t a = addr;
            uint32_t v0 = rd32(c, a), v1 = rd32(c, a + 4);
            if (c->dabort) return;
            if (wb) c->r[rn] = ea;
            c->r[rd & ~1] = v0;
            reg_write(c, (rd & ~1) + 1, v1);
            return;
        }
        case 3: /* STRD */
            wr32(c, addr, c->r[rd & ~1]);
            wr32(c, addr + 4, c->r[(rd & ~1) + 1]);
            break;
        }
        if (c->dabort) return;
        if (wb) c->r[rn] = ea;
    }
}

static void arm_multiply(CPU *c, uint32_t in) {
    int rd = (in >> 16) & 15, rn = (in >> 12) & 15, rs = (in >> 8) & 15, rm = in & 15;
    bool s = (in >> 20) & 1;
    if (!(in & (1 << 23))) {
        uint32_t r = c->r[rm] * c->r[rs];
        if (in & (1 << 21)) r += c->r[rn];
        c->r[rd] = r;
        if (s) set_nz(c, r);
    } else {
        uint64_t r;
        if (in & (1 << 22)) r = (uint64_t)((int64_t)(int32_t)c->r[rm] * (int32_t)c->r[rs]);
        else r = (uint64_t)c->r[rm] * c->r[rs];
        if (in & (1 << 21)) r += ((uint64_t)c->r[rd] << 32) | c->r[rn];
        c->r[rn] = (uint32_t)r;
        c->r[rd] = (uint32_t)(r >> 32);
        if (s) c->cpsr = (c->cpsr & ~(PSR_N | PSR_Z)) | ((r >> 32) & PSR_N) | (r == 0 ? PSR_Z : 0);
    }
}

static void arm_dsp_mul(CPU *c, uint32_t in) {
    int rd = (in >> 16) & 15, rn = (in >> 12) & 15, rs = (in >> 8) & 15, rm = in & 15;
    int op = (in >> 21) & 3;
    bool x = (in >> 5) & 1, y = (in >> 6) & 1;
    int32_t a = x ? (int16_t)(c->r[rm] >> 16) : (int16_t)c->r[rm];
    int32_t b = y ? (int16_t)(c->r[rs] >> 16) : (int16_t)c->r[rs];
    switch (op) {
    case 0: { /* SMLAxy */
        int64_t r = (int64_t)(a * b) + (int32_t)c->r[rn];
        if (r != (int32_t)r) c->cpsr |= PSR_Q;
        c->r[rd] = (uint32_t)r;
        break;
    }
    case 1: { /* SMLAWy / SMULWy */
        int64_t r = ((int64_t)(int32_t)c->r[rm] * b) >> 16;
        if (!x) { int64_t s2 = r + (int32_t)c->r[rn]; if (s2 != (int32_t)s2) c->cpsr |= PSR_Q; r = s2; }
        c->r[rd] = (uint32_t)r;
        break;
    }
    case 2: { /* SMLALxy */
        int64_t acc = (int64_t)(((uint64_t)c->r[rd] << 32) | c->r[rn]);
        acc += (int64_t)a * b;
        c->r[rn] = (uint32_t)acc; c->r[rd] = (uint32_t)((uint64_t)acc >> 32);
        break;
    }
    case 3: c->r[rd] = (uint32_t)(a * b); break;
    }
}

static void arm_exec(CPU *c, uint32_t in) {
    uint32_t cond = in >> 28;
    if (cond == 0xF) {
        if ((in & 0x0e000000) == 0x0a000000) { /* BLX imm */
            int32_t off = ((int32_t)(in << 8) >> 6) | ((in >> 23) & 2);
            c->r[14] = c->cur_pc + 4;
            c->cpsr |= PSR_T;
            branch_to(c, c->cur_pc + 8 + off);
            return;
        }
        if ((in & 0x0d70f000) == 0x0550f000) return; /* PLD */
        undefined(c, in, false);
        return;
    }
    if (!cond_pass(c, cond)) return;

    switch ((in >> 25) & 7) {
    case 0:
        if ((in & 0x0fc000f0) == 0x00000090 || (in & 0x0f8000f0) == 0x00800090) { arm_multiply(c, in); return; }
        if ((in & 0x0fb00ff0) == 0x01000090) { /* SWP */
            int rn = (in >> 16) & 15, rd = (in >> 12) & 15, rm = in & 15;
            uint32_t addr = c->r[rn];
            if (in & (1 << 22)) {
                uint32_t t = rd8(c, addr); wr8(c, addr, c->r[rm]);
                if (!c->dabort) c->r[rd] = t;
            } else {
                uint32_t t = ror32(rd32(c, addr & ~3u), (addr & 3) * 8);
                wr32(c, addr & ~3u, c->r[rm]);
                if (!c->dabort) c->r[rd] = t;
            }
            return;
        }
        if ((in & 0x0e000090) == 0x00000090) { arm_halfword(c, in); return; }
        if ((in & 0x0ffffff0) == 0x012fff10) { branch_interwork(c, c->r[in & 15]); return; }
        if ((in & 0x0ffffff0) == 0x012fff30) { uint32_t t = c->r[in & 15]; c->r[14] = c->cur_pc + 4; branch_interwork(c, t); return; }
        if ((in & 0x0fff0ff0) == 0x016f0f10) { uint32_t v = c->r[in & 15]; c->r[(in >> 12) & 15] = v ? __builtin_clz(v) : 32; return; }
        if ((in & 0x0f900ff0) == 0x01000050) {
            int rd = (in >> 12) & 15; int32_t m = c->r[in & 15], n = c->r[(in >> 16) & 15];
            switch ((in >> 21) & 3) {
            case 0: c->r[rd] = sat32((int64_t)m + n, c); break;
            case 1: c->r[rd] = sat32((int64_t)m - n, c); break;
            case 2: c->r[rd] = sat32((int64_t)m + sat32((int64_t)n * 2, c), c); break;
            case 3: c->r[rd] = sat32((int64_t)m - sat32((int64_t)n * 2, c), c); break;
            }
            return;
        }
        if ((in & 0x0ff000f0) == 0x01200070) { /* BKPT */
            fprintf(stderr, "[cpu] BKPT at %08x\n", c->cur_pc);
            exception(c, MODE_ABT, 0x0c, c->cur_pc + 4, false);
            return;
        }
        if ((in & 0x0f900090) == 0x01000080) { arm_dsp_mul(c, in); return; }
        if ((in & 0x0fbf0fff) == 0x010f0000) {
            c->r[(in >> 12) & 15] = (in & (1 << 22)) ? c->spsr : c->cpsr;
            return;
        }
        if ((in & 0x0fb0fff0) == 0x0120f000) { arm_msr(c, in); return; }
        arm_data_proc(c, in);
        return;
    case 1:
        if ((in & 0x0fb0f000) == 0x0320f000) { arm_msr(c, in); return; }
        arm_data_proc(c, in);
        return;
    case 2: arm_single(c, in); return;
    case 3:
        if (in & 0x10) { undefined(c, in, false); return; }
        arm_single(c, in);
        return;
    case 4: arm_ldm_stm(c, in); return;
    case 5: {
        int32_t off = (int32_t)(in << 8) >> 6;
        if (in & (1 << 24)) c->r[14] = c->cur_pc + 4;
        branch_to(c, c->cur_pc + 8 + off);
        return;
    }
    case 6: undefined(c, in, false); return;
    case 7:
        if (in & (1 << 24)) { exception(c, MODE_SVC, 0x08, c->cur_pc + 4, false); return; }
        if ((in & 0x10) && ((in >> 8) & 15) == 15) {
            int crn = (in >> 16) & 15, crm = in & 15, op1 = (in >> 21) & 7, op2 = (in >> 5) & 7, rd = (in >> 12) & 15;
            if (in & (1 << 20)) {
                uint32_t v = cp15_read(c, crn, crm, op1, op2);
                if (rd == 15) c->cpsr = (c->cpsr & 0x0fffffff) | (v & 0xf0000000);
                else c->r[rd] = v;
            } else {
                cp15_write(c, crn, crm, op1, op2, rd == 15 ? c->cur_pc + 12 : c->r[rd]);
            }
            return;
        }
        if ((in & 0x10) && ((in >> 8) & 15) == 14) { /* cp14 debug: read 0 */
            if (in & (1 << 20)) { int rd = (in >> 12) & 15; if (rd != 15) c->r[rd] = 0; }
            return;
        }
        undefined(c, in, false);
        return;
    }
}

/* ---------------------------------------------------------------- Thumb */
static void thumb_exec(CPU *c, uint32_t in) {
    uint32_t pc = c->cur_pc + 4;
    switch (in >> 11) {
    case 0x00: case 0x01: case 0x02: { /* shift imm */
        int rd = in & 7, rm = (in >> 3) & 7, amt = (in >> 6) & 31, type = in >> 11;
        int cy; uint32_t r = shift_op(c, c->r[rm], type, amt, false, &cy);
        c->r[rd] = r; set_nz(c, r); set_c(c, cy);
        return;
    }
    case 0x03: {
        int rd = in & 7, rn = (in >> 3) & 7;
        uint32_t v = (in & (1 << 10)) ? (in >> 6) & 7 : c->r[(in >> 6) & 7];
        if (in & (1 << 9)) c->r[rd] = sub_flags(c, c->r[rn], v, 1, true);
        else c->r[rd] = add_flags(c, c->r[rn], v, 0, true);
        return;
    }
    case 0x04: { int rd = (in >> 8) & 7; c->r[rd] = in & 0xff; set_nz(c, c->r[rd]); return; }
    case 0x05: { int rd = (in >> 8) & 7; sub_flags(c, c->r[rd], in & 0xff, 1, true); return; }
    case 0x06: { int rd = (in >> 8) & 7; c->r[rd] = add_flags(c, c->r[rd], in & 0xff, 0, true); return; }
    case 0x07: { int rd = (in >> 8) & 7; c->r[rd] = sub_flags(c, c->r[rd], in & 0xff, 1, true); return; }
    case 0x08:
        if (!(in & 0x400)) { /* ALU */
            int rd = in & 7, rm = (in >> 3) & 7;
            uint32_t a = c->r[rd], b = c->r[rm], r; int cy;
            switch ((in >> 6) & 15) {
            case 0x0: r = a & b; c->r[rd] = r; set_nz(c, r); break;
            case 0x1: r = a ^ b; c->r[rd] = r; set_nz(c, r); break;
            case 0x2: r = shift_op(c, a, 0, b, true, &cy); c->r[rd] = r; set_nz(c, r); set_c(c, cy); break;
            case 0x3: r = shift_op(c, a, 1, b, true, &cy); c->r[rd] = r; set_nz(c, r); set_c(c, cy); break;
            case 0x4: r = shift_op(c, a, 2, b, true, &cy); c->r[rd] = r; set_nz(c, r); set_c(c, cy); break;
            case 0x5: c->r[rd] = add_flags(c, a, b, FLAG_C(c), true); break;
            case 0x6: c->r[rd] = sub_flags(c, a, b, FLAG_C(c), true); break;
            case 0x7: r = shift_op(c, a, 3, b, true, &cy); c->r[rd] = r; set_nz(c, r); set_c(c, cy); break;
            case 0x8: set_nz(c, a & b); break;
            case 0x9: c->r[rd] = sub_flags(c, 0, b, 1, true); break;
            case 0xA: sub_flags(c, a, b, 1, true); break;
            case 0xB: add_flags(c, a, b, 0, true); break;
            case 0xC: r = a | b; c->r[rd] = r; set_nz(c, r); break;
            case 0xD: r = a * b; c->r[rd] = r; set_nz(c, r); break;
            case 0xE: r = a & ~b; c->r[rd] = r; set_nz(c, r); break;
            case 0xF: r = ~b; c->r[rd] = r; set_nz(c, r); break;
            }
            return;
        } else { /* hi reg ops / BX */
            int rd = (in & 7) | ((in >> 4) & 8), rm = (in >> 3) & 15;
            uint32_t b = rm == 15 ? pc : c->r[rm];
            switch ((in >> 8) & 3) {
            case 0: {
                uint32_t a = rd == 15 ? pc : c->r[rd];
                if (rd == 15) branch_to(c, (a + b) & ~1u); else c->r[rd] = a + b;
                break;
            }
            case 1: sub_flags(c, rd == 15 ? pc : c->r[rd], b, 1, true); break;
            case 2: if (rd == 15) branch_to(c, b & ~1u); else c->r[rd] = b; break;
            case 3:
                if (in & 0x80) c->r[14] = (c->cur_pc + 2) | 1;
                branch_interwork(c, b);
                break;
            }
            return;
        }
    case 0x09: { int rd = (in >> 8) & 7; c->r[rd] = rd32(c, (pc & ~3u) + (in & 0xff) * 4); return; }
    case 0x0a: case 0x0b: {
        int rd = in & 7, rn = (in >> 3) & 7, rm = (in >> 6) & 7;
        uint32_t addr = c->r[rn] + c->r[rm];
        uint32_t v;
        switch ((in >> 9) & 7) {
        case 0: wr32(c, addr & ~3u, c->r[rd]); break;
        case 1: wr16(c, addr & ~1u, c->r[rd]); break;
        case 2: wr8(c, addr, c->r[rd]); break;
        case 3: v = (uint32_t)(int8_t)rd8(c, addr); if (!c->dabort) c->r[rd] = v; break;
        case 4: v = ror32(rd32(c, addr & ~3u), (addr & 3) * 8); if (!c->dabort) c->r[rd] = v; break;
        case 5: v = rd16(c, addr & ~1u); if (addr & 1) v = ror32(v, 8); if (!c->dabort) c->r[rd] = v; break;
        case 6: v = rd8(c, addr); if (!c->dabort) c->r[rd] = v; break;
        case 7: v = (addr & 1) ? (uint32_t)(int8_t)rd8(c, addr) : (uint32_t)(int16_t)rd16(c, addr); if (!c->dabort) c->r[rd] = v; break;
        }
        return;
    }
    case 0x0c: { int rd = in & 7; uint32_t a = c->r[(in >> 3) & 7] + ((in >> 6) & 31) * 4; wr32(c, a & ~3u, c->r[rd]); return; }
    case 0x0d: { int rd = in & 7; uint32_t a = c->r[(in >> 3) & 7] + ((in >> 6) & 31) * 4; uint32_t v = ror32(rd32(c, a & ~3u), (a & 3) * 8); if (!c->dabort) c->r[rd] = v; return; }
    case 0x0e: { int rd = in & 7; uint32_t a = c->r[(in >> 3) & 7] + ((in >> 6) & 31); wr8(c, a, c->r[rd]); return; }
    case 0x0f: { int rd = in & 7; uint32_t a = c->r[(in >> 3) & 7] + ((in >> 6) & 31); uint32_t v = rd8(c, a); if (!c->dabort) c->r[rd] = v; return; }
    case 0x10: { int rd = in & 7; uint32_t a = c->r[(in >> 3) & 7] + ((in >> 6) & 31) * 2; wr16(c, a & ~1u, c->r[rd]); return; }
    case 0x11: { int rd = in & 7; uint32_t a = c->r[(in >> 3) & 7] + ((in >> 6) & 31) * 2; uint32_t v = rd16(c, a & ~1u); if (!c->dabort) c->r[rd] = v; return; }
    case 0x12: { int rd = (in >> 8) & 7; wr32(c, (c->r[13] + (in & 0xff) * 4) & ~3u, c->r[rd]); return; }
    case 0x13: { int rd = (in >> 8) & 7; uint32_t a = c->r[13] + (in & 0xff) * 4; uint32_t v = ror32(rd32(c, a & ~3u), (a & 3) * 8); if (!c->dabort) c->r[rd] = v; return; }
    case 0x14: { int rd = (in >> 8) & 7; c->r[rd] = (pc & ~3u) + (in & 0xff) * 4; return; }
    case 0x15: { int rd = (in >> 8) & 7; c->r[rd] = c->r[13] + (in & 0xff) * 4; return; }
    case 0x16: case 0x17:
        switch ((in >> 8) & 15) {
        case 0x0:
            if (in & 0x80) c->r[13] -= (in & 0x7f) * 4; else c->r[13] += (in & 0x7f) * 4;
            return;
        case 0x4: case 0x5: { /* PUSH */
            uint32_t list = in & 0xff; bool lr = in & 0x100;
            int n = __builtin_popcount(list) + lr;
            uint32_t addr = c->r[13] - n * 4;
            uint32_t a = addr;
            for (int i = 0; i < 8; i++) if (list & (1 << i)) { wr32(c, a, c->r[i]); a += 4; }
            if (lr) wr32(c, a, c->r[14]);
            if (!c->dabort) c->r[13] = addr;
            return;
        }
        case 0xc: case 0xd: { /* POP */
            uint32_t list = in & 0xff; bool pcb = in & 0x100;
            uint32_t a = c->r[13];
            uint32_t vals[9]; int k = 0;
            for (int i = 0; i < 8; i++) if (list & (1 << i)) { vals[k++] = rd32(c, a); a += 4; }
            uint32_t pv = 0;
            if (pcb) { pv = rd32(c, a); a += 4; }
            if (c->dabort) return;
            k = 0;
            for (int i = 0; i < 8; i++) if (list & (1 << i)) c->r[i] = vals[k++];
            c->r[13] = a;
            if (pcb) branch_interwork(c, pv);
            return;
        }
        case 0xe:
            fprintf(stderr, "[cpu] BKPT (thumb) at %08x\n", c->cur_pc);
            exception(c, MODE_ABT, 0x0c, c->cur_pc + 4, false);
            return;
        default: undefined(c, in, true); return;
        }
    case 0x18: { /* STMIA */
        int rn = (in >> 8) & 7; uint32_t list = in & 0xff; uint32_t a = c->r[rn];
        if (!list) { c->r[rn] += 0x40; return; }
        for (int i = 0; i < 8; i++) if (list & (1 << i)) { wr32(c, a, c->r[i]); a += 4; }
        if (!c->dabort) c->r[rn] = a;
        return;
    }
    case 0x19: { /* LDMIA */
        int rn = (in >> 8) & 7; uint32_t list = in & 0xff; uint32_t a = c->r[rn];
        if (!list) { c->r[rn] += 0x40; return; }
        uint32_t vals[8];
        for (int i = 0; i < 8; i++) if (list & (1 << i)) { vals[i] = rd32(c, a); a += 4; }
        if (c->dabort) return;
        c->r[rn] = a;
        for (int i = 0; i < 8; i++) if (list & (1 << i)) c->r[i] = vals[i];
        return;
    }
    case 0x1a: case 0x1b: {
        uint32_t cond = (in >> 8) & 15;
        if (cond == 0xf) { exception(c, MODE_SVC, 0x08, c->cur_pc + 2, false); return; }
        if (cond == 0xe) { undefined(c, in, true); return; }
        if (cond_pass(c, cond)) branch_to(c, pc + ((int32_t)(int8_t)(in & 0xff) << 1));
        return;
    }
    case 0x1c: branch_to(c, pc + (((int32_t)(in << 21)) >> 20)); return;
    case 0x1d: { /* BLX suffix */
        uint32_t t = (c->r[14] + ((in & 0x7ff) << 1)) & ~3u;
        c->r[14] = (c->cur_pc + 2) | 1;
        c->cpsr &= ~PSR_T;
        branch_to(c, t);
        return;
    }
    case 0x1e: c->r[14] = pc + (((int32_t)(in << 21)) >> 9); return;
    case 0x1f: {
        uint32_t t = c->r[14] + ((in & 0x7ff) << 1);
        c->r[14] = (c->cur_pc + 2) | 1;
        branch_to(c, t & ~1u);
        return;
    }
    }
}

/* ---------------------------------------------------------------- run loop */
void cpu_dump(CPU *c) {
    fprintf(stderr, "r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x r6=%08x r7=%08x\n",
            c->r[0], c->r[1], c->r[2], c->r[3], c->r[4], c->r[5], c->r[6], c->r[7]);
    fprintf(stderr, "r8=%08x r9=%08x r10=%08x r11=%08x r12=%08x sp=%08x lr=%08x pc=%08x\n",
            c->r[8], c->r[9], c->r[10], c->r[11], c->r[12], c->r[13], c->r[14], c->r[15]);
    fprintf(stderr, "cpsr=%08x spsr=%08x ctrl=%08x ttb=%08x irq=%d fiq=%d halted=%d\n", c->cpsr, c->spsr, c->cp15_ctrl, c->cp15_ttb, c->irq_line, c->fiq_line, c->halted);
}

#define PCS_N 4096
static uint32_t ct_lo = 1, ct_hi = 0; static int ct_count;
static uint32_t pcs_pc[PCS_N], pcs_cnt[PCS_N]; static unsigned pcs_n; static int pcsample = -1;
static void pcs_record(uint32_t pc) {
    uint32_t k = pc & ~0x3fu, h = (k >> 6) % PCS_N;
    for (int i = 0; i < PCS_N; i++, h = (h + 1) % PCS_N) {
        if (pcs_pc[h] == k) { pcs_cnt[h]++; return; }
        if (!pcs_cnt[h]) { pcs_pc[h] = k; pcs_cnt[h] = 1; return; }
    }
}
void cpu_pcsample_dump(void) {
    if (pcsample <= 0) return;
    for (int t = 0; t < 25; t++) {
        int best = -1;
        for (int i = 0; i < PCS_N; i++) if (pcs_cnt[i] && (best < 0 || pcs_cnt[i] > pcs_cnt[best])) best = i;
        if (best < 0) break;
        fprintf(stderr, "[pcs] %08x %u\n", pcs_pc[best], pcs_cnt[best]); pcs_cnt[best] = 0;
    }
}
bool hle_stub(CPU *c, uint32_t pc);
int hle_nstubs(void);
int cpu_run(CPU *c, int n) {
    int done = 0;
    static int hle_n = -1, dbg = -1;
    static uint32_t tracepc = 1;
    if (tracepc == 1) tracepc = getenv("TRACEPC") ? strtoul(getenv("TRACEPC"), NULL, 16) : 0xffffffff;
    if (hle_n < 0) hle_n = hle_nstubs();
    if (pcsample < 0) pcsample = getenv("PCSAMPLE") != NULL;
    if (ct_lo == 1) { ct_lo = 0; if (getenv("CALLTRACE")) sscanf(getenv("CALLTRACE"), "%x-%x", &ct_lo, &ct_hi); }
    if (dbg < 0) dbg = getenv("DRC_DEBUG") != NULL || cpu_break_addr != 0xffffffff;
    while (done < n) {
        if (c->reset_req) { bool i = c->irq_line, f = c->fiq_line; cpu_reset(c); c->irq_line = i; c->fiq_line = f; }
        if (c->fiq_line && !(c->cpsr & PSR_F)) {
            c->halted = false;
            exception(c, MODE_FIQ, 0x1c, c->r[15] + 4, true);
        } else if (c->irq_line && !(c->cpsr & PSR_I)) {
            c->halted = false;
            if (getenv("IRQDBG")) fprintf(stderr, "[cpu] IRQ taken at %08x\n", c->r[15]);
            exception(c, MODE_IRQ, 0x18, c->r[15] + 4, false);
        }
        if (c->halted) { c->cycles += n - done; return n; }

        uint32_t pc = c->r[15];
        if (pcsample && !(++pcs_n & 1023)) pcs_record(pc);
        if (pc == cpu_break_addr) {
            fprintf(stderr, "[cpu] breakpoint %08x hit at cycle %llu\n", pc, (unsigned long long)c->cycles + done);
            cpu_dump(c); cpu_trace_dump();
            for (int i = 0; i < 16; i++) fprintf(stderr, "  [sp+%02x] %08x\n", i * 4, cpu_peek32(c, c->r[13] + i * 4));
            exit(2);
        }
        if (hle_n && hle_stub(c, pc)) { done++; continue; }
        c->cur_pc = pc;
        if (pc == tracepc) fprintf(stderr, "[tracepc] %08x r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x lr=%08x [r0]=%08x\n", pc, c->r[0], c->r[1], c->r[2], c->r[3], c->r[4], c->r[5], c->r[14], cpu_peek32(c, c->r[0] & ~3u));
        if (pc == 0x0005e9c2u && dbg) fprintf(stderr, "[appmsg] %02x from %08x\n", cpu_peek32(c, c->r[0] & ~3u) >> ((c->r[0] & 3) * 8) & 0xff, c->r[14]);
        if (pc == 0x000533d8u || pc == 0x0004a3a8u) { extern void hle_check(CPU *, uint32_t); hle_check(c, pc); }
        c->branched = false;
        c->dabort = false;
        if (dbg) { trace_pc[trace_pos] = pc; trace_cpsr[trace_pos] = c->cpsr; trace_pos = (trace_pos + 1) % TRACE_N; }

        bool thumb = c->cpsr & PSR_T;
        if ((pc >> 12) != fetch_vpage) {
            uint32_t fs;
            if (!fetch_setup(c, pc, &fs)) {
                fprintf(stderr, "[cpu] prefetch abort at %08x (fs=%x)\n", pc, fs);
                cpu_dump(c);
                cpu_trace_dump();
                c->cp15_ifsr = fs; c->cp15_far = pc;
                exception(c, MODE_ABT, 0x0c, pc + 4, false);
                done++;
                if (fs == 0x100) { /* executing from MMIO/unmapped: hard stop */
                    exit(1);
                }
                continue;
            }
        }
        if (thumb) {
            uint32_t in = *(uint16_t *)(fetch_host + (pc & 0xffe));
            c->r[15] = pc + 4;
            if (cpu_trace_enabled) fprintf(stderr, "T %08x: %04x\n", pc, in);
            thumb_exec(c, in);
            if (!c->branched) c->r[15] = pc + 2;
        } else {
            uint32_t in = *(uint32_t *)(fetch_host + (pc & 0xffc));
            c->r[15] = pc + 8;
            if (cpu_trace_enabled) fprintf(stderr, "A %08x: %08x\n", pc, in);
            arm_exec(c, in);
            if (!c->branched) c->r[15] = pc + 4;
        }
        if (c->dabort) {
            exception(c, MODE_ABT, 0x10, pc + 8, false);
        }
        if (ct_hi && c->branched && pc - ct_lo <= ct_hi - ct_lo && (c->r[14] & ~1u) - pc <= 6 && ct_count < 200000) {
            ct_count++;
            fprintf(stderr, "[call] %08x -> %08x (r0=%08x r1=%08x r2=%08x)\n", pc, c->r[15], c->r[0], c->r[1], c->r[2]);
        }
        if (dbg && c->branched) {
            uint32_t npc = c->r[15];
            if (npc - 0x88000u < 0x4000u && (c->r[14] & ~1u) - pc <= 6 && !(pc - 0x88000u < 0x4000u)) {
                /* outermost call into the kernel from task code */
                for (int i = 0; i < CALLN; i++) if (!pend[i].t) {
                    pend[i].t = npc; pend[i].r0 = c->r[0]; pend[i].r1 = c->r[1]; pend[i].r2 = c->r[2];
                    pend[i].lr = c->r[14] & ~1u; pend[i].sp = c->r[13]; pend[i].cyc = c->cycles + done; break;
                }
            } else {
                for (int i = 0; i < CALLN; i++) if (pend[i].t && pend[i].lr == npc && pend[i].sp == c->r[13]) { pend[i].t = 0; break; }
            }
        }
        if (c->branched) fetch_vpage = (c->r[15] >> 12) == fetch_vpage ? fetch_vpage : 0xffffffff;
        done++;
    }
    c->cycles += done;
    return done;
}
