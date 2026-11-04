@ SPDX-License-Identifier: MIT
@ get_fattime from the clock chip, for stock 2.05 (docs/sd-card.md). Stock get_fattime (0x0802402E) returns the
@ FatFs sample constant 0x3449B3C0 (2006-02-09 22:30:00), so every file is created in 2006. This returns the FAT
@ date/time from the clock-chip copy 0x200080D7.. (year since 2000, month, day, hour, minute, second, binary; main
@ reloads it from the NJU6350 every second), packed as File_SetTimestamp packs it:
@   (year + 20) << 25 | month << 21 | day << 16 | hour << 11 | minute << 5 | second / 2
@ Linked at 0x08029700; the stock entry becomes `b.w get_fattime_rtc`.
        .syntax unified
        .cpu cortex-m3
        .thumb
        .section .text.fattime, "ax"
        .global get_fattime_rtc
        .thumb_func
get_fattime_rtc:
        ldr     r3, =0x200080d7
        ldrb    r0, [r3, #0]
        adds    r0, #20
        lsls    r0, r0, #25
        ldrb    r1, [r3, #1]
        orr     r0, r0, r1, lsl #21
        ldrb    r1, [r3, #2]
        orr     r0, r0, r1, lsl #16
        ldrb    r1, [r3, #3]
        orr     r0, r0, r1, lsl #11
        ldrb    r1, [r3, #4]
        orr     r0, r0, r1, lsl #5
        ldrb    r1, [r3, #5]
        orr     r0, r0, r1, lsr #1
        bx      lr
        .ltorg
