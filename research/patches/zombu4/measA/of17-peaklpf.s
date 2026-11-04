@ PATCH peak-lpf-excl
@ OF-17: PEAK (short) and the 1 kHz filter (long REL) can both be on in ACV/ACmV; the HY3131 row is then the plain AC
@ row while INTE is the peak one, no interrupt is serviced and the display stays blank. Fix: each key is refused
@ (Beep_Double, as for any key not available) while the other function is on.
@ PEAK short press (0x080152B4): accept iff ((mode 2 and not AC+DC) or mode 4) and the filter 0x200080B9 is off.
@ 1 kHz long press (0x080163BA): refuse when AC+DC 0x200080BA (stock) or PEAK 0x200080B7 is on.
.equ PEAK_ACCEPT, 0x080152d2
.equ PEAK_REJECT, 0x0801532e
.equ LPF_REJECT,  0x080163ea


@ AT 0x080152B4 LEN 30
	bl peak_ok
	cmp r0, #0
	beq PEAK_REJECT
@ AT 0x080163BA LEN 10
	bl lpf_busy
	cmp r0, #0
	bne LPF_REJECT
@ APPEND 0x08029C78
.thumb_func
peak_ok:
	ldr r1, =0x2000803C        @ mode
	ldrb r0, [r1]
	cmp r0, #4
	beq 1f
	cmp r0, #2
	bne 2f
	ldr r1, =0x200080BA        @ AC+DC
	ldrb r0, [r1]
	cbnz r0, 2f
1:	ldr r1, =0x200080B9        @ 1 kHz filter
	ldrb r0, [r1]
	eor r0, r0, #1
	bx lr
2:	movs r0, #0
	bx lr
.thumb_func
lpf_busy:
	ldr r1, =0x200080BA        @ AC+DC | PEAK
	ldrb r0, [r1]
	ldr r1, =0x200080B7
	ldrb r1, [r1]
	orrs r0, r1
	bx lr
	.ltorg
