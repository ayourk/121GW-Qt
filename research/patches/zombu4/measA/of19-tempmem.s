@ PATCH temp-mem-stale
@ OF-19: Mode_Temp_OnAD1 converts (g_reading + cold junction) to a temperature whenever the new-value flag 0x2000805D
@ is set and writes the result back into g_reading. Only the display routine clears 0x2000805D, and it is skipped in
@ MEM view, so there every sample feeds the previous temperature back in as microvolts (24.2 -> 35.7 degC). Fix:
@ convert only on the sample that produced a new mean (0x20008081 set when Mode_DCmV_OnAD1 is entered, the test it
@ uses itself). The call at 0x08007842 goes through temp_pre, which leaves that flag in r4 (free in the caller until
@ 0x08007868); the conversion at 0x0800785A is moved to temp_conv and skipped when r4 is 0. Rounding (0x0800788A on)
@ is unchanged.
.equ DCmV_OnAD1,    0x080077f1
.equ TC_EmfToTemp,  0x08013c41
.equ Temp_CToF,     0x08013b77
.equ TEMP_ROUND,    0x0800788a
.equ TEMP_RET,      0x080078b6
@ AT 0x08007842 LEN 4
	bl temp_pre
@ AT 0x0800785A LEN 4
	b.w temp_conv
@ APPEND 0x08029CC8
.thumb_func
temp_pre:
	push {r0, lr}
	ldr r1, =0x20008081        @ a new 16-sample mean is due in this call
	ldrb r4, [r1]
	bl DCmV_OnAD1
	pop {r0, pc}
.thumb_func
temp_conv:
	cbz r4, 2f
	ldr r0, =0x20007D64        @ g_reading (uV mean)
	ldr r0, [r0]
	ldr r1, =0x20007E7C        @ cold-junction EMF
	ldr r1, [r1]
	adds r0, r1, r0
	bl TC_EmfToTemp
	ldr r0, =0x20007E80
	ldr r4, [r0]
	ldr r0, =0x200080AB        @ temperature unit (0 = degF)
	ldrb r0, [r0]
	cbnz r0, 1f
	mov r0, r4
	bl Temp_CToF
	mov r4, r0
1:	b.w TEMP_ROUND
2:	b.w TEMP_RET
	.ltorg
