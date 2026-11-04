@ PATCH adc-smpr-off
@ OF-13: ADC_ReadOnce (0x080137B4) switches the ADC on (ADC_Cmd 0x080137C8), waits for ADONS, and only then selects
@ the channel with ADC_RegularChannelConfig (0x080137EA / 0x080137FA), which writes the sample time into ADC_SMPR3
@ (0x080201F0) while ADON = 1; RM0038 12.15.6 allows SMPRx writes only with ADON = 0. The ADC is still on at entry
@ (Board_ADCInit leaves it on, and the read does not switch it off). New order: ADC off, channel and sample time
@ (channel 5 battery for arg 1, channel 4 thermistor otherwise, 384 cycles), ADC on, wait for ADONS; the
@ conversion start at 0x080137FE follows unchanged.
        .equ    ADC_Cmd, 0x08020104
        .equ    ADC_RegularChannelConfig, 0x0802012C
        .equ    ADC_GetFlagStatus, 0x080202FA
        .equ    ADC1_LIT, 0x08013E50          @ literal word 0x40012400 of this function
        .equ    BLK, 0x080137C2
        .macro  ldr_adc1                      @ ldr.w r0, [pc, #imm] to the literal above
        ldr.w   r0, [pc, #(ADC1_LIT - ((BLK + (. - blk) + 4) & ~3))]
        .endm
@ AT 0x080137C2 LEN 60
blk:
        movs    r1, #0
        ldr_adc1
        bl      ADC_Cmd                       @ ADON = 0
        uxtb    r4, r4
        cmp     r4, #1
        ite     eq
        moveq   r1, #5                        @ PA5 battery
        movne   r1, #4                        @ PA4 thermistor
        movs    r3, #7                        @ 384 cycles
        movs    r2, #1                        @ rank 1
        ldr_adc1
        bl      ADC_RegularChannelConfig
        movs    r1, #1
        ldr_adc1
        bl      ADC_Cmd                       @ ADON = 1
1:      movs    r1, #0x40                     @ ADONS
        ldr_adc1
        bl      ADC_GetFlagStatus
        cmp     r0, #0
        beq     1b
