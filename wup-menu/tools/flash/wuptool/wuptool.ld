/* wuptool's layout: one piece at 0x10000, where the boot ROM's UART loader lands after its 64 KB of zeros
   (docs/install.md). Stacks at the top of the 4 MB of RAM. */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY { ram : ORIGIN = 0x10000, LENGTH = 0x3F0000 }

__sp_svc = 0x3FFFFC;
__sp_irq = __sp_svc - 0x100;
__sp_usr = __sp_irq - 0x100;

PHDRS { main PT_LOAD FLAGS(7); }

SECTIONS
{
    .crt0 : { KEEP(*(.crt0)) . = ALIGN(4); } >ram :main = 0x00
    .text : { *(.text) *(.stub) *(.text.*) *(.gnu.warning) *(.gnu.linkonce.t*) *(.glue_7) *(.glue_7t) . = ALIGN(4); } >ram :main = 0xff
    .rodata : { *(.rodata) *all.rodata*(*) *(.roda) *(.rodata.*) *(.gnu.linkonce.r*) . = ALIGN(4); } >ram :main = 0xff
    .data ALIGN(4) : { *(.data) *(.data.*) *(.gnu.linkonce.d*) . = ALIGN(4); } >ram :main = 0xff
    .bss (NOLOAD) : {
        __bss_start__ = ABSOLUTE(.);
        *(.dynbss) *(.gnu.linkonce.b*) *(.bss*) *(COMMON)
        . = ALIGN(4);
        __bss_end__ = ABSOLUTE(.);
    } >ram :NONE
}
