        .syntax unified
        .cpu cortex-m3
        .thumb
        .thumb_func
        .global round_dadd
round_dadd:                         @ Cal_DivideGain adds 0.5 (r2:r3) before truncating: use -0.5 for negatives
        tst     r1, #0x80000000
        beq     1f
        orr     r3, r3, #0x80000000
1:      b.w     __aeabi_dadd
