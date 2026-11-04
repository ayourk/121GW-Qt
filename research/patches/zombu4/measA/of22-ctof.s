@ PATCH ctof-signed
@ OF-22: Temp_CToF takes the sign from bit 15 of a 32-bit value and negates with ones' complement on input and output:
@ 327.68..655.35 degC convert as negatives, negative inputs as |x| - 1 (-40.00 degC -> -39.99 degF). Fix: the entry
@ is replaced in place by an integer version: F = round((9 x + 16000) / 5) in 0.01 degF on the signed 32-bit x (the
@ quotient is never a tie; nearest by +-2 before the truncating divide), stored to 0x20007E88 as stock does.
@ Callers use the result as a plain signed int (Mode_Temp_OnAD1, Temp_ToDisplayTenths).
@ AT 0x08013B76 LEN 202
	add.w r1, r0, r0, lsl #3   @ 9x
	add.w r1, r1, #16000       @ 5 * (0.01 degF)
	cmp r1, #0
	ite ge
	addge r1, r1, #2
	sublt r1, r1, #2
	movs r2, #5
	sdiv r0, r1, r2
	ldr r1, =0x20007E88
	str r0, [r1]
	bx lr
	.ltorg
