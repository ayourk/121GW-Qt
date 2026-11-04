@ SPDX-License-Identifier: MIT
@ Bluetooth telemetry for stock 2.05 + the four fixes (docs/telemetry.md). Built by patches/build_v2.py:
@  - hy_diag (patches/hy_diag.s, SILENT): power-on HY3131 read + reserved-bit latch test at boot, no screens/delay;
@  - tele_poll: hooked in place of the main loop's `bl HY_Poll` (0x08024B6A); calls HY_Poll every pass, and while
@    Bluetooth is on sends one 19-byte frame per two TIM2 ticks (10 ms each) in ticks 2..34 of TIM2's 50-tick cycle,
@    the window in which the stock packet path is idle (stock: build at tick 40, send at tick 50). Frames go out the
@    same way TIM2 sends a stock packet (g_ble_tx, index 0x20008031, first byte by USART_SendByteBlocking, the rest by
@    the USART2 TXE interrupt). Stock packets are never modified.
@ Frame: D9 kind off_hi off_lo d0..d13 xor   (xor = XOR of bytes 0..17, as stock packets)
@   'E' data EEPROM 0x08080000 + off (12 KB)     'F' flash 0x08000000 + off (bootloader, 24 KB)
@   'O' 0x1FF80000 + off (option/ID/factory-cal page, 256 B)
@   'R' g_hy_mirror (HY3131 R00..R33 as last read) + off (0, 14, 28, 42)
@   'S' off = slot counter; mode range battery_dV(2, LE) ad1_average(4, LE) reading(4, LE) GPIOG.IDR(2, LE)
@   'T' off = slot counter; RTC copy(6) GPIOE.IDR>>8 pf8_strap variant_ohm6 variant_acv0 reading_fast(4, LE)
@   'A' off = 0; R34..R37 (after HY_SetMode) latch-mask nonzero-count power-on R20..R27
@   'B' off = 0; power-on R28..R35 (14 bytes)
@ Schedule per sent frame (slot n): n%8==0 'R' (rotating offset), n%8==4 'S'/'T' alternately, n%16==2 'A'/'B'
@ alternately, otherwise the next 14 bytes of the dump E -> F -> O, repeating (one pass about 2 minutes).
        .set    SILENT, 1
        .include "hy_diag.s"

        .equ TS,      0x2000bf80          @ telemetry state: +0 dump pointer, +4 slot, +8 last tick, +12 R index
        .equ TICK,    0x20007f04          @ TIM2 50-tick counter (word)
        .equ TXIDX,   0x20008031          @ index of the byte being sent (byte)
        .equ BLEON,   0x2000802b          @ g_ble_on (byte)
        .equ USART2,  0x40004400
        .equ MIRROR2, 0x200077d0
        .equ E_SIZE,  0x3000
        .equ F_SIZE,  0x6000
        .equ O_SIZE,  0x100
        .equ D_TOTAL, (E_SIZE + F_SIZE + O_SIZE)

        .section .text.tele, "ax"
        .global tele_poll
        .thumb_func
