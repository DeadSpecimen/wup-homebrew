/* UIC: the Renesas microcontroller that owns buttons, sticks, sensors, power and battery.
 * It talks to the SoC over SPI. Commands are one byte, followed by a fixed-length reply. */
#include "dev.h"
#include "uic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t cmd[256]; static int ncmd;
static uint8_t resp[256]; static int nresp, presp;
int uic_log = 0;

UicInput uic_input = {
    .lx = 0x800, .ly = 0x800, .rx = 0x800, .ry = 0x800,
    .battery = 0x60,
    .volume = 0xc0,          /* volume slider (0 = min, 0xff = max) */
};
static uint8_t uic_seq;
uint8_t uic_boot_state = 0x2f;
uint8_t uic_boot_param = 0;
uint8_t uic_sub_param = 0;
int uic_reboot_request = -1;
bool uic_powered_off;       /* SoC power cut by the UIC (cmd 01 04) */
/* SYNC (back) and TV (front) are handled by the UIC itself: it restarts the SoC with the
 * requested app mode, which the firmware reads back with command 0x05. */
void uic_press_special(int mode) { uic_reboot_request = mode; }

static void put16le(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

/* ---- UIC config EEPROM (addresses 0x1000-0x17FF; firmware config lives at 0x1100-0x16FF) ---- */
#define EE_BASE 0x1000
#define EE_SIZE 0x800
static uint8_t eeprom[EE_SIZE];
static const char *ee_path;              /* firmware/uic_eeprom.bin (see fw_path in main.c) */

static uint16_t crc16_8408(const uint8_t *p, int n) {
    uint16_t c = 0xffff;
    for (int i = 0; i < n; i++) {
        c ^= p[i];
        for (int b = 0; b < 8; b++) c = (c & 1) ? (c >> 1) ^ 0x8408 : c >> 1;
    }
    return c;
}
/* write a checksummed config block (data + crc16 LE) and its backup copy at +0x80 */
static void ee_block(uint32_t addr, const uint8_t *data, int n) {
    uint8_t *p = &eeprom[addr - EE_BASE];
    memcpy(p, data, n);
    uint16_t c = crc16_8408(p, n);
    p[n] = c; p[n + 1] = c >> 8;
    memcpy(p + 0x80, p, n + 2);
}
static void ee_save(void) {
    FILE *f = fopen(ee_path, "wb");
    if (f) { fwrite(eeprom, 1, EE_SIZE, f); fclose(f); }
}
static uint32_t uic_version;
/* The UIC must report the same version as the UMI_ image in flash, or the firmware reflashes it. */
static void find_umi_version(void) {
    uint32_t idx = 0x100000;   /* partition index of bank 0 */
    for (int i = 0; i < 16; i++) {
        const uint8_t *e = flash + idx + i * 16;
        if (!memcmp(e + 8, "UMI_", 4)) { memcpy(&uic_version, e + 12, 4); return; }
    }
}
void uic_init(void) {
    extern const char *fw_path(const char *name);
    if (!ee_path) ee_path = getenv("UIC_EEPROM") ? getenv("UIC_EEPROM") : fw_path("uic_eeprom.bin");
    find_umi_version();
    if (getenv("UIC_SUBPARAM")) uic_sub_param = strtol(getenv("UIC_SUBPARAM"), NULL, 0);
    if (getenv("UIC_BOOTPARAM")) uic_boot_param = strtol(getenv("UIC_BOOTPARAM"), NULL, 0);
    FILE *f = fopen(ee_path, "rb");
    if (f && fread(eeprom, 1, EE_SIZE, f) == EE_SIZE) { fclose(f); return; }
    if (f) fclose(f);
    memset(eeprom, 0xff, EE_SIZE);
    uint8_t board = 0x21;   /* main version 1, sub version 2 */
    uint8_t region = 0x01;  /* USA */
    ee_block(0x1200, &board, 1);
    ee_block(0x1203, &region, 1);
    ee_save();
}

/* 128-byte input report (same layout the GamePad later forwards to the console) */
static void build_report(uint8_t *r) {
    memset(r, 0, 128);
    UicInput *in = &uic_input;
    r[0] = uic_seq++;
    r[2] = in->buttons >> 8;          /* buttons, big endian: A B X Y LEFT RIGHT UP DOWN | ZL ZR L R + - HOME SYNC */
    r[3] = in->buttons & 0xff;        /* (decoded by the key table at LVC 0xb195c) */
    r[4] = in->power_status;          /* bit0 AC, bit1 power button, ... */
    r[5] = in->battery;
    put16le(r + 6, in->lx);
    put16le(r + 8, in->ly);
    put16le(r + 10, in->rx);
    put16le(r + 12, in->ry);
    r[14] = in->volume;
    /* accelerometer: 1g on Z when lying flat */
    put16le(r + 15, 0); put16le(r + 17, 0); put16le(r + 19, 0x1000);
    /* touch: 10 samples of (x,y) with pressure/valid bits */
    for (int i = 0; i < 10; i++) {
        uint16_t x = in->touch_down ? (in->tx & 0xfff) | 0x8000 : 0;
        uint16_t y = in->touch_down ? (in->ty & 0xfff) | 0x8000 : 0;
        put16le(r + 36 + i * 4, x);
        put16le(r + 38 + i * 4, y);
    }
    r[80] = in->extra_buttons;
    /* debug: HID_POKE=byte:mask@start_ms-end_ms forces bits in the raw report */
    static int pb = -2, pm; static double ps, pe;
    if (pb == -2) { const char *e = getenv("HID_POKE"); pb = -1; if (e) sscanf(e, "%x:%x@%lf-%lf", &pb, &pm, &ps, &pe); }
    if (pb >= 0) { extern uint64_t emu_total_cycles; double ms = emu_total_cycles / 108000.0; if (ms >= ps && ms < pe) r[pb] |= pm; }
    r[127] = r[0] ^ 0xff;
}

static void uic_command(void) {
    nresp = presp = 0;
    switch (cmd[0]) {
    case 0x7f: resp[nresp++] = uic_boot_state; break;            /* boot/power state: normal */
    case 0x05: resp[nresp++] = uic_boot_param; break;    /* boot reason / boot parameter */
    case 0x0b:                                          /* UIC firmware version (big endian) */
        resp[0] = uic_version; resp[1] = uic_version >> 8; resp[2] = uic_version >> 16; resp[3] = uic_version >> 24;
        nresp = 4; break;
    case 0x07: build_report(resp); nresp = 128; break;  /* HID input report */
    case 0x13: resp[nresp++] = 0x01; break;            /* ready */
    case 0x10: {                                         /* battery level 0..6 (the pad's own scale; UIC_BATTERY, default 5) */
        const char *b = getenv("UIC_BATTERY"); int lv = b ? atoi(b) : 5;
        resp[nresp++] = (uint8_t)(lv < 0 ? 0 : lv > 6 ? 6 : lv); break;
    }
    case 0x1c: resp[0] = uic_sub_param; resp[1] = resp[2] = resp[3] = 0; nresp = 4; break;
    case 0x03: break;                                   /* EEPROM read: handled once address+length arrive */
    default: break;
    }
}

void uic_cs(bool active) {
    if (!active && ncmd >= 2 && cmd[0] == 0x01 && cmd[1] == 0x04 && !uic_powered_off) {  /* power off */
        fprintf(stderr, "[uic] GamePad powered off (press POWER to turn it back on)\n");
        uic_powered_off = true;
    }
    /* 01 <mode>: the SoC asks the UIC to restart it in another mode. The firmware's MC_STATE module
       (LVC 0x30932) maps its app ids back to raw UIC modes (inverse of the boot-mode table at
       0x30734) and sends 01 <raw mode>; 01 04 is power off (above), 01 03 is sent while POWER is
       held (part of the power-off sequence). Other modes restart the SoC: inferred from that mapping. */
    if (!active && ncmd >= 2 && cmd[0] == 0x01 && cmd[1] != 0x03 && cmd[1] != 0x04 && cmd[1] <= 14) {
        fprintf(stderr, "[uic] SoC restart requested into mode %d\n", cmd[1]);
        uic_reboot_request = cmd[1];
    }
    if (!active && ncmd >= 4 && cmd[0] == 0x02) {      /* EEPROM write: 02 AH AL LEN data... */
        uint32_t a = (cmd[1] << 8) | cmd[2]; int n = cmd[3];
        for (int i = 0; i < n && 4 + i < ncmd; i++) {
            uint32_t e = a + i - EE_BASE;
            if (e < EE_SIZE) eeprom[e] = cmd[4 + i];
        }
        ee_save();
    }
    if (active) { ncmd = 0; nresp = presp = 0; }
    else if (ncmd && (uic_log || getenv("UIC_LOG_ALL") || ((getenv("DRC_DEBUG") || getenv("UIC_LOG")) && cmd[0] != 0x07))) {
        static int quiet;
        if (quiet < 200) {
            quiet++;
            fprintf(stderr, "[uic] cmd:");
            for (int i = 0; i < ncmd && i < 32; i++) fprintf(stderr, " %02x", cmd[i]);
            extern CPU cpu;
            fprintf(stderr, "  (pc %08x lr %08x; stack code ptrs:", cpu.cur_pc, cpu.r[14]);
            if (getenv("UIC_LOG_STACK")) {   /* return addresses on the stack: who issued this command */
                extern uint32_t cpu_peek32(CPU *, uint32_t);
                for (int i = 0; i < 48; i++) { uint32_t v = cpu_peek32(&cpu, cpu.r[13] + i * 4); if ((v & 1) && v > 0x10000 && v < 0xc0000) fprintf(stderr, " %08x", v); }
            }
            fprintf(stderr, ")\n");
        }
    }
}

uint8_t uic_xfer(uint8_t out, bool rx) {
    if (!rx) {
        if (ncmd < 256) cmd[ncmd++] = out;
        if (ncmd == 1) uic_command();
        if (cmd[0] == 0x03 && ncmd == 4) {               /* 03 AH AL LEN -> LEN bytes */
            uint32_t a = (cmd[1] << 8) | cmd[2]; int n = cmd[3] ? cmd[3] : 256;
            for (int i = 0; i < n; i++) {
                uint32_t e = a + i - EE_BASE;
                resp[i] = e < EE_SIZE ? eeprom[e] : 0xff;
            }
            nresp = n; presp = 0;
        }
        return 0xff;
    }
    if (presp < nresp) return resp[presp++];
    return 0x00;
}
