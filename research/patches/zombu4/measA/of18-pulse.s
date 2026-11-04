@ PATCH pulse-width
@ OF-18: Counter_PulseWidth sets W = P * duty / 1000 with duty = CTC / (T / 1000) truncated to 0.1 %, so W moves in
@ steps of P / 1000 and is always rounded down (1 kHz 25 % shows 0.2499 ms; 10 Hz 25.03 % shows 25.000 ms). Fix:
@ W from the measured high-time count directly, W = CTC * 10^8 / (ref * CTB), rounded to nearest, in the same
@ 10 ns units as the period P = T * 10^8 / (ref * CTB) (0x20007C48, 64-bit). The duty value 0x20007E34 is still
@ computed (Counter_ComputeDuty runs first, as before). Zero denominator stores 0.
.equ uldivmod, 0x08014029
.equ PW_NEXT,  0x0800b0ae
@ AT 0x0800B080 LEN 46
	bl pulse_w
	b.w PW_NEXT
@ APPEND 0x08029D14
.thumb_func
pulse_w:
	push {r4, r5, r6, lr}
	ldr r0, =0x20007E44        @ CTC: high-time reference counts
	ldr r0, [r0]
	ldr r2, =100000000
	umull r0, r1, r0, r2
	ldr r2, =0x20007E18        @ reference clock (Hz)
	ldr r2, [r2]
	ldr r3, =0x20007E40        @ CTB: signal periods counted
	ldr r3, [r3]
	umull r2, r3, r2, r3
	orrs.w r4, r2, r3
	beq 1f
	lsrs r4, r2, #1            @ + den / 2: round to nearest
	orr.w r4, r4, r3, lsl #31
	lsrs r5, r3, #1
	adds r0, r0, r4
	adcs r1, r1, r5
	bl uldivmod
	b 2f
1:	movs r0, #0
	movs r1, #0
2:	ldr r2, =0x20007C48
	strd r0, r1, [r2]
	pop {r4, r5, r6, pc}
	.ltorg