tele_poll:
        push    {r4-r7, lr}
        ldr     r0, =BLEON
        ldrb    r0, [r0]
        cmp     r0, #0
        beq     90f
        ldr     r0, =USART2
        ldr     r1, [r0, #0x0c]          @ CR1: TXEIE (bit 7) set = a frame is still going out
        tst     r1, #0x80
        bne     90f
        ldr     r1, =TICK
        ldr     r1, [r1]
        cmp     r1, #2
        blt     90f
        cmp     r1, #34
        bgt     90f
        ldr     r4, =TS
        ldr     r2, [r4, #8]
        cmp     r1, r2                   @ at most one frame per two ticks
        beq     90f
        adds    r2, #1
        cmp     r1, r2
        beq     90f
        str     r1, [r4, #8]
        ldr     r5, =BLETX
        movs    r0, #0xd9
        strb    r0, [r5, #0]
        ldr     r6, [r4, #4]             @ slot
        adds    r6, #1
        str     r6, [r4, #4]
        and     r0, r6, #7
        cmp     r0, #0
        beq     20f
        cmp     r0, #4
        beq     30f
        and     r0, r6, #15
        cmp     r0, #2
        beq     40f
        @ ---- dump: E, F, O
        ldr     r7, [r4, #0]             @ pointer into the concatenation
        ldr     r0, =D_TOTAL
        cmp     r7, r0
        blo     11f
        movs    r7, #0
11:     ldr     r0, =E_SIZE
        cmp     r7, r0
        bhs     12f
        movs    r1, #'E'
        ldr     r2, =0x08080000
        mov     r3, r7                   @ offset
        ldr     r0, =E_SIZE              @ region size
        b       15f
12:     ldr     r0, =(E_SIZE + F_SIZE)
        cmp     r7, r0
        bhs     13f
        movs    r1, #'F'
        ldr     r2, =0x08000000
        ldr     r0, =E_SIZE
        sub     r3, r7, r0
        ldr     r0, =F_SIZE
        b       15f
13:     movs    r1, #'O'
        ldr     r2, =0x1ff80000
        ldr     r0, =(E_SIZE + F_SIZE)
        sub     r3, r7, r0
        ldr     r0, =O_SIZE
15:     strb    r1, [r5, #1]             @ kind
        lsrs    r1, r3, #8
        strb    r1, [r5, #2]
        strb    r3, [r5, #3]
        add     r2, r2, r3               @ source address
        sub     r0, r0, r3               @ bytes left in this region
        cmp     r0, #14
        ble     16f
        movs    r0, #14
16:     movs    r1, #0                   @ copy r0 bytes, zero the rest of 14
17:     cmp     r1, #14
        bge     18f
        movs    r3, #0
        cmp     r1, r0
        bge     19f
        ldrb    r3, [r2, r1]
19:     adds    r6, r5, r1
        strb    r3, [r6, #4]
        adds    r1, #1
        b       17b
18:     adds    r7, r0                   @ advance by the bytes actually sent
        str     r7, [r4, #0]
        b       80f
        @ ---- 'R': HY3131 mirror, rotating 0/14/28/42
20:     movs    r1, #'R'
        strb    r1, [r5, #1]
        ldrb    r3, [r4, #12]
        adds    r0, r3, #1
        cmp     r0, #4
        blo     21f
        movs    r0, #0
21:     strb    r0, [r4, #12]
        movs    r0, #14
        muls    r3, r0                   @ offset
        movs    r0, #0
        strb    r0, [r5, #2]
        strb    r3, [r5, #3]
        ldr     r2, =MIRROR2
        add     r2, r2, r3
        movs    r0, #52
        sub     r0, r0, r3               @ bytes left in the 52-byte mirror
        cmp     r0, #14
        ble     22f
        movs    r0, #14
22:     movs    r1, #0                   @ copy (no dump-pointer advance)
23:     cmp     r1, #14
        bge     80f
        movs    r3, #0
        cmp     r1, r0
        bge     24f
        ldrb    r3, [r2, r1]
24:     adds    r7, r5, r1
        strb    r3, [r7, #4]
        adds    r1, #1
        b       23b
        @ ---- 'S' / 'T': sensors and state
30:     strb    r6, [r5, #3]             @ off = slot counter
        lsrs    r0, r6, #8
        strb    r0, [r5, #2]
        tst     r6, #8
        bne     35f
        movs    r0, #'S'
        strb    r0, [r5, #1]
        ldr     r2, =0x2000803c          @ g_mode, g_range
        ldrb    r0, [r2, #0]
        strb    r0, [r5, #4]
        ldrb    r0, [r2, #1]
        strb    r0, [r5, #5]
        ldr     r2, =0x20007fba          @ g_battery_dV
        ldrb    r0, [r2, #0]
        strb    r0, [r5, #6]
        ldrb    r0, [r2, #1]
        strb    r0, [r5, #7]
        ldr     r2, =0x20007dd4          @ g_ad1_average
        movs    r1, #8
        bl      put4
        ldr     r2, =0x20007d64          @ g_reading
        movs    r1, #12
        bl      put4
        ldr     r2, =0x40021c10          @ GPIOG.IDR (dial + keys)
        ldr     r0, [r2]
        strb    r0, [r5, #16]
        lsrs    r0, r0, #8
        strb    r0, [r5, #17]
        b       80f
35:     movs    r0, #'T'
        strb    r0, [r5, #1]
        ldr     r2, =0x200080d7          @ RTC copy: year month day hour minute second
        movs    r1, #0
36:     ldrb    r0, [r2, r1]
        adds    r3, r5, r1
        strb    r0, [r3, #4]
        adds    r1, #1
        cmp     r1, #6
        blt     36b
        ldr     r2, =0x40021010          @ GPIOE.IDR high byte (PE10 mA jack, PE11 A jack)
        ldr     r0, [r2]
        lsrs    r0, r0, #8
        strb    r0, [r5, #10]
        ldr     r2, =0x20007fdf          @ g_pf8_strap_low, g_variant_ohm6
        ldrb    r0, [r2, #0]
        strb    r0, [r5, #11]
        ldrb    r0, [r2, #1]
        strb    r0, [r5, #12]
        ldr     r2, =0x20008087          @ g_variant_acv0
        ldrb    r0, [r2]
        strb    r0, [r5, #13]
        ldr     r2, =0x20007d68          @ g_reading_fast
        movs    r1, #14
        bl      put4
        b       80f
        @ ---- 'A' / 'B': the boot diagnostic
40:     movs    r0, #0
        strb    r0, [r5, #2]
        strb    r0, [r5, #3]
        ldr     r2, =RES
        tst     r6, #16
        bne     45f
        movs    r0, #'A'
        strb    r0, [r5, #1]
        ldr     r0, [r2, #28]            @ R34..R37 after HY_SetMode
        strb    r0, [r5, #4]
        lsrs    r0, r0, #8
        strb    r0, [r5, #5]
        lsrs    r0, r0, #8
        strb    r0, [r5, #6]
        lsrs    r0, r0, #8
        strb    r0, [r5, #7]
        ldrb    r0, [r2, #32]            @ mask
        strb    r0, [r5, #8]
        ldrb    r0, [r2, #33]            @ nonzero count
        strb    r0, [r5, #9]
        movs    r1, #0                   @ power-on R20..R27 -> bytes 10..17
41:     adds    r3, r2, r1
        ldrb    r0, [r3, #4]
        adds    r3, r5, r1
        strb    r0, [r3, #10]
        adds    r1, #1
        cmp     r1, #8
        blt     41b
        b       80f
45:     movs    r0, #'B'
        strb    r0, [r5, #1]
        movs    r1, #0                   @ power-on R28..R35 + R36 R37 -> bytes 4..17
46:     adds    r3, r2, r1
        ldrb    r0, [r3, #12]
        adds    r3, r5, r1
        strb    r0, [r3, #4]
        adds    r1, #1
        cmp     r1, #14
        blt     46b
        @ ---- checksum and send (as TIM2_IRQHandler starts a stock packet)
80:     ldrb    r1, [r5, #0]
        movs    r3, #1
81:     ldrb    r0, [r5, r3]
        eors    r1, r0
        adds    r3, #1
        cmp     r3, #18
        blt     81b
        strb    r1, [r5, #18]
        ldr     r0, =TXIDX
        movs    r1, #0
        strb    r1, [r0]
        ldr     r0, =USART2
        ldrb    r1, [r5, #0]
        bl      USART_SendByteBlocking
        ldr     r0, =USART2
        movw    r1, #0x727
        movs    r2, #1
        bl      USART_ITConfig
90:     bl      HY_Poll
        pop     {r4-r7, pc}

@ put4(r1 = frame offset, r2 = source): copy 4 bytes from r2 to g_ble_tx[r1..r1+3] (little-endian as in RAM)
        .thumb_func
put4:
        ldr     r3, =BLETX
        add     r3, r3, r1
        ldrb    r0, [r2, #0]
        strb    r0, [r3, #0]
        ldrb    r0, [r2, #1]
        strb    r0, [r3, #1]
        ldrb    r0, [r2, #2]
        strb    r0, [r3, #2]
        ldrb    r0, [r2, #3]
        strb    r0, [r3, #3]
        bx      lr
        .ltorg
