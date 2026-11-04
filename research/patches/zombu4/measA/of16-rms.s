@ PATCH rms-skip
@ OF-16: on every HY3131 RMS interrupt HY_Service calls HY_OnRMS (0x08008208), which assembles RMS<39:8> from the
@ register buffer, takes a double square root, keeps a 4-sample average and then calls per-mode consumers that are
@ bare `bx lr` (0x0800A5B2, 0x0800A5BA, 0x0800A7F8, 0x0800A7FA). Each result word (0x20007DD8, 0x20007DDC,
@ 0x20007DE0, 0x20007DE4, 0x20007DE8, 0x20007D78, 0x20008082, 0x20008083) has a single literal-pool reference, inside
@ HY_OnRMS, so nothing reads them. Fix: HY_Service no longer calls it (the bl becomes two nops). The interrupt and the
@ register read in HY_Service are unchanged (RMSIE stays part of the PEAK interrupt set; INTE is not touched).
@ AT 0x080061B8 LEN 4
	nop
	nop
