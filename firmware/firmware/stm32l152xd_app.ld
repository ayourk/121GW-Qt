/*
 * Linker script for the 121GW application (STM32L15xxD), entered from the
 * SD bootloader at 0x08006000.
 *
 * Every global variable keeps its address in the original v2.05 image (they
 * are bound by tools/hybrid_syms.py --ram-only), so the image defines no RAM
 * data of its own; the start-up code clears and initialises RAM as the
 * original does. The only RAM objects defined here are FatFs's two static
 * variables, placed at the original addresses.
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08006000, LENGTH = 384K - 24K
}

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > FLASH

    .fatfs_volumes 0x20007C58 (NOLOAD) : { *(.bss.FatFs) }   /* FATFS *FatFs[1] */
    .fatfs_mount_id 0x20007F98 (NOLOAD) : { *(.bss.Fsid) }   /* WORD Fsid */

    /* Custom: Bluetooth transfer state (src/bt_xfer.c), the SD layer's state (.sd_ram: diskio.c, the FatFs
       R0.16 objects of sd_card.c and sd_log.c) and FatFs R0.16's buffers,
       above the original's variables (which end at 0x200080FB); the start-up code clears it. 48 KB RAM ends
       at 0x2000C000. */
    .bt_ram 0x20008100 (NOLOAD) :
    {
        __bt_ram_start = .;
        *(.bt_ram)
        *(.sd_ram)
        *(.bss.DirBuf)          /* FatFs R0.16: exFAT directory entry block buffer */
        *(.bss.LfnBuf)          /* FatFs R0.16: LFN working buffer */
        . = ALIGN(4);
        __bt_ram_end = .;
    }
    ASSERT(__bt_ram_end <= 0x2000C000, "bt_xfer RAM does not fit below the end of RAM")

    .data : { *(.data*) } > FLASH
    .bss  : { *(.bss*) *(COMMON) } > FLASH

    ASSERT(SIZEOF(.data) == 0, "the application must not define initialised RAM data")
    ASSERT(SIZEOF(.bss) == 0, "the application must not define RAM variables")
}
