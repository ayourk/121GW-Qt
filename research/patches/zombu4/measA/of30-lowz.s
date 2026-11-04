@ PATCH lowz-blank
@ OF-30: LowZ_AutoAcDc forces g_reading = 0 for the first four above-threshold means, until the voltage is confirmed
@ (0x20008099), and LCD_DrawMain exempts mode 0 from its settle blank, so "0.0" V is drawn while a voltage is present.
@ Fix: LCD_DrawMain's blank test (0x08016FCE) calls lowz_blank: mode != 0 -> stock test (settle 0x20000238 or
@ 0x2000805C); mode 0 -> blank while a confirmation is in progress (not locked and count 0x2000023A below its reload 5).
@ The fifth mean (the one that confirms) was zeroed as well, so the value appeared one mean later still; LowZ_AutoAcDc's
@ unconfirmed branch (0x080065FA) now counts down first and zeroes only while not yet confirmed.
@ The quiet dead band (below 10 V, by design) still shows 0.0; a confirmed voltage shows as before.
.equ BLANK_BODY, 0x08016fec
.equ DRAW_VALUE, 0x08017006

.equ LOWZ_RET,   0x0800662a
@ AT 0x080065FA LEN 48
	ldr r1, =0x2000023A        @ confirmation countdown
	ldrb r0, [r1]
	subs r0, #1
	strb r0, [r1]
	bne 1f
	movs r0, #1
	ldr r1, =0x20008060        @ icons dirty
	strb r0, [r1]
	strb.w r0, [r1, #0x39]     @ 0x20008099 confirmed
	b LOWZ_RET
1:	movs r0, #0
	ldr r1, =0x20007D64        @ g_reading: not confirmed yet
	str r0, [r1]
	b LOWZ_RET
	.ltorg
@ AT 0x08016FCE LEN 30
	bl lowz_blank
	cmp r0, #0
	beq DRAW_VALUE
@ APPEND 0x08029C40
.thumb_func
lowz_blank:
	ldr r1, =0x2000803C        @ mode
	ldrb r1, [r1]
	cbnz r1, 2f
	ldr r1, =0x20008099        @ LowZ voltage confirmed
	ldrb r0, [r1]
	cbnz r0, 1f
	ldr r1, =0x2000023A        @ confirmation countdown, 5 when idle
	ldrb r0, [r1]
	subs r0, #5
	bx lr
1:	movs r0, #0
	bx lr
2:	ldr r1, =0x20000238        @ settle count
	ldrb r0, [r1]
	ldr r1, =0x2000805C
	ldrb r1, [r1]
	orrs r0, r1
	bx lr
	.ltorg
