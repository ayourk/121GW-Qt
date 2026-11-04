@ SPDX-License-Identifier: MIT
@ HY3131 diagnostic for stock 2.05 (docs/hy3131-switches.md section 6). Hooked in place of BootScreen's
@ HY_SetMode(5, 0) call (0x0800EA8A); runs once per power-on:
@  1. reads R20..R37 (24 bytes) before 2.05's first configuration write: the chip's power-on state (the SD bootloader
@     only sets the bus pins idle; 2.05 never writes R34..R37);
@  2. calls the original HY_SetMode(mode, range);
@  3. latch test of the six reserved bits in R20..R33: read the register, write it with the bit set, read back,
@     restore the original value; bit i of the mask = reserved bit i stuck (a real latch);
@  4. stores everything at 0x2000BF00 (for SWD) and shows it, ~3 s per screen, keeping the boot screen:
@     screen 1  main "H" + R34 R35 hex   sub "H" + R36 R37 hex     (power-on values)
@     screen 2  main "L" + mask hex + "  "  sub "n" + count of nonzero power-on R20..R33 bytes (hex) + "  "
@ Result block 0x2000BF00: +0 'HYDG', +4 R20..R37 at power-on (24 B), +28 R34..R37 after HY_SetMode (4 B),
@ +32 latch mask, +33 nonzero count, +40 scratch, +44 BLE packet counter, +48/+56 text, +64 saved LCD shadow (32 B).
@ Also reported over Bluetooth (ble_diag below) once BLE is switched on.
@ R37 is never written (test mode; 0x60 = reset). Nothing else in 2.05 changes.
        .syntax unified
        .cpu cortex-m3
        .thumb
        .equ MIRROR, 0x200077d0          @ g_hy_mirror (HY_Read destination)
        .equ SHADOW, 0x20007790          @ LCD shadow (8 words)
        .equ RES,    0x2000bf00          @ result block (free RAM above BSS)
        .equ DELAY,  0x00600000          @ ~3 s in emulation (2 instructions per turn at 4194 per ms)

        .section .text.hy_diag, "ax"
        .global hy_diag
        .thumb_func
hy_diag:
        push    {r4-r7, lr}
        mov     r6, r0                   @ mode
        mov     r7, r1                   @ range
        ldr     r4, =RES
        ldr     r0, =0x47445948          @ "HYDG"
        str     r0, [r4]
        @ 1. power-on R20..R37 -> RES+4
        ldr     r0, =MIRROR
        movs    r1, #0x20
        movs    r2, #24
        bl      HY_Read
        ldr     r5, =MIRROR
        movs    r3, #0
        movs    r2, #0                   @ nonzero count over R20..R33 (first 20)
1:      ldrb    r0, [r5, r3]
        adds    r1, r3, #4
        strb    r0, [r4, r1]
        cmp     r3, #20
        bge     2f
        cmp     r0, #0
        beq     2f
        adds    r2, #1
