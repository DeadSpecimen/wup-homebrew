@ Boot hook, patched into the stock firmware (LVC rev 7677, NR4_12_20140805_release) in place.
@
@ The app manager starts the boot app by calling get_boot_mode(&app_id) (LVC 0x30734) from 0x63a0e;
@ the installs redirect that call here. This stub calls the original and, on a normal boot (app id 0), loads the
@ loader to LAUNCHER_RAM, checks its "CFW1" header and calls it. The loader may change the app id and returns; the
@ firmware then starts whatever app id it left. Anything wrong: the stock boot, untouched.
@
@ Where the loader comes from:
@   1. the bundle in the running bank: BANK + BUNDLE_OFF, after the firmware's last section. The Wii U install
@      (installer/) puts it there; the bank is the one the bank selector byte (flash 0xF000) boots, the way stage 2
@      picks it: 1 = Product2 (0x500000), anything else Product1 (0x100000).
@   2. else flash LOADER_FLASH (0x10000, the free gap below Product1: the UART install, libwup/src/fs.c).
@
@ Lives in 160 bytes of dead data: two adjacent unreferenced WPS-CLI debug strings at 0xb951c.
@ The flash driver reads by DMA, into RAM at 0x178000 or above: every read goes to LAUNCHER_RAM.
@ Everything here was derived from the firmware itself (see docs/architecture.md).

    .syntax unified
    .thumb
    .text
    .global _start
    .thumb_func
_start:
    push    {r4, r5, r6, lr}
    sub     sp, #8
    movs    r4, r0                  @ r4 = &app_id
    ldr     r3, =0x30735            @ original get_boot_mode (thumb)
    blx     r3
    movs    r5, r0                  @ keep its result (0 = ok)
    cmp     r0, #0
    bne     out
    ldrb    r1, [r4]
    cmp     r1, #0                  @ normal boot only (not pairing, TV remote, DK menu, ...)
    bne     out
    movs    r5, #0
    movs    r1, #0xf                @ the bank selector byte
    lsls    r1, r1, #12
    movs    r3, #1
    bl      fread
    bne     gap
    ldrb    r2, [r6]
    movs    r1, #1                  @ Product1 (0x100000)
    cmp     r2, #1
    bne     1f
    movs    r1, #5                  @ Product2 (0x500000)
1:  lsls    r1, r1, #20
    ldr     r2, =BUNDLE_OFF
    adds    r1, r1, r2
    bl      try
gap:
    ldr     r1, =LOADER_FLASH
    bl      try
    b       out

@ try(r1 = flash address): the loader from there; returns only if there is none
try:
    push    {lr}
    movs    r3, #(LOADER_MAX >> 8)  @ (8 KB)
    lsls    r3, r3, #8
    bl      fread
    bne     1f
    ldr     r2, [r6]                @ header: magic, entry
    ldr     r3, =0x31574643         @ "CFW1"
    cmp     r2, r3
    bne     1f
    ldr     r3, [r6, #4]
    movs    r0, r4
    blx     r3                      @ loader_main(&app_id)
    movs    r0, r5
    add     sp, #12                 @ (try's lr, then out's frame)
    pop     {r4, r5, r6, pc}
1:  pop     {pc}
out:
    movs    r0, r5
    add     sp, #8
    pop     {r4, r5, r6, pc}

@ flash_read(handle, r1 = flash address, LAUNCHER_RAM, r3 = length, &bytes_read); r6 = LAUNCHER_RAM;
@ the flags say whether it worked (eq)
fread:
    push    {r4, lr}
    sub     sp, #8
    add     r2, sp, #4
    str     r2, [sp]
    ldr     r0, =0xbb5f0
    ldr     r0, [r0, #4]
    movs    r2, #(LAUNCHER_RAM >> 16)   @ (LAUNCHER_RAM: 64 KB aligned)
    lsls    r2, r2, #16
    movs    r6, r2
    ldr     r4, =0x9141             @ flash_read (thumb)
    blx     r4
    cmp     r0, #0
    add     sp, #8
    pop     {r4, pc}
    .ltorg
