@ PATCH lpf-comp
@ OF-26: with the 1 kHz filter on, Cal_DivideGain yields ten times the count and ACFreq_LpfCompensate scales it back
@ (reading * k / 10000, k from the frequency table 20..6000 Hz), but returns the x10 value unchanged for f < 20 Hz,
@ which includes f = 0 before the first counter result. Fix: clamp f to 20 Hz (the first table point) and go on
@ through the table, so the 1/10 scale and the 20 Hz filter correction always apply. r1 still holds &f (0x20007C5C)
@ from the store at 0x08006D7E.
@ Also (docs/v204-vs-v205.md): reading * k is a 32-bit product that wraps for large over-range values (sign flip or
@ a plausible small value). Fix: 64-bit product, one signed 64-bit divide by 10000 (same truncation as the stock
@ /1000 then /10).
.equ ldivmod, 0x08013fe5
.equ LPF_RET, 0x08007094
@ AT 0x08006D86 LEN 8
	cmp r0, #20
	bge.n 1f
	movs r0, #20
	str r0, [r1]
1:
@ AT 0x0800707E LEN 22
	smull r0, r1, r0, r4       @ k * reading
	movw r2, #10000
	movs r3, #0
	bl ldivmod
	movs r6, r0
	b LPF_RET
