#include "bus.h"
#include "cpu.h"
#include <stdlib.h>
#include <string.h>

uint8_t *page_r[1 << 20];
uint8_t *page_w[1 << 20];
uint8_t *ram;
uint8_t *bootrom;
int log_mmio = 0;

typedef struct {
    uint32_t base, size;
    const char *name;
    void *ctx;
    mmio_read_fn r;
    mmio_write_fn w;
} Region;

static Region regions[64];
static int nregions;

void bus_init(void) {
    ram = calloc(1, RAM_SIZE);
    if (getenv("RAMFILL")) memset(ram, (int)strtol(getenv("RAMFILL"), NULL, 0), RAM_SIZE);   /* canary: find unused RAM */
    bootrom = calloc(1, 0x1000);
    for (uint32_t a = 0; a < RAM_SIZE; a += 0x1000) {
        page_r[(RAM_BASE + a) >> 12] = ram + a;
        page_w[(RAM_BASE + a) >> 12] = ram + a;
    }
}

void bus_map_rom(bool mapped) {
    page_r[0] = mapped ? bootrom : ram;
    cpu_tlb_flush();
}

void bus_register(uint32_t base, uint32_t size, const char *name, void *ctx, mmio_read_fn r, mmio_write_fn w) {
    Region *g = &regions[nregions++];
    g->base = base; g->size = size; g->name = name; g->ctx = ctx; g->r = r; g->w = w;
}

static Region *find(uint32_t pa) {
    for (int i = nregions - 1; i >= 0; i--)
        if (pa - regions[i].base < regions[i].size) return &regions[i];
    return NULL;
}

/* remember which unknown addresses were already reported */
#define SEEN_N 4096
static uint32_t seen[SEEN_N];
static bool first_time(uint32_t key) {
    uint32_t h = (key * 2654435761u) % SEEN_N;
    for (int i = 0; i < SEEN_N; i++) {
        uint32_t j = (h + i) % SEEN_N;
        if (seen[j] == key) return false;
        if (seen[j] == 0) { seen[j] = key; return true; }
    }
    return false;
}

extern CPU cpu;

static uint32_t trace_lo = 1, trace_hi = 0;
static void trace_init(void) {
    static int done; if (done) return; done = 1;
    const char *e = getenv("MMIOTRACE");
    if (e) { sscanf(e, "%x-%x", &trace_lo, &trace_hi); }
}
#define TRACED(pa) ((pa) >= trace_lo && (pa) <= trace_hi)

uint32_t bus_mmio_read(uint32_t pa, int size) {
    trace_init();
    Region *g = find(pa);
    if (g && g->r && TRACED(pa)) {
        uint32_t v = g->r(g->ctx, pa - g->base, size);
        fprintf(stderr, "[trace] R%d %08x -> %08x pc=%08x lr=%08x\n", size * 8, pa, v, cpu.cur_pc, cpu.r[14]);
        return v;
    }
    if (g && g->r) {
        uint32_t v = g->r(g->ctx, pa - g->base, size);
        if (log_mmio) fprintf(stderr, "[mmio] R%d %08x (%s+%x) -> %08x  pc=%08x\n", size * 8, pa, g->name, pa - g->base, v, cpu.cur_pc);
        return v;
    }
    if (getenv("DRC_DEBUG") && first_time(pa | 1))
        fprintf(stderr, "[bus] unhandled read%d  %08x pc=%08x\n", size * 8, pa, cpu.cur_pc);
    return 0;
}

void bus_mmio_write(uint32_t pa, uint32_t val, int size) {
    trace_init();
    Region *g = find(pa);
    if (TRACED(pa)) { extern uint64_t emu_total_cycles; fprintf(stderr, "[trace] W%d %08x <- %08x pc=%08x lr=%08x t=%.3f\n", size * 8, pa, val, cpu.cur_pc, cpu.r[14], emu_total_cycles / 108e6); }
    if (g && g->w) {
        if (log_mmio) fprintf(stderr, "[mmio] W%d %08x (%s+%x) <- %08x  pc=%08x\n", size * 8, pa, g->name, pa - g->base, val, cpu.cur_pc);
        g->w(g->ctx, pa - g->base, val, size);
        return;
    }
    if (getenv("DRC_DEBUG") && first_time(pa | 2))
        fprintf(stderr, "[bus] unhandled write%d %08x <- %08x pc=%08x\n", size * 8, pa, val, cpu.cur_pc);
}

void bus_dma_write(uint32_t pa, const uint8_t *src, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint8_t *p = page_w[(pa + i) >> 12];
        if (p) p[(pa + i) & 0xfff] = src[i];
        else bus_mmio_write(pa + i, src[i], 1);
    }
}
void bus_dma_read(uint32_t pa, uint8_t *dst, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint8_t *p = page_r[(pa + i) >> 12];
        dst[i] = p ? p[(pa + i) & 0xfff] : (uint8_t)bus_mmio_read(pa + i, 1);
    }
}
