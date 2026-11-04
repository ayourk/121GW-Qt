@ PATCH ol-hyst-clear
@ OF-20: Autorange's OL limit is FS + FS/12 while the hysteresis flag 0x200080F8 is set, FS + FS/10 otherwise; the
@ flag is only cleared during a settle count or in cal mode and only set on the range-switching path. BEEP (settle
@ count 0, one range) therefore keeps the flag of the previous mode: OL from 541.67 instead of 550.00 ohm. Fix: the
@ mode-change-only part of Mode_ApplyChange (0x0800D37E, 0x2000804A set) also clears 0x200080F8. Range changes are
@ left alone (the range-switching path sets the flag on purpose just before them).
@ AT 0x0800D37E LEN 8
	bl clear_hyst
@ APPEND 0x08029CB4
.thumb_func
clear_hyst:
	movs r0, #0
	ldr r1, =0x200080A2        @ the stock store this call replaces
	strb r0, [r1]
	ldr r1, =0x200080F8        @ OL hysteresis
	strb r0, [r1]
	bx lr
	.ltorg
