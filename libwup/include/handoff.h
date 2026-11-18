/* The hand-off block between the loader and what it runs (0x34F000, in the loader's 64 KB slot
 * 0x340000..0x350000: uncached, outside every app area).
 *
 * The menu is an app like any other. The loader keeps nothing in memory but itself: it puts an app image where
 * it belongs and runs it; when the app returns WUP_HANDOFF_RUN it has staged another image (libwup's wup_exec), which
 * the loader runs next; after any other app the menu is loaded again from its flash slot. So every app owns all
 * of the RAM.
 * A staged image is at WUP_HANDOFF_STAGE (the screen's framebuffer memory: the screen is off then), stage_size bytes,
 * deflated (zlib) when stage_raw is not 0 (its size inflated). The loader inflates it to WUP_HANDOFF_WORK, copies the
 * first segment to 0x350000 and a second to WUP_SPARE (include/wupfmt.h), and calls its entry. */
#pragma once
#include <stdint.h>
#define WUP_HANDOFF_ADDR   0x0034f000u
#define WUP_HANDOFF_MAGIC  0x42444150u          /* "PADB" */
#define WUP_HANDOFF_RUN    0x100                /* an app's return value: run the image I staged */
#define WUP_HANDOFF_STAGE  0x00221400u          /* a staged image (up to 0xD9000 bytes) */
#define WUP_HANDOFF_WORK   0x00190000u          /* where the loader inflates one (up to 0x91400 bytes) */
#define WUP_HANDOFF_WORK_MAX 0x00091400u
#define WUP_HANDOFF_FS_BOUNCE 0x0034e000u       /* 4 KB uncached, DMA-reachable: libwup's flash bounce buffer */
enum {
    WUP_HO_RADIO_READY = 1,                     /* the radio is patched and up: the menu skips that step */
    WUP_HO_WIFI_JOINED = 2,                     /* it was associated when the app started */
    WUP_HO_NET_INFO = 8,                        /* net_* below hold the menu's live connection (libwup's networking) */
};
enum { WUP_UI_SET = 1, WUP_UI_DARK = 2, WUP_UI_QUIET = 4, WUP_UI_24H = 8 };   /* ui: set by the menu; dark appearance; no sound effects; 24-hour clock */
typedef struct {
    uint32_t magic, version;                /* WUP_HANDOFF_MAGIC, 2 */
    uint32_t flags, code_size;              /* WUP_HO_*; size of the app image at 0x350000 */
    uint32_t menu_sel, runs;                /* home-screen selection to restore; apps started this boot */
    char app_path[64];                      /* "/apps/x.wup" ("" from wuplink) */
    char arg[192];                          /* an argument for the app (e.g. a ROM path); "" = none */
    /* the network the menu was on (WUP_HO_NET_INFO): the app's lwIP takes it over without DHCP */
    uint32_t net_ip, net_mask, net_gw, net_dns;   /* as lwIP stores them (network byte order in memory) */
    uint8_t net_mac[6];
    uint8_t light;                          /* the menu's screen brightness + 1 (1..101 = 0..100 %; 0: not set) */
    uint8_t ui;                             /* the menu's look and sound for apps that match it: WUP_UI_* */
    uint32_t stage_size, stage_raw;         /* the staged image (WUP_HANDOFF_RUN): bytes at WUP_HANDOFF_STAGE; inflated size or 0 */
    /* (after the loader's part, which it clears at power-on: valid only with time_magic and time_check, src/time.c)
       the date and time at a reading of the 997.3 kHz counter (0xF0000408), and the time zone (a POSIX TZ rule) */
    uint32_t time_magic, time_check, time_s_lo, time_s_hi, time_us, time_tick;
    char time_zone[48];
    char menu_version[16];                  /* the Homebrew Menu that started this app: "1.0.0" (Software Update) */
} wup_handoff_t;
#define WUP_HANDOFF ((volatile wup_handoff_t *)WUP_HANDOFF_ADDR)
