#pragma once
int mmu_cache(unsigned lo, unsigned hi);   /* make [lo, hi) cacheable (64 KB aligned, coarse-table large pages) */
void mmu_uncache(void);                    /* clean + invalidate it and restore the firmware's mapping */
void mmu_uncache_last(void);           /* restore just the most recent range */
int mmu_cache_section(unsigned mb_addr, unsigned hi);   /* [mb_addr, hi) of a 1 MB section (e.g. 0x200000); the rest as it was */
void mmu_uncache_all(void);                /* sections + ranges back to the firmware mapping */
