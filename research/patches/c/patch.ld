/* SPDX-License-Identifier: MIT */
/* C patches for stock 2.05: code in free flash after the stock image and the asm patches, state in free RAM above
   2.05's BSS (0x200080FC) and below the telemetry block (0x2000BF00). No initialized .data: patch state lives in
   .bss and is zeroed by patch_init() (called from the first hook that runs). */
INCLUDE stock_syms.ld
MEMORY { FLASH (rx) : ORIGIN = 0x0802A000, LENGTH = 0x30000
         RAM (rw)   : ORIGIN = 0x2000A000, LENGTH = 0x1F00 }
SECTIONS {
  .text : { KEEP(*(.text.hooks)) *(.text*) *(.rodata*) } > FLASH
  .data : { *(.data*) } > RAM AT > FLASH
  .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; } > RAM
  /DISCARD/ : { *(.ARM.exidx*) *(.comment) }
}
ASSERT(SIZEOF(.data) == 0, "C patches must not use initialized .data (no startup copies it)")
