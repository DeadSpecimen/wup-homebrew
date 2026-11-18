/* fwpatch: the GamePad firmware with the Homebrew Menu built in, made from the console's own copy of Nintendo's
 * (drc_fw.bin), on the console. Plain C: the installer and the host test (test/) build the same file. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* orig: drc_fw.bin (its 16-byte header and the firmware); payload: what wup-menu's tools/mkinstaller.py made.
   0 and *out (malloc'd, *out_len bytes) on success; else -1 and *err says why. */
int fwpatch(const uint8_t *orig, size_t orig_len, const uint8_t *payload, size_t payload_len,
            uint8_t **out, size_t *out_len, const char **err);

/* the checks fwpatch makes of a drc_fw.bin: 0 = it is the firmware the payload is for */
int fwpatch_check(const uint8_t *orig, size_t orig_len, const uint8_t *payload, size_t payload_len, const char **err);

uint32_t fwpatch_crc32(const uint8_t *p, size_t n);

#ifdef __cplusplus
}
#endif
