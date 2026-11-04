#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Builds the HY3131 diagnostic image (patches/hy_diag.s) from stock 2.05: assembles and links the routine at
# 0x08029500 (free flash after the image) against the stock functions, appends it, and replaces BootScreen's
# `bl HY_SetMode` at 0x0800EA8A with `bl hy_diag`. Refuses unless the input is stock 2.05 and the hook site holds
# the expected original instruction.
#   build_hy_diag.py EEVBlog2_05.bin out/patched/hy-diag.bin
import hashlib, os, subprocess, sys, tempfile

BASE, ORG, HOOK = 0x08006000, 0x08029500, 0x0800EA8A
BLE_HOOK = 0x0800C5D4                              # BLE_BuildPacket_Thunk: bl BLE_BuildPacket
KNOWN = "8f885f12f567c2cc"
SYMS = {"HY_Read": 0x0800C0C0, "HY_Write": 0x0800C03C, "HY_SetMode": 0x0800B974,
        "LCD_DrawText5": 0x08019AE0, "LCD_Flush": 0x0801B304, "BLE_BuildPacket": 0x0800C5DA}
HERE = os.path.dirname(os.path.abspath(__file__))

def bl(src, dst):
    """Thumb-2 BL from src to dst (both even addresses)."""
    off = dst - (src + 4)
    assert -(1 << 24) <= off < (1 << 24) and off % 2 == 0
    s = (off >> 24) & 1; i1 = (off >> 23) & 1; i2 = (off >> 22) & 1
    j1 = (~i1 ^ s) & 1; j2 = (~i2 ^ s) & 1
    imm10 = (off >> 12) & 0x3FF; imm11 = (off >> 1) & 0x7FF
    hi = 0xF000 | (s << 10) | imm10; lo = 0xD000 | (j1 << 13) | (j2 << 11) | imm11
    return hi.to_bytes(2, "little") + lo.to_bytes(2, "little")

def main():
    src, dst = sys.argv[1], sys.argv[2]
    img = bytearray(open(src, "rb").read())
    h = hashlib.sha256(img).hexdigest()
    if not h.startswith(KNOWN): sys.exit(f"refused: {src} sha256 {h[:16]} is not stock 2.05")
    if bytes(img[HOOK - BASE:HOOK - BASE + 4]) != bl(HOOK, SYMS["HY_SetMode"]):
        sys.exit(f"refused: {HOOK:08x} is not bl HY_SetMode")
    if bytes(img[BLE_HOOK - BASE:BLE_HOOK - BASE + 4]) != bl(BLE_HOOK, SYMS["BLE_BuildPacket"]):
        sys.exit(f"refused: {BLE_HOOK:08x} is not bl BLE_BuildPacket")
    if BASE + len(img) > ORG: sys.exit("refused: image overlaps the patch origin")
    with tempfile.TemporaryDirectory(dir=tempfile.gettempdir()) as t:
        obj, elf, raw = (os.path.join(t, n) for n in ("d.o", "d.elf", "d.bin"))
        subprocess.run(["arm-none-eabi-as", "-mcpu=cortex-m3", "-o", obj, os.path.join(HERE, "hy_diag.s")], check=True)
        defs = sum((["--defsym", f"{k}=0x{v | 1:08x}"] for k, v in SYMS.items()), [])
        subprocess.run(["arm-none-eabi-ld", "-Ttext", f"0x{ORG:08x}", "-e", "hy_diag", *defs, "-o", elf, obj], check=True)
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "-j", ".text", elf, raw], check=True)
        nm = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True, check=True).stdout
        entry = int(next(l.split()[0] for l in nm.splitlines() if l.endswith(" hy_diag")), 16)
        ble_entry = int(next(l.split()[0] for l in nm.splitlines() if l.endswith(" ble_diag")), 16)
        blob = open(raw, "rb").read()
    img.extend(b"\xff" * (ORG - BASE - len(img)))
    img.extend(blob)
    while len(img) % 4: img.append(0xFF)          # the bootloader writes whole words only
    img[HOOK - BASE:HOOK - BASE + 4] = bl(HOOK, entry & ~1)
    img[BLE_HOOK - BASE:BLE_HOOK - BASE + 4] = bl(BLE_HOOK, ble_entry & ~1)
    if len(img) > 0x5A000: sys.exit("refused: larger than the 360 KB application area")
    open(dst, "wb").write(bytes(img))
    print(f"hy_diag at {entry & ~1:08x}, ble_diag at {ble_entry & ~1:08x}, {len(blob)} bytes; hooks {HOOK:08x} {BLE_HOOK:08x}; wrote {dst} ({len(img)} bytes) "
          f"sha256 {hashlib.sha256(img).hexdigest()[:16]}")

main()
