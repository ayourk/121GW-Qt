@ PATCH adc-init-zero
@ OF-39: Board_ADCInit (0x08013DD4) builds the ADC_InitTypeDef on the stack but never writes its word at [sp+8]
@ (ExternalTrigConvEdge); ADC_Init (0x080200B4) ORs that word unmasked into ADC_CR2 (0x080200DC..0x080200E8). The
@ slot holds a stale register from an earlier call chain (r6 as left at reset), so CR2 can get ALIGN, EXTEN or
@ reserved bits for the whole session. The six field stores become word stores of 0 over [sp+0]..[sp+0x13]
@ (Resolution, the two mode bytes and their padding, ExternalTrigConvEdge, ExternalTrigConv, DataAlign: all 0 as
@ before), NbrOfConversion = 1 at [sp+0x14] stays as it is.
@ AT 0x08013DDE LEN 24
        movs    r0, #0
        str     r0, [sp, #0]
        str     r0, [sp, #4]
        str     r0, [sp, #8]
        str     r0, [sp, #12]
        str     r0, [sp, #16]
