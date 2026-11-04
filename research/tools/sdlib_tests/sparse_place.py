#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Copy the data extents of a sparse file (e.g. a freshly formatted partition) into another sparse file (a whole-card
image) at a sector offset; holes stay holes. The reverse of ../sparse_extract.py.
usage: sparse_place.py PART CARD START_LBA   (512-byte sectors; CARD must exist and be large enough)"""
import os, sys
src, dst, start = sys.argv[1], sys.argv[2], int(sys.argv[3]) * 512
fi = os.open(src, os.O_RDONLY); fo = os.open(dst, os.O_WRONLY)
end = os.fstat(fi).st_size
if start + end > os.fstat(fo).st_size: sys.exit("card too small for the partition")
pos = 0
while pos < end:
    try: pos = os.lseek(fi, pos, os.SEEK_DATA)
    except OSError: break
    stop = os.lseek(fi, pos, os.SEEK_HOLE)
    while pos < stop:
        b = os.pread(fi, min(1 << 22, stop - pos), pos)
        if b.count(0) != len(b): os.pwrite(fo, b, start + pos)
        pos += len(b)
os.close(fi); os.close(fo)
