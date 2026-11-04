#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Read-only access to the root directory of an exFAT card image (MBR + first partition, or a bare volume), for
checking what the meter wrote: no mount, works on sparse multi-terabyte images (reads only the sectors it needs).
  exfat_tool.py ls CARD                name, size, attributes, created and modified time (local, as stored)
  exfat_tool.py cat CARD NAME          the file's bytes to stdout (name matched without regard to case)
Written from the exFAT specification published by Microsoft (2019); handles the root directory only."""
import struct, sys


class ExFat:
    def __init__(self, path):
        self.f = open(path, "rb")
        mbr = self.rd(0, 512)
        self.base = 0
        if mbr[3:11] != b"EXFAT   ":
            if mbr[510:512] != b"\x55\xaa": sys.exit("no MBR and no exFAT boot sector")
            self.base = struct.unpack_from("<I", mbr, 446 + 8)[0] * 512
        vbr = self.rd(self.base, 512)
        if vbr[3:11] != b"EXFAT   ": sys.exit("first partition is not exFAT")
        self.fat_off, self.fat_len, self.heap_off, self.nclus, self.root = struct.unpack_from("<IIIII", vbr, 80)
        self.bps, self.spc = 1 << vbr[108], 1 << vbr[109]
        self.csize = self.bps * self.spc

    def rd(self, off, n):
        self.f.seek(off); return self.f.read(n)

    def cluster(self, c):
        return self.rd(self.base + (self.heap_off * self.bps) + (c - 2) * self.csize, self.csize)

    def fat_next(self, c):
        return struct.unpack("<I", self.rd(self.base + self.fat_off * self.bps + 4 * c, 4))[0]

    def chain(self, first, size, contiguous):
        """Clusters of an object of `size` bytes (contiguous: the NoFatChain flag)."""
        n = (size + self.csize - 1) // self.csize
        if contiguous: return list(range(first, first + n))
        out, c = [], first
        while 2 <= c < 0xFFFFFFF7 and len(out) < max(n, 1) + 1:
            out.append(c); c = self.fat_next(c)
        return out

    def read(self, first, size, contiguous):
        return b"".join(self.cluster(c) for c in self.chain(first, size, contiguous))[:size]

    def root_entries(self):
        data = b"".join(self.cluster(c) for c in self.chain(self.root, 1 << 30, False))
        i, out = 0, []
        while i + 32 <= len(data):
            t = data[i]
            if t == 0: break
            if t == 0x85:                                   # file directory entry set
                n2 = data[i + 1]
                attr, = struct.unpack_from("<H", data, i + 4)
                ctime, mtime = struct.unpack_from("<I", data, i + 8)[0], struct.unpack_from("<I", data, i + 12)[0]
                s = data[i + 32:i + 64]
                flags, nlen = s[1], s[3]
                first, = struct.unpack_from("<I", s, 20); size, = struct.unpack_from("<Q", s, 24)
                name = b"".join(data[i + 64 + 32 * k + 2:i + 64 + 32 * k + 32] for k in range(n2 - 1))
                name = name.decode("utf-16-le")[:nlen]
                out.append(dict(name=name, attr=attr, ctime=ctime, mtime=mtime, first=first, size=size,
                                contiguous=bool(flags & 2)))
                i += 32 * (n2 + 1); continue
            i += 32
        return out


def ts(v):
    d, t = v >> 16, v & 0xFFFF
    return f"{1980 + (d >> 9)}-{d >> 5 & 15:02}-{d & 31:02} {t >> 11:02}:{t >> 5 & 63:02}:{(t & 31) * 2:02}"


def main():
    cmd, card = sys.argv[1], sys.argv[2]
    fs = ExFat(card)
    ents = fs.root_entries()
    if cmd == "ls":
        for e in ents:
            print(f"{e['name']:<20} {e['size']:10d}  attr {e['attr']:02x}  created {ts(e['ctime'])}  modified {ts(e['mtime'])}")
    elif cmd == "cat":
        for e in ents:
            if e["name"].upper() == sys.argv[3].upper():
                sys.stdout.buffer.write(fs.read(e["first"], e["size"], e["contiguous"])); return
        sys.exit(f"{sys.argv[3]}: not found")
    else:
        sys.exit(__doc__)


main()
