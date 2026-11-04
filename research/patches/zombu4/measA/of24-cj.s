@ PATCH cj-below-20
@ OF-24: TC_ColdJunctionEmf returns without writing the cold-junction EMF 0x20007E7C for board temperatures below
@ -20.0 degC (negative and |t| >= 201 in 0.1 degC), with no fault flag, so the thermocouple is evaluated against a
@ stale junction (power-on: 0 uV, i.e. 0 degC). The K table at 0x08025CF8 goes down to -210 degC (index 0), so the
@ negative side can use the same 100.0 degC limit as the positive side. Fix: drop the 201 test (cmp + bge become
@ nops: the following cmp r1, #1000 / bgt then bounds both signs).
@ AT 0x08013D02 LEN 4
	nop
	nop
