@ PATCH log-mean
@ OF-35 (behavior change): Record_EncodeMain (0x08010908) fills the log record value 0x20007D28 from the per-sample
@ value g_reading_fast 0x20007D68 in every mode but 5..9 and 12, which use the displayed mean g_reading 0x20007D64.
@ The record of a V or A reading is then one unaveraged sample, which can differ from what the display showed (a
@ step 1 V -> 4 V logs 39999 while the display still shows the mixed mean). The three loads of the per-sample value
@ (0x080109B2, 0x080109CC, 0x080109DC, literal 0x08011150) are pointed at the literal of g_reading one word before
@ it (0x0801114C, used by the mean branch of the same function), so every mode logs the displayed value.
@ AT 0x080109B2 LEN 4
        .inst.w 0xf8df0798              @ ldr.w r0, [pc, #0x798] -> 0x0801114C (&g_reading)
@ AT 0x080109CC LEN 4
        .inst.w 0xf8df077c              @ ldr.w r0, [pc, #0x77c] -> 0x0801114C
@ AT 0x080109DC LEN 4
        .inst.w 0xf8df076c              @ ldr.w r0, [pc, #0x76c] -> 0x0801114C
