@ mem: memset, memcpy, memmove and bzero for every app, a word at a time. The C library's (picolibc, built for size)
@ go a byte at a time; a Game Boy frame clears and copies about 200 KB, and that was 40 % of WUP Boy's time.
@ ARM state (ARM926EJ-S, ARMv5TE), callable from ARM and Thumb code. These replace the library's (an app's own
@ objects are linked before it), under the names the compiler calls them by as well: __aeabi_memcpy, __aeabi_memmove,
@ __aeabi_memset (dst, n, c: its own order) and __aeabi_memclr, each also as ...4 and ...8 (aligned operands).
    .syntax unified
    .arm
    .text

@ void *memset(void *dst, int c, size_t n)
    .global memset
    .type memset, %function
memset:
    mov     r3, r0
    and     r1, r1, #255
    cmp     r2, #16
    blo     3f
    orr     r1, r1, r1, lsl #8
    orr     r1, r1, r1, lsl #16
1:  tst     r3, #3                  @ up to a word boundary
    strbne  r1, [r3], #1
    subne   r2, r2, #1
    bne     1b
2:  subs    r2, r2, #16             @ 16 bytes a turn
    strhs   r1, [r3], #4
    strhs   r1, [r3], #4
    strhs   r1, [r3], #4
    strhs   r1, [r3], #4
    bhs     2b
    add     r2, r2, #16
3:  subs    r2, r2, #1              @ what is left
    strbhs  r1, [r3], #1
    bhs     3b
    bx      lr
    .size memset, . - memset

@ void bzero(void *dst, size_t n)
    .global bzero
    .type bzero, %function
bzero:
    mov     r2, r1
    mov     r1, #0
    b       memset
    .size bzero, . - bzero

@ void *memmove(void *dst, const void *src, size_t n): forwards unless dst lies inside the source
    .global memmove
    .type memmove, %function
memmove:
    subs    r3, r0, r1              @ dst - src
    bxeq    lr
    cmp     r3, r2                  @ (unsigned) dst - src >= n: no overlap that matters going forwards
    bhs     memcpy
    add     r1, r1, r2              @ backwards, a byte at a time (rare)
    add     r3, r0, r2
4:  subs    r2, r2, #1
    ldrbhs  r12, [r1, #-1]!
    strbhs  r12, [r3, #-1]!
    bhs     4b
    bx      lr
    .size memmove, . - memmove

@ void *memcpy(void *dst, const void *src, size_t n)
    .global memcpy
    .type memcpy, %function
memcpy:
    mov     r3, r0
    cmp     r2, #16
    blo     7f
    eor     r12, r0, r1             @ words only if both reach a boundary together
    tst     r12, #3
    bne     7f
5:  tst     r3, #3
    ldrbne  r12, [r1], #1
    strbne  r12, [r3], #1
    subne   r2, r2, #1
    bne     5b
6:  subs    r2, r2, #16
    ldrhs   r12, [r1], #4
    strhs   r12, [r3], #4
    ldrhs   r12, [r1], #4
    strhs   r12, [r3], #4
    ldrhs   r12, [r1], #4
    strhs   r12, [r3], #4
    ldrhs   r12, [r1], #4
    strhs   r12, [r3], #4
    bhs     6b
    add     r2, r2, #16
7:  subs    r2, r2, #1
    ldrbhs  r12, [r1], #1
    strbhs  r12, [r3], #1
    bhs     7b
    bx      lr
    .size memcpy, . - memcpy

    .macro alias name, to
    .global \name
    .type \name, %function
    .set \name, \to
    .endm
    alias __aeabi_memcpy, memcpy
    alias __aeabi_memcpy4, memcpy
    alias __aeabi_memcpy8, memcpy
    alias __aeabi_memmove, memmove
    alias __aeabi_memmove4, memmove
    alias __aeabi_memmove8, memmove
    alias __aeabi_memclr, bzero
    alias __aeabi_memclr4, bzero
    alias __aeabi_memclr8, bzero

@ void __aeabi_memset(void *dst, size_t n, int c)
    .global __aeabi_memset
    .type __aeabi_memset, %function
__aeabi_memset:
    mov     r3, r1
    mov     r1, r2
    mov     r2, r3
    b       memset
    .size __aeabi_memset, . - __aeabi_memset
    alias __aeabi_memset4, __aeabi_memset
    alias __aeabi_memset8, __aeabi_memset
