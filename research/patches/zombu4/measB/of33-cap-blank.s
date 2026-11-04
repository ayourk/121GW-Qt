@ PATCH cap-jump-blank
@ OF-33: Mode_Cap_Poll (0x0800B540) jumps the capacitance range on a counter timeout (range < 2 -> 2, < 5 -> 5,
@ 0x0800B59E..0x0800B5C0) and requests the re-apply (0x2000804B = 1, 0x0800B5D2..0x0800B5D8). Until Mode_ApplyChange
@ runs, the glass keeps the old digits under the new range's decimal point ("10.000" nF becomes "1.0000" nF for
@ about 18 ms). Autorange's own range step avoids this through the blank flag 0x2000805C, but that only acts inside
@ Display_Update, which does not run in this pass. So the jump blanks the main digits itself, as Display_Update does
@ with the blank flag (LCD_ShowBlank(0), skipped with auto-hold 0x20008050 on, as there).
@ The in-place store of the re-apply request becomes a call that stores it and blanks.
        .equ    LCD_ShowBlank, 0x08019A98
@ AT 0x0800B5D2 LEN 8
        bl      cap_jump_blank
@ APPEND 0x08029E00
cap_jump_blank:
        push    {r4, lr}
        movs    r0, #1
        ldr     r1, =0x2000804B         @ range changed: Mode_ApplyChange re-applies
        strb    r0, [r1]
        ldr     r1, =0x20008050         @ auto-hold
        ldrb    r0, [r1]
        cbnz    r0, 1f
        bl      LCD_ShowBlank           @ (0): main line
1:      pop     {r4, pc}
        .ltorg
