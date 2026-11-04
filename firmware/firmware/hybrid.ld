/*
 * Hybrid test image: rebuilt modules linked against the original v2.05 image.
 * Code lives at 0x08100000 (outside real flash; emulator only). All globals
 * resolve to their original RAM addresses, so the image must not define any
 * RAM data of its own. The only exception is FatFs (common/fatfs-r016/ff.c,
 * unmodified), whose two static variables are placed at the original
 * image's addresses.
 */
MEMORY
{
    TEST (rx) : ORIGIN = 0x08100000, LENGTH = 256K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.text*))
        *(.rodata*)
    } > TEST

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

    .data : { *(.data*) } > TEST
    .bss  : { *(.bss*) *(COMMON) } > TEST

    ASSERT(SIZEOF(.data) == 0, "hybrid image must not define initialised RAM data")
    ASSERT(SIZEOF(.bss) == 0, "hybrid image must not define RAM variables")
}
