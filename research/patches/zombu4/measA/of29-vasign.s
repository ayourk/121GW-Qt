@ PATCH va-dc-sign
@ OF-29: VA_VoltageCorrect in DC VA (mode 24) depends on the polarity. Range 2: the correction d is computed from |V|
@ and |I|, but with strap flag 0x20007FDF = 0 it is added to the signed V (+20000/+3000 -> +19931, -20000/-3000 ->
@ -20069); only with 0x20007FDF != 0 is it applied to the magnitude. Ranges 0, 1, 3 return early on any negative V
@ or I (+20000 -> +19976, -20000 -> -20000).
@ Fix (a): range 2 always takes the magnitude path (the 0x20007FDF test at 0x08009280 becomes nops): V < 0 -> V - d.
@ Fix (b): ranges 0, 1, 3 with V and I both negative (the same measurement with reversed polarity) are corrected as
@ the positive pair and the sign restored: a wrapper at the entry negates g_va_volts and g_va_amps, runs the stock
@ function, and negates both back. Mixed signs in ranges 0, 1, 3 stay uncorrected as before (no table for them; in
@ range 2 the stock mixed-sign table exists only for 0x20007FDF != 0).
.equ VC_BODY, 0x08008ed5
@ AT 0x08009280 LEN 8
	nop
@ AT 0x08008ED0 LEN 4
	b.w va_corr
@ APPEND 0x08029DAC
.thumb_func
va_corr:
	ldr r2, =0x2000803C        @ mode, range
	ldrb r0, [r2]
	cmp r0, #24
	bne 1f
	ldrb r0, [r2, #1]
	cmp r0, #2
	beq 1f
	ldr r2, =0x20007D7C        @ g_va_volts, g_va_amps
	ldr r0, [r2]
	ldr r1, [r2, #4]
	ands.w r3, r0, r1
	bpl 1f                     @ not both negative
	negs r0, r0
	negs r1, r1
	strd r0, r1, [r2]
	push {r4, lr}
	mov r4, r2
	bl 1f
	ldrd r0, r1, [r4]
	negs r0, r0
	negs r1, r1
	strd r0, r1, [r4]
	pop {r4, pc}
1:	stmdb sp!, {r4, r5, r6, r7, r8, r9, sl, lr}
	b.w VC_BODY
	.ltorg
