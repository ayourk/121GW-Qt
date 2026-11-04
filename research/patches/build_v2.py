#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Builds the v2 image: stock 2.05 + the four fixes (scripts/patch_stock.py zero-trim big-card temp-round fattime)
# + Bluetooth telemetry (patches/telemetry.s, which includes hy_diag.s SILENT) appended at 0x08029800, with two hooks:
# BootScreen's `bl HY_SetMode` (0x0800EA8A) -> hy_diag and the main loop's `bl HY_Poll` (0x08024B6A) -> tele_poll.
# Refuses unless the input is stock 2.05 and both hook sites hold the expected original instructions.
#   build_v2.py EEVBlog2_05.bin out/patched/v2.bin
import hashlib, os, subprocess, sys, tempfile

BASE, ORG = 0x08006000, 0x08029800
HOOKS = {0x0800EA8A: ("HY_SetMode", "hy_diag"), 0x08024B6A: ("HY_Poll", "tele_poll")}
SYMS = {"HY_Read": 0x0800C0C0, "HY_Write": 0x0800C03C, "HY_SetMode": 0x0800B974, "HY_Poll": 0x08006124,
        "LCD_DrawText5": 0x08019AE0, "LCD_Flush": 0x0801B304, "BLE_BuildPacket": 0x0800C5DA,
        "USART_SendByteBlocking": 0x0800D170, "USART_ITConfig": 0x0801CC08}
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)

def bl(src, dst):
    off = dst - (src + 4)
    assert -(1 << 24) <= off < (1 << 24) and off % 2 == 0
    s = (off >> 24) & 1; i1 = (off >> 23) & 1; i2 = (off >> 22) & 1
    j1 = (~i1 ^ s) & 1; j2 = (~i2 ^ s) & 1
    hi = 0xF000 | (s << 10) | ((off >> 12) & 0x3FF); lo = 0xD000 | (j1 << 13) | (j2 << 11) | ((off >> 1) & 0x7FF)
    return hi.to_bytes(2, "little") + lo.to_bytes(2, "little")

def main():
    src, dst = sys.argv[1], sys.argv[2]
    work = tempfile.gettempdir()
    with tempfile.TemporaryDirectory(dir=work) as t:
        fixed = os.path.join(t, "fixes4.bin")
        subprocess.run([sys.executable, "-I", os.path.join(ROOT, "scripts", "patch_stock.py"), src, fixed,
                        "zero-trim", "big-card", "temp-round", "fattime"], check=True)
        img = bytearray(open(fixed, "rb").read())
        for site, (orig, _) in HOOKS.items():
            if bytes(img[site - BASE:site - BASE + 4]) != bl(site, SYMS[orig]):
                sys.exit(f"refused: {site:08x} is not bl {orig}")
        if BASE + len(img) > ORG: sys.exit(f"refused: image (to {BASE + len(img):08x}) overlaps {ORG:08x}")
        obj, elf, raw = (os.path.join(t, n) for n in ("v.o", "v.elf", "v.bin"))
        subprocess.run(["arm-none-eabi-as", "-mcpu=cortex-m3", "-I", HERE, "-o", obj, os.path.join(HERE, "telemetry.s")], check=True)
        defs = sum((["--defsym", f"{k}=0x{v | 1:08x}"] for k, v in SYMS.items()), [])
        subprocess.run(["arm-none-eabi-ld", "-Ttext", f"0x{ORG:08x}", "-e", "tele_poll", *defs, "-o", elf, obj], check=True)
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "-j", ".text", elf, raw], check=True)
        nm = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True, check=True).stdout
        sym = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}
        blob = open(raw, "rb").read()
    img.extend(b"\xff" * (ORG - BASE - len(img))); img.extend(blob)
    while len(img) % 4: img.append(0xFF)
    for site, (_, new) in HOOKS.items(): img[site - BASE:site - BASE + 4] = bl(site, sym[new] & ~1)
    if len(img) > 0x5A000: sys.exit("refused: larger than the 360 KB application area")
    open(dst, "wb").write(bytes(img))
    print(f"v2: fixes4 + telemetry {len(blob)} B at {ORG:08x} (hy_diag {sym['hy_diag'] & ~1:08x}, tele_poll {sym['tele_poll'] & ~1:08x}); "
          f"wrote {dst} ({len(img)} B) sha256 {hashlib.sha256(img).hexdigest()[:16]}")

main()
