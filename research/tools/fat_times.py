#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Prints root-directory entries (8.3 name, size, created, modified, accessed date) of a FAT32 card image whose first
# partition starts at sector 2048 (out/sd/fat32-mbr.img layout), straight from the directory entries.
#   fat_times.py CARD.img
import struct, sys
d = open(sys.argv[1], "rb").read(); p = 2048 * 512
bps, spc, rsv, nf = struct.unpack_from("<HBHB", d, p + 11)
fsz, root = struct.unpack_from("<I", d, p + 36)[0], struct.unpack_from("<I", d, p + 44)[0]
rd = p + (rsv + nf * fsz) * bps + (root - 2) * spc * bps
dt = lambda D, T: f"{1980 + (D >> 9)}-{D >> 5 & 15:02}-{D & 31:02} {T >> 11:02}:{T >> 5 & 63:02}:{(T & 31) * 2:02}"
for i in range(0, spc * bps, 32):
    e = d[rd + i:rd + i + 32]
    if e[0] == 0: break
    if e[0] == 0xE5 or e[11] & 0x08: continue
    ct, cd, ad, mt, md, size = struct.unpack_from("<HHH", e, 14) + struct.unpack_from("<HH", e, 22) + (struct.unpack_from("<I", e, 28)[0],)
    print(f"{e[:8].decode().strip()}.{e[8:11].decode().strip():3s} {size:8d}  created {dt(cd, ct)}  modified {dt(md, mt)}  accessed {1980 + (ad >> 9)}-{ad >> 5 & 15:02}-{ad & 31:02}")
