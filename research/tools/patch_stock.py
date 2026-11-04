#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# In-place patches to stock 2.05 (docs/binary-patching.md). Refuses unless the input is the known image (sha256)
# and every site holds the expected original bytes. Emulator-tested only; not for a meter without the no-op
# patch proving the update path first.
#   patch_stock.py [--extra SET.json ...] EEVBlog2_05.bin OUT.bin PATCH [PATCH ...]   ("list" prints them)
import hashlib, json, sys

BASE = 0x08006000
STOCK_END = 0x080294E8                # first byte after stock 2.05
KNOWN = "8f885f12f567c2cc"       # first 16 hex digits of sha256(EEVBlog2_05.bin), as emu_run.py prints it
PATCHES = {
    # Key_LongPress, REL long in a current mode on range 0: stores g_reading (the residual after the trim already
    # applied) instead of the accumulated trim 0x20007e0c, so a second long REL saves ~0 and the zero is lost at
    # the next power-on. Fix: load the trim's literal (0x08016cac) instead of g_reading's (0x08016cb0).
    "zero-trim": [(0x0801644A, "dff86408", "dff86008")],
    # disk_read / disk_write build the SD byte address as sector << 9 in 32 bits with the high word 0, so cards over
    # 4 GiB wrap to sector mod 2^23 (docs/sd-card.md). Fix: high word = sector >> 23 (movs r3,#0 -> lsrs r3,r5,#23).
    # Exact for single-sector writes (all the logger does); a multi-sector disk_write straddling 4 GiB stays wrong.
    "big-card": [(0x08023F98, "0023", "eb0d"), (0x08023FFC, "0023", "eb0d")],
    # Temp_ToDisplayTenths rounds on the kept digit ((x / 10) % 10 >= 5) instead of the dropped one (x % 10): values
    # with a tenths digit of 5-9 show 0.1 deg high. Fix: q = x / 10, r = x - 10 q, r5 = q; cmp/blt/adds unchanged.
    # get_fattime returns the FatFs sample constant (2006-02-09 22:30:00): every file is created in 2006. Fix: the
    # entry branches to patches/fattime.s (44 bytes appended at 0x08029700), which packs the clock-chip copy.
    "fattime": [(0x08029700, "", "094b187814304006597840ea4150997840ea0140d97840eac120197940ea4110597940ea51007047d7800020"), (0x0802402E, "01487047", "05f067bb")],
    # Negative readings came out 2 counts toward zero: (1) HY_AD1Signed converts the 24-bit AD1 result with
    # raw - 0xFFFFFF (off by one) and treats 0x800000 as positive; (2) Cal_DivideGain rounds with trunc(x + 0.5),
    # which rounds negatives toward zero. Fix: raw - 0x1000000 for raw >= 0x800000, and round half away from zero
    # (patches/neg_round.s at 0x08029740: -0.5 for negatives before the same truncation).
    "neg-round": [(0x08006322, "09d9", "09d3"), (0x08006324, "7ff07f42", "4ff08072"),
                  (0x08029740, "", "11f0004f01d043f00043e9f735bd"), (0x0800F42A, "03f0c5fe", "1af089f9")],
    "temp-round": [(0x08018AF6, "0a2095fbf0f52db20a2095fbf0f100fb1150", "0a2095fbf0f100fb11500d0000bf00bf00bf")],
}

def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "list":
        for k, v in PATCHES.items(): print(k, [(hex(a), o, n) for a, o, n in v])
        return
    args = sys.argv[1:]
    while "--extra" in args:                      # extra patch sets: JSON {name: [[addr_hex, old_hex, new_hex], ...]}
        i = args.index("--extra")
        for k, v in json.load(open(args[i + 1])).items():
            if k in PATCHES: sys.exit(f"refused: extra patch {k} already defined")
            PATCHES[k] = [(int(a, 16), o, n) for a, o, n in v]
        del args[i:i + 2]
    src, dst, names = args[0], args[1], args[2:]
    img = bytearray(open(src, "rb").read())
    h = hashlib.sha256(img).hexdigest()
    if not h.startswith(KNOWN):
        sys.exit(f"refused: {src} sha256 {h[:16]} is not stock 2.05 ({KNOWN})")
    for name in names:
        if name not in PATCHES: sys.exit(f"refused: unknown patch {name}")
        for addr, old, new in PATCHES[name]:
            o, ob, nb = addr - BASE, bytes.fromhex(old), bytes.fromhex(new)
            if not ob:                                    # appended code: only past the end of the stock image
                if o < STOCK_END - BASE: sys.exit(f"refused: {name} appends at {addr:08x}, inside the image")
                if len(img) < o: img.extend(b"\xff" * (o - len(img)))
                if any(b != 0xFF for b in img[o:o + len(nb)]): sys.exit(f"refused: {name} at {addr:08x} overlaps")
                img[o:o + len(nb)] = nb
                while len(img) % 4: img.append(0xFF)      # the bootloader writes whole words only
                print(f"{name}: {addr:08x} + {len(nb)} bytes appended"); continue
            if len(ob) != len(nb): sys.exit(f"refused: {name} at {addr:08x} changes size")
            if img[o:o + len(ob)] != ob:
                sys.exit(f"refused: {name} at {addr:08x} holds {img[o:o + len(ob)].hex()}, expected {old}")
            img[o:o + len(nb)] = nb
            print(f"{name}: {addr:08x} {old} -> {new}")
    open(dst, "wb").write(bytes(img))
    print(f"wrote {dst} sha256 {hashlib.sha256(img).hexdigest()[:16]}")

main()
