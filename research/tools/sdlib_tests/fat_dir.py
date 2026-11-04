#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Root directory of a FAT12/16/32 card image straight from the directory entries: 8.3 name, attribute byte, NT
case flags, size, created, modified and accessed date; also the partition start (for mtools' @@offset). Works on
an MBR card (first partition) or a bare volume ("super floppy"), sparse multi-terabyte images included (reads only
the sectors it needs). LFN entries are skipped (the 8.3 entry is shown).
  fat_dir.py CARD          entries
  fat_dir.py CARD --start  the volume's first byte offset only"""
import struct, sys

f = open(sys.argv[1], "rb")
def rd(off, n): f.seek(off); return f.read(n)
b0 = rd(0, 512)
base = 0
if not (b0[0] in (0xEB, 0xE9) and struct.unpack_from("<H", b0, 11)[0] in (512, 1024, 2048, 4096)):
    base = struct.unpack_from("<I", b0, 446 + 8)[0] * 512
if "--start" in sys.argv: print(base); sys.exit(0)
v = rd(base, 512)
bps, spc, rsv, nf, nroot, tot16, _, fsz16 = struct.unpack_from("<HBHBHHBH", v, 11)
fsz = fsz16 or struct.unpack_from("<I", v, 36)[0]
first_data = rsv + nf * fsz + (nroot * 32 + bps - 1) // bps
def chain(c):
    out = []
    fat = base + rsv * bps
    while 2 <= c < 0x0FFFFFF7 and len(out) < 1 << 16:
        out.append(c); c = struct.unpack("<I", rd(fat + 4 * c, 4))[0] & 0x0FFFFFFF
    return out
if fsz16:                                          # FAT12/16: fixed root directory
    root = rd(base + (rsv + nf * fsz) * bps, nroot * 32)
else:
    root = b"".join(rd(base + (first_data + (c - 2) * spc) * bps, spc * bps) for c in chain(struct.unpack_from("<I", v, 44)[0]))
dt = lambda D, T: f"{1980 + (D >> 9)}-{D >> 5 & 15:02}-{D & 31:02} {T >> 11:02}:{T >> 5 & 63:02}:{(T & 31) * 2:02}"
for i in range(0, len(root), 32):
    e = root[i:i + 32]
    if e[0] == 0: break
    if e[0] == 0xE5 or e[11] == 0x0F: continue
    ct, cd, ad = struct.unpack_from("<HHH", e, 14); mt, md = struct.unpack_from("<HH", e, 22); size = struct.unpack_from("<I", e, 28)[0]
    name = e[:8].decode("latin-1").rstrip() + ("." + e[8:11].decode("latin-1").rstrip() if e[8:11].strip() else "")
    print(f"{name:<12} attr {e[11]:02x} nt {e[12]:02x} {size:10d}  created {dt(cd, ct)}  modified {dt(md, mt)}  "
          f"accessed {1980 + (ad >> 9)}-{ad >> 5 & 15:02}-{ad & 31:02}")
