/* High-level hooks: capture the firmware's compiled-out debug printf. */
#include "dev.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PRINTF_ADDR 0x000533d8u
int hle_printf_enabled = 1;

static void read_str(uint32_t va, char *out, int max) {
    int i = 0;
    for (; i < max - 1; i++) {
        uint32_t w = cpu_peek32(&cpu, (va + i) & ~3u);
        char ch = (char)(w >> (((va + i) & 3) * 8));
        if (!ch) break;
        out[i] = ch;
    }
    out[i] = 0;
}

static void fw_printf(CPU *c, int fmtreg, const char *tag) {
    char fmt[512], out[1024]; int o = 0;
    read_str(c->r[fmtreg], fmt, sizeof fmt);
    int argi = fmtreg + 1;
    uint32_t sp = c->r[13];
#define NEXT() (argi <= 3 ? c->r[argi++] : cpu_peek32(c, sp + 4 * (argi++ - 4)))
    for (char *p = fmt; *p && o < 900; p++) {
        if (*p != '%') { out[o++] = *p; continue; }
        char spec[16]; int n = 0; spec[n++] = '%'; p++;
        while (*p && strchr("-+ #0123456789.l", *p) && n < 12) { if (*p != 'l') spec[n++] = *p; p++; }
        if (!*p) break;
        char cv = *p;
        if (cv == '%') { out[o++] = '%'; continue; }
        spec[n++] = (cv == 'p') ? 'x' : cv; spec[n] = 0;
        if (cv == 's') { char s[256]; read_str(NEXT(), s, sizeof s); o += snprintf(out + o, 1024 - o, spec, s); }
        else if (cv == 'd' || cv == 'i') o += snprintf(out + o, 1024 - o, spec, (int)NEXT());
        else o += snprintf(out + o, 1024 - o, spec, NEXT());
        if (o > 1000) o = 1000;
    }
    out[o] = 0;
    while (o && (out[o - 1] == '\n' || out[o - 1] == '\r')) out[--o] = 0;
    fprintf(stderr, "[%s] %s\n", tag, out);
}

/* function-return stubs: when pc hits addr, return val immediately */
#define MAXSTUB 32
static uint32_t stub_addr[MAXSTUB], stub_val[MAXSTUB]; static int nstub;
static void stubs_init(void) {
    static int done; if (done) return; done = 1;
    const char *e = getenv("HLE_RET");
    while (e && *e && nstub < MAXSTUB) {
        unsigned a, v; int n;
        if (sscanf(e, "%x:%x%n", &a, &v, &n) != 2) break;
        stub_addr[nstub] = a; stub_val[nstub++] = v;
        e += n; if (*e == ',') e++;
    }
}
bool hle_stub(CPU *c, uint32_t pc) {
    stubs_init();
    for (int i = 0; i < nstub; i++) if (stub_addr[i] == pc) {
        c->r[0] = stub_val[i];
        uint32_t lr = c->r[14];
        if (lr & 1) { c->cpsr |= PSR_T; c->r[15] = lr & ~1u; } else { c->cpsr &= ~PSR_T; c->r[15] = lr & ~3u; }
        return true;
    }
    return false;
}
int hle_nstubs(void) { stubs_init(); return nstub; }

void hle_check(CPU *c, uint32_t pc) {
    if (!hle_printf_enabled) return;
    if (pc == PRINTF_ADDR) fw_printf(c, 0, "fw");
    else if (pc == 0x0004a3a8u) fw_printf(c, 1, "fw2");
}
