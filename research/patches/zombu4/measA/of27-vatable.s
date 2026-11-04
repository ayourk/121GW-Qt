@ PATCH va-table-col7
@ OF-27: VA_SelectCorrTable reads mode 15 ranges 2/3 (strap flag 0x20007FDF = 0) from 7-word rows (0x080283C4,
@ 0x080284C0), but the voltage bin reaches 7 for V >= 50001: the "column 7" words come from the next row, past the
@ table for the last row (0x080285BC, HY3131 configuration data; 52.000 V in range 3 shows OFL). Fix: a wrapper at
@ the entry; for exactly that case it runs the stock fetch and then replaces the two column-7 words with the
@ column-6 words of the same rows (EBC := EB8, EC4 := EC0): above 50000 counts the correction stays at its 50000
@ value (flat), the bilinear weight then has nothing to interpolate. Every other mode, range or bin is unchanged.
.equ SEL_BODY, 0x0800936b
@ AT 0x08009364 LEN 6
	b.w va_sel
@ APPEND 0x08029D64
.thumb_func
va_sel:
	cmp r1, #7
	bne 1f
	ldr r3, =0x2000803C        @ mode
	ldrb r3, [r3]
	cmp r3, #15
	bne 1f
	ldr r3, =0x2000803D        @ range
	ldrb r3, [r3]
	subs r3, #2
	cmp r3, #1
	bhi 1f
	ldr r3, =0x20007FDF        @ 0: the 7-word tables
	ldrb r3, [r3]
	cbnz r3, 1f
	push {r3, lr}
	bl 1f
	ldr r0, =0x20007EB8        @ row-1: col-1, col; row: col-1, col
	ldr r1, [r0]
	str r1, [r0, #4]
	ldr r1, [r0, #8]
	str r1, [r0, #12]
	pop {r3, pc}
1:	push {r4}                  @ the stock entry (2 instructions displaced by the branch)
	ldr r3, =0x2000803C
	b.w SEL_BODY
	.ltorg
