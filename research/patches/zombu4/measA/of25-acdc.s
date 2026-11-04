@ PATCH acdc-sum64
@ OF-25: ACDC_Combine forms |AC|^2 + |DC|^2 in 32 bits; from 65536 counts combined the sum wraps (47000 + 47000
@ shows 11092). Each square is below 2^32 (both inputs < 60001 on this path), so convert each to double and add
@ there. The squares are still stored at 0x20007EA4 / 0x20007EA8. r7 is not touched (the epilogue pops r0, pc).
.equ ui2d,     0x08013ed9
.equ dadd,     0x080131b9
.equ dsqrt,    0x08013ef5
.equ d2iz,     0x080132a1
.equ EPI,      0x0800a74a
@ AT 0x0800A702 LEN 72
	ldr r2, =0x20007E9C        @ |AC|, then |DC|, AC^2, DC^2
	ldr r0, [r2]
	muls r0, r0
	str r0, [r2, #8]
	ldr r0, [r2, #4]
	muls r0, r0
	str r0, [r2, #12]
	bl ui2d                    @ (double) DC^2
	push {r0, r1}
	ldr r0, =0x20007EA4
	ldr r0, [r0]
	bl ui2d                    @ (double) AC^2
	pop {r2, r3}
	bl dadd
	bl dsqrt
	bl d2iz
	ldr r1, =0x20007D64        @ g_reading
	str r0, [r1]
	b EPI
	.ltorg