2:      adds    r3, #1
        cmp     r3, #24
        blt     1b
        strb    r2, [r4, #33]
        @ 2. the original call
        mov     r0, r6
        mov     r1, r7
        bl      HY_SetMode
        @ R34..R37 after it -> RES+28
        ldr     r0, =MIRROR
        movs    r1, #0x34
        movs    r2, #4
        bl      HY_Read
        ldr     r0, [r5]
        str     r0, [r4, #28]
        @ 3. latch test
        movs    r6, #0                   @ mask
        movs    r7, #0                   @ index
3:      adr     r0, rsv_table
        lsls    r1, r7, #1
        adds    r0, r0, r1
        ldrb    r1, [r0, #0]             @ register address
        ldrb    r0, [r0, #1]             @ bit
        strb    r1, [r4, #41]            @ keep addr / bit across calls
        strb    r0, [r4, #42]
        ldr     r0, =MIRROR              @ read the register
        movs    r2, #1
        bl      HY_Read
        ldrb    r0, [r5]
        strb    r0, [r4, #43]            @ original value
        ldrb    r1, [r4, #42]
        orrs    r0, r1
        strb    r0, [r4, #40]
        add     r0, r4, #40              @ write value | bit
        ldrb    r1, [r4, #41]
        movs    r2, #1
        bl      HY_Write
        ldr     r0, =MIRROR              @ read back
        ldrb    r1, [r4, #41]
        movs    r2, #1
        bl      HY_Read
        ldrb    r0, [r5]
        ldrb    r1, [r4, #42]
        tst     r0, r1
        beq     4f
        movs    r0, #1
        lsls    r0, r0, r7
        orrs    r6, r0
4:      ldrb    r0, [r4, #43]            @ restore
        strb    r0, [r4, #40]
        add     r0, r4, #40
        ldrb    r1, [r4, #41]
        movs    r2, #1
        bl      HY_Write
        adds    r7, #1
        cmp     r7, #6
        blt     3b
        strb    r6, [r4, #32]
        .ifndef SILENT                   @ SILENT (telemetry build): no screens, no delay, boot keys unaffected
        @ 4. show: save the boot screen's shadow
        ldr     r0, =SHADOW
        add     r1, r4, #64
        bl      copy32
        @ screen 1
        add     r0, r4, #48
        movs    r1, #'H'
        strb    r1, [r0]
        ldrb    r1, [r4, #(4 + 0x14)]    @ power-on R34
        adds    r0, #1
        bl      hex2
        ldrb    r1, [r4, #(4 + 0x15)]    @ R35
        bl      hex2
        movs    r1, #0
        strb    r1, [r0]
        add     r0, r4, #56
        movs    r1, #'H'
        strb    r1, [r0]
        ldrb    r1, [r4, #(4 + 0x16)]    @ R36
        adds    r0, #1
        bl      hex2
        ldrb    r1, [r4, #(4 + 0x17)]    @ R37
        bl      hex2
        movs    r1, #0
        strb    r1, [r0]
        bl      show
        @ screen 2
        add     r0, r4, #48
        movs    r1, #'L'
        strb    r1, [r0]
        ldrb    r1, [r4, #32]
        adds    r0, #1
        bl      hex2
        movs    r1, #' '
        strb    r1, [r0]
        strb    r1, [r0, #1]
        movs    r1, #0
        strb    r1, [r0, #2]
        add     r0, r4, #56
        movs    r1, #'n'
        strb    r1, [r0]
        ldrb    r1, [r4, #33]
        adds    r0, #1
        bl      hex2
        movs    r1, #' '
        strb    r1, [r0]
        strb    r1, [r0, #1]
        movs    r1, #0
        strb    r1, [r0, #2]
        bl      show
        @ restore the boot screen
        add     r0, r4, #64
        ldr     r1, =SHADOW
        bl      copy32
        bl      LCD_Flush
        .endif
        pop     {r4-r7, pc}

@ show(): main text RES+48, sub text RES+56, flush, wait
        .thumb_func
show:
        push    {r4, lr}
        ldr     r4, =RES
        movs    r0, #0
        add     r1, r4, #48
        bl      LCD_DrawText5
        movs    r0, #1
        add     r1, r4, #56
        bl      LCD_DrawText5
        bl      LCD_Flush
        ldr     r0, =DELAY
5:      subs    r0, #1
        bne     5b
        pop     {r4, pc}

@ hex2(r0 = dest, r1 = byte): two uppercase hex digits at r0, returns r0 + 2
        .thumb_func
hex2:
        lsrs    r2, r1, #4
        adds    r2, #'0'
        cmp     r2, #'9'
        ble     6f
        adds    r2, #7
6:      strb    r2, [r0]
        movs    r2, #15
        ands    r2, r1
        adds    r2, #'0'
        cmp     r2, #'9'
        ble     7f
        adds    r2, #7
7:      strb    r2, [r0, #1]
        adds    r0, #2
        bx      lr

@ copy32(r0 = src, r1 = dst): 8 words
        .thumb_func
copy32:
        movs    r2, #8
8:      ldr     r3, [r0], #4
        str     r3, [r1], #4
        subs    r2, #1
        bne     8b
        bx      lr

@ ble_diag(): replaces the main loop's call to BLE_BuildPacket (in BLE_BuildPacket_Thunk, 0x0800C5D4). Builds the
@ normal packet, then on every 8th one (once the boot diagnostic has run) overwrites g_ble_tx with a diagnostic
@ frame instead, alternating A and B; byte 18 = XOR of bytes 0..17, as in stock packets. Header 0xD9 (stock: 0xF2).
@   A: D9 'A' R34 R35 R36 R37 (after HY_SetMode) mask nonzero R20..R29 (power-on)
@   B: D9 'B' R2A..R37 (power-on, 14 bytes) 00 00
        .equ BLETX, 0x20007ae0
        .thumb_func
        .global ble_diag
ble_diag:
        push    {r4, lr}
        bl      BLE_BuildPacket
        ldr     r4, =RES
        ldr     r0, [r4]
        ldr     r1, =0x47445948
        cmp     r0, r1
        bne     9f
        ldrb    r0, [r4, #44]            @ packet counter
        adds    r0, #1
        strb    r0, [r4, #44]
        tst     r0, #7
        bne     9f
        ldr     r2, =BLETX
        movs    r1, #0xd9
        strb    r1, [r2, #0]
        tst     r0, #8
        bne     10f
        movs    r1, #'A'
        strb    r1, [r2, #1]
        ldr     r1, [r4, #28]            @ R34..R37 after HY_SetMode
        str     r1, [r2, #2]             @ g_ble_tx is word-aligned + 2: unaligned store is fine on M3 RAM
        ldrb    r1, [r4, #32]
        strb    r1, [r2, #6]
        ldrb    r1, [r4, #33]
        strb    r1, [r2, #7]
        movs    r3, #0                   @ power-on R20..R29 -> bytes 8..17
11:     adds    r0, r4, r3
        ldrb    r1, [r0, #4]
        adds    r0, r2, r3
        strb    r1, [r0, #8]
        adds    r3, #1
        cmp     r3, #10
        blt     11b
        b       12f
10:     movs    r1, #'B'
        strb    r1, [r2, #1]
        movs    r3, #0                   @ power-on R2A..R37 -> bytes 2..15
13:     adds    r0, r4, r3
        ldrb    r1, [r0, #14]
        adds    r0, r2, r3
        strb    r1, [r0, #2]
        adds    r3, #1
        cmp     r3, #14
        blt     13b
        movs    r1, #0
        strb    r1, [r2, #16]
        strb    r1, [r2, #17]
12:     ldrb    r1, [r2, #0]             @ XOR of bytes 0..17 -> byte 18
        movs    r3, #1
14:     ldrb    r0, [r2, r3]
        eors    r1, r0
        adds    r3, #1
        cmp     r3, #18
        blt     14b
        strb    r1, [r2, #18]
9:      pop     {r4, pc}

        .align  2
rsv_table:                               @ (register, bit) for the six reserved bits of R20..R33
        .byte   0x20, 0x01,  0x23, 0x20,  0x23, 0x40,  0x26, 0x40,  0x28, 0x08,  0x28, 0x80
        .ltorg
