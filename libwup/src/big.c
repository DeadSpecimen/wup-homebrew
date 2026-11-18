/* Code in WUP_BIGMEM (app.cfg: text_in_big). A large app's code goes in two segments the loader places (192 KB at
 * 0x350000, 368 KB in WUP_SPARE); what does not fit is stored in the .wup after them (wupfmt.h, big_*), raw-deflated,
 * and put at the start of WUP_BIGMEM here, before main() (the app's .bss and heap follow it). Only built into apps
 * that have such code (tools/wupbuild.py adds this file and tinf). */
#include <stdlib.h>
#include "wup.h"
#include "wupfmt.h"
#include "armcpu.h"
#include "fs.h"
#include "tinf.h"

extern uint8_t __big_start[], __big_end[];
int wup__big_load(void) {                          /* 0, or < 0: the app cannot run (its code is missing) */
    wup_hdr_t h; uint32_t want = (uint32_t)(__big_end - __big_start);
    if (!want) return 0;
    if (fs_read(wup_app_path(), 0, &h, sizeof h) != (int)sizeof h || h.magic != WUP_MAGIC || h.hdr_size < WUP_HDR_V4
        || h.big_addr != (uint32_t)__big_start || h.big_raw != want) { wup_log("[libwup] this app's code for WUP_BIGMEM is missing\n"); return -1; }
    uint8_t *z = malloc(h.big_size);
    if (!z) return -2;
    int r = fs_read(wup_app_path(), h.big_off, z, h.big_size) == (int)h.big_size && crc32(z, h.big_size) == h.big_crc ? 0 : -3;
    unsigned n = want;
    if (!r && (tinf_uncompress(__big_start, &n, z, h.big_size) != TINF_OK || n != want)) r = -4;
    free(z);
    if (r) { wup_log("[libwup] this app's code for WUP_BIGMEM is damaged\n"); return r; }
    sync_code((uint32_t)__big_start, want);
    return 0;
}
