#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Assembles the measB byte-patch fragments (of*.s here) into bytes.json (patch_stock.py --extra format).
# Fragment syntax:  "@ PATCH name" once; ".equ" lines for stock addresses; then blocks headed
#   "@ AT 0xADDR LEN n"  (in place: padded with nop to n bytes; OLD = the stock bytes) or
#   "@ APPEND 0xADDR"    (appended code in the measB free range 0x08029E00-0x08029FBF; OLD = "").
# All blocks of a fragment are linked together, so a label in one block can be the target of a bl in another.
import glob, json, os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "../../.."))
BASE, FREE = 0x08006000, (0x08029E00, 0x08029FC0)
stock = open(os.path.join(ROOT, "EEVBlog2_05.bin"), "rb").read()
tmp = os.path.join(tempfile.gettempdir(), "measB-mk"); os.makedirs(tmp, exist_ok=True)
HDR = re.compile(r"^@ (AT|APPEND) (0x[0-9a-fA-F]+)(?: LEN (\d+))?[^\n]*\n", re.M)
out, used = {}, []
for f in sorted(glob.glob(os.path.join(HERE, "of*.s"))):
    src = open(f).read(); stem = os.path.basename(f)[:-2]
    name = re.search(r"^@ PATCH (\S+)", src, re.M).group(1)
    p = HDR.split(src); head = p[0]
    blocks = [(p[i], int(p[i + 1], 16), p[i + 2], p[i + 3]) for i in range(1, len(p), 4)]
    asm = [".syntax unified", ".thumb", ".cpu cortex-m3", head]
    for n, (kind, addr, ln, body) in enumerate(blocks):
        if addr & 1: sys.exit(f"{f}: odd address {addr:08x}")
        asm += [f'.section .b{n},"ax",%progbits'] + ([".space 2"] if addr & 2 else []) + [body, f".global _e{n}", f"_e{n}:"]
    s, o, e = (os.path.join(tmp, f"{stem}.{x}") for x in "s o elf".split())
    open(s, "w").write("\n".join(asm) + "\n")
    subprocess.run(["arm-none-eabi-as", "-mcpu=cortex-m3", "-mthumb", s, "-o", o], check=True)
    subprocess.run(["arm-none-eabi-ld", "-e", "0", *[f"--section-start=.b{n}=0x{b[1] & ~3:08x}" for n, b in enumerate(blocks)], o, "-o", e], check=True)
    nm = subprocess.run(["arm-none-eabi-nm", e], capture_output=True, text=True, check=True).stdout
    sym = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}
    for n, (kind, addr, ln, body) in enumerate(blocks):
        bn = os.path.join(tmp, f"{stem}.b{n}.bin")
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "-j", f".b{n}", e, bn], check=True)
        nb = open(bn, "rb").read()[addr & 2: sym[f"_e{n}"] - (addr & ~3)]
        if kind == "AT":
            k = int(ln)
            if len(nb) > k: sys.exit(f"{f}: {len(nb)} bytes > LEN {k} at {addr:08x}")
            nb += b"\x00\xbf" * ((k - len(nb)) // 2); old = stock[addr - BASE: addr - BASE + k].hex()
        else:
            if not (FREE[0] <= addr and addr + len(nb) <= FREE[1]): sys.exit(f"{f}: {addr:08x}+{len(nb)} outside {FREE[0]:08x}-{FREE[1]:08x}")
            old = ""
        for a0, a1, who in used:
            if addr < a1 and a0 < addr + len(nb): sys.exit(f"{f}: {addr:08x}+{len(nb)} overlaps {who}")
        used.append((addr, addr + len(nb), f"{name}@{addr:08x}"))
        out.setdefault(name, []).append([f"{addr:08X}", old, nb.hex()])
        print(f"{name}: {kind} {addr:08x} {len(nb)} bytes")
tmpf = os.path.join(HERE, "bytes.json.part")
with open(tmpf, "w") as fh: json.dump(out, fh, indent=1); fh.flush(); os.fsync(fh.fileno())
os.replace(tmpf, os.path.join(HERE, "bytes.json"))
