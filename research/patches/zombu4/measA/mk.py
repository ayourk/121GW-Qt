#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Builds bytes.json (patch_stock.py --extra format) from the fragment files *.s here. A fragment file:
#   @ PATCH name                 patch-set name
#   .equ SYM, 0x...              stock addresses (absolute: bl / b.w / beq.w to them work)
#   @ AT 0xADDR LEN n            in-place block: padded with nop to n bytes, OLD = the stock bytes there
#   @ APPEND 0xADDR              appended block in this area's free range (OLD = "")
# Blocks of one file are linked together, so labels (global) can cross blocks. Literal pools (ldr =, .ltorg) stay
# inside their block.
import glob, json, os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "../../.."))
BASE = 0x08006000; FREE = (0x08029C40, 0x08029E00)
stock = open(os.path.join(ROOT, "EEVBlog2_05.bin"), "rb").read()
tmp = os.path.join(tempfile.gettempdir(), "measA"); os.makedirs(tmp, exist_ok=True)
out, used = {}, []
for f in sorted(glob.glob(os.path.join(HERE, "of*.s"))):
    src = open(f).read(); stem = os.path.basename(f)[:-2]
    name = re.search(r"^@ PATCH (\S+)", src, re.M).group(1)
    parts = re.split(r"^@ (AT|APPEND) (0x[0-9a-fA-F]+)(?: LEN (\d+))?[^\n]*\n", src, flags=re.M)
    head, blocks = parts[0], [(parts[i], int(parts[i + 1], 16), parts[i + 2], parts[i + 3]) for i in range(1, len(parts), 4)]
    asm = [".syntax unified", ".thumb", ".cpu cortex-m3", head]
    for n, (kind, addr, ln, body) in enumerate(blocks):
        asm += [f'.section .p{n},"ax",%progbits', ".p2align 2"] + ([f".space {addr & 3}"] if addr & 3 else []) + [body, f".global __end_p{n}", f"__end_p{n}:"]
    s, o, e = (os.path.join(tmp, f"{stem}.{x}") for x in ("s", "o", "elf"))
    open(s, "w").write("\n".join(asm) + "\n")
    subprocess.run(["arm-none-eabi-as", "-mcpu=cortex-m3", "-mthumb", s, "-o", o], check=True)
    subprocess.run(["arm-none-eabi-ld", "-e", "0", *[f"--section-start=.p{n}=0x{b[1] & ~3:08x}" for n, b in enumerate(blocks)],
                    o, "-o", e], check=True)
    nm = subprocess.run(["arm-none-eabi-nm", e], capture_output=True, text=True, check=True).stdout
    ends = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}
    for n, (kind, addr, ln, body) in enumerate(blocks):
        bn = os.path.join(tmp, f"{stem}.p{n}.bin")
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "-j", f".p{n}", e, bn], check=True)
        nb = open(bn, "rb").read()[addr & 3: ends[f"__end_p{n}"] - (addr & ~3)]
        if kind == "AT":
            k = int(ln)
            if len(nb) > k: sys.exit(f"{f}: {len(nb)} bytes > LEN {k} at {addr:08x}")
            nb += b"\x00\xbf" * ((k - len(nb)) // 2)
            old = stock[addr - BASE: addr - BASE + k].hex()
        else:
            if not (FREE[0] <= addr and addr + len(nb) <= FREE[1]): sys.exit(f"{f}: append {addr:08x}+{len(nb)} outside area")
            old = ""
        for a0, a1, who in used:
            if addr < a1 and a0 < addr + len(nb): sys.exit(f"{f}: {addr:08x}+{len(nb)} overlaps {who}")
        used.append((addr, addr + len(nb), f"{name}@{addr:08x}"))
        out.setdefault(name, []).append([f"{addr:08X}", old, nb.hex()])
        print(f"{name}: {kind} {addr:08x} {len(nb)} bytes")
json.dump(out, open(os.path.join(HERE, "bytes.json.tmp"), "w"), indent=1)
os.replace(os.path.join(HERE, "bytes.json.tmp"), os.path.join(HERE, "bytes.json"))
