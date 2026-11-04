#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Adds the C patches (patches/c/*.c, linked by patch.ld at 0x0802A000 against stock_syms.ld) to an already
# byte-patched 2.05 image and redirects the calls listed in hooks.tsv. Refuses if the image reaches 0x0802A000, if a
# hook site does not hold `bl <expected stock callee>`, or if the result exceeds the 360 KB application area.
#   build_c.py IN.bin OUT.bin [--hooks EXTRA.tsv ...] [module.c ...]   (default modules: patches/c/*.c)
import hashlib, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
BASE, ORG = 0x08006000, 0x0802A000
CFLAGS = ["-mcpu=cortex-m3", "-mthumb", "-Os", "-ffreestanding", "-fno-builtin", "-nostdlib", "-ffunction-sections",
          "-fdata-sections", "-Wall", "-Wextra", "-Werror", "-fno-common", "-std=c11"]

def bl(src, dst):
    off = dst - (src + 4)
    assert -(1 << 24) <= off < (1 << 24) and off % 2 == 0
    s = (off >> 24) & 1; i1 = (off >> 23) & 1; i2 = (off >> 22) & 1
    j1 = (~i1 ^ s) & 1; j2 = (~i2 ^ s) & 1
    hi = 0xF000 | (s << 10) | ((off >> 12) & 0x3FF); lo = 0xD000 | (j1 << 13) | (j2 << 11) | ((off >> 1) & 0x7FF)
    return hi.to_bytes(2, "little") + lo.to_bytes(2, "little")

def names():
    n = {}
    for line in open(os.path.join(ROOT, "names.tsv")):
        if line.strip() and not line.startswith("#"):
            c = line.split("\t"); n[c[1]] = int(c[0], 16)
    return n

def main():
    args = sys.argv[1:]; extra_hooks = []
    while "--hooks" in args:
        i = args.index("--hooks"); extra_hooks.append(args[i + 1]); del args[i:i + 2]
    src, dst, mods = args[0], args[1], args[2:]
    if not mods: mods = sorted(os.path.join(HERE, f) for f in os.listdir(HERE) if f.endswith(".c"))
    img = bytearray(open(src, "rb").read())
    if BASE + len(img) > ORG: sys.exit(f"refused: input reaches {BASE + len(img):08x} (C patches start at {ORG:08x})")
    subprocess.run([sys.executable, "-I", os.path.join(HERE, "gen_stock_ld.py")], check=True, stdout=subprocess.DEVNULL)
    hooks = [l.rstrip("\n").split("\t") for f in [os.path.join(HERE, "hooks.tsv")] + extra_hooks
             for l in open(f) if l.strip() and not l.startswith("#")]
    stock = names()
    with tempfile.TemporaryDirectory(dir=tempfile.gettempdir()) as t:
        objs = []
        for m in mods:
            o = os.path.join(t, os.path.basename(m) + ".o")
            subprocess.run(["arm-none-eabi-gcc", *CFLAGS, "-I", HERE, "-c", m, "-o", o], check=True); objs.append(o)
        elf, raw = os.path.join(t, "c.elf"), os.path.join(t, "c.bin")
        subprocess.run(["arm-none-eabi-gcc", *CFLAGS, "-T", os.path.join(HERE, "patch.ld"), "-L", HERE,
                        "-Wl,--gc-sections", *[f"-Wl,--undefined={h[2]}" for h in hooks], "-o", elf, *objs, "-lgcc"], check=True)
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "-j", ".text", elf, raw], check=True)
        nm = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True, check=True).stdout
        sym = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}
        size = subprocess.run(["arm-none-eabi-size", "-A", elf], capture_output=True, text=True, check=True).stdout
        blob = open(raw, "rb").read()
    img.extend(b"\xff" * (ORG - BASE - len(img))); img.extend(blob)
    while len(img) % 4: img.append(0xFF)
    for site, callee, new, *_ in hooks:
        a = int(site, 16)
        if new not in sym: sys.exit(f"refused: {new} not linked")
        if bytes(img[a - BASE:a - BASE + 4]) != bl(a, stock[callee]): sys.exit(f"refused: {a:08x} is not bl {callee}")
        img[a - BASE:a - BASE + 4] = bl(a, sym[new] & ~1)
    if len(img) > 0x5A000: sys.exit("refused: larger than the 360 KB application area")
    open(dst, "wb").write(bytes(img))
    bss = next((l.split()[1] for l in size.splitlines() if l.startswith(".bss")), "0")
    print(f"C patches: {len(blob)} B code at {ORG:08x}, {bss} B bss; {len(hooks)} hooks; wrote {dst} ({len(img)} B) "
          f"sha256 {hashlib.sha256(img).hexdigest()[:16]}")

main()
