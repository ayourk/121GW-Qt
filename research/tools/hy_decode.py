#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Decodes a 20-byte HY3131 configuration image (R20..R33, as HY_SetMode writes it in one burst) into the named
# bit fields of DS-HY3131-V10_EN section 8 (HY3131 register list). Fields given as (name, high bit, low bit).
#   hy_decode.py HEX40 [HEX40 ...]        or   hy_decode.py --positions out/hy-positions.txt
import sys, re

FIELDS = {
    0x20: [("SCMPI", 7, 5), ("ENCMP", 4, 4), ("ENCNTI", 3, 3), ("ENPCMPO", 2, 2), ("ENCTR", 1, 1)],
    0x21: [("SCMPRH", 7, 4), ("SCMPRL", 3, 0)],
    0x22: [("AD1OS", 7, 5), ("AD1CHOP", 4, 3), ("AD1OSR", 2, 0)],
    0x23: [("ENAD1", 7, 7), ("AD1RG", 4, 4), ("AD1RHBUF", 3, 3), ("AD1RLBUF", 2, 2), ("AD1IPBUF", 1, 1), ("AD1INBUF", 0, 0)],
    0x24: [("SAD1FP", 7, 4), ("SDIO", 3, 3), ("SAD1FN", 2, 0)],
    0x25: [("AD2IG", 7, 6), ("AD1IG", 5, 4), ("SACM", 3, 2), ("OPS2", 1, 1), ("OPS1", 0, 0)],
    0x26: [("ENAD2", 7, 7), ("ENCHOPAD2", 5, 5), ("AD2RG", 4, 4), ("SAD2CLK", 3, 3), ("AD2OSR", 2, 0)],
    0x27: [("SAD2IP", 7, 6), ("SAD2IN", 5, 4), ("SAD2RH", 3, 2), ("SAD2RL", 1, 0)],
    0x28: [("SAD1RH", 6, 4), ("SAD1RL", 2, 0)],
    0x29: [("ENRMS", 7, 7), ("ENLPF", 6, 6), ("LPFBW", 5, 3), ("ENPKH", 2, 2), ("PKHSEL", 1, 0)],
    0x2A: [("PS1", 7, 7), ("DS1", 6, 6), ("FS1", 5, 5), ("SS1", 4, 4), ("PS0", 3, 3), ("DS0", 2, 2), ("FS0", 1, 1), ("SS0", 0, 0)],
    0x2B: [("PS3", 7, 7), ("DS3", 6, 6), ("FS3", 5, 5), ("SS3", 4, 4), ("PS2", 3, 3), ("DS2", 2, 2), ("FS2", 1, 1), ("SS2", 0, 0)],
    0x2C: [("PS5", 7, 7), ("DS5", 6, 6), ("FS5", 5, 5), ("SS5", 4, 4), ("PS4", 3, 3), ("DS4", 2, 2), ("FS4", 1, 1), ("SS4", 0, 0)],
    0x2D: [("PS7", 7, 7), ("DS7", 6, 6), ("FS7", 5, 5), ("SS7", 4, 4), ("PS6", 3, 3), ("DS6", 2, 2), ("FS6", 1, 1), ("SS6", 0, 0)],
    0x2E: [("PS9", 7, 7), ("DS9", 6, 6), ("FS9", 5, 5), ("SS9", 4, 4), ("PS8", 3, 3), ("DS8", 2, 2), ("FS8", 1, 1), ("SS8", 0, 0)],
    0x2F: [("ENVS", 7, 7), ("SMODE", 6, 0)],
    0x30: [("SREFO", 7, 7), ("ACC", 6, 0)],
    0x31: [("ENREFO", 7, 7), ("ENBIAS", 6, 6), ("SAGND", 5, 4), ("SFUVR", 3, 0)],
    0x32: [("ENOP2", 7, 7), ("SOP2P", 6, 4), ("ENOP1", 3, 3), ("SOP1P", 2, 0)],
    0x33: [("OP1CHOP", 7, 6), ("ENOSC", 5, 5), ("ENXI", 4, 4), ("SFT1", 3, 2), ("SAD1I", 1, 0)],
}

def decode(hexs):
    b = bytes.fromhex(hexs)
    assert len(b) == 20, "need 20 bytes (R20..R33)"
    out = []
    for i, v in enumerate(b):
        r = 0x20 + i
        parts = []
        for nm, hi, lo in FIELDS[r]:
            f = (v >> lo) & ((1 << (hi - lo + 1)) - 1)
            if hi == lo:
                if f: parts.append(nm)
            else:
                parts.append(f"{nm}={f}")
        out.append(f"R{r:02X}={v:02x}  " + " ".join(parts))
    return out

def switches(hexs):
    """The PA/PB switch network bits R2A..R2E that are on, as a compact list."""
    b = bytes.fromhex(hexs)
    on = []
    for i in range(0x2A, 0x2F):
        for nm, hi, lo in FIELDS[i]:
            if b[i - 0x20] >> lo & 1: on.append(nm)
    return on

if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--positions":
        for line in open(args[1]):
            m = re.match(r"(\S+)\s+mode\s+(\d+) range (\d+)\s+emu (\S+)", line)
            if not m: continue
            print(f"== {m.group(1)}: mode {m.group(2)} range {m.group(3)}  {m.group(4)}")
            print("   switches on: " + (" ".join(switches(m.group(4))) or "none"))
            for l in decode(m.group(4)): print("   " + l)
    else:
        for h in args:
            print("== " + h)
            for l in decode(h): print("   " + l)
