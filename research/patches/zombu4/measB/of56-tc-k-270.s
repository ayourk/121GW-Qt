@ PATCH tc-k-270
@ OF-56 (behavior change, 0.02 degC): the type K table at 0x08025CF8 (uV at -210 + 10 k degC) holds 10970 uV for
@ 270 degC (entry 48); the NIST ITS-90 table gives 10.971 mV (reference function 10970.95 uV), the other 157
@ entries agree with NIST. The entry is set to 10971.
@ AT 0x08025DB8 LEN 4
        .word   10971
