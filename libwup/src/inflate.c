/* inflate: raw deflate data (zip entries, .wup images) for apps, with tinf (github.com/jibsen/tinf, Zlib licence) */
#include "wup.h"
#include "tinf.h"
int wup_inflate(void *out, unsigned *out_len, const void *in, unsigned in_len) {
    return tinf_uncompress(out, out_len, in, in_len) == TINF_OK ? 0 : -1;
}
