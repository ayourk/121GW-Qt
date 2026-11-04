#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Copy a byte range of a sparse file (e.g. one partition of a 2 TB card image) to a new sparse file, data extents only.
usage: sparse_extract.py IN OUT START_LBA [SECTORS]   (512-byte sectors; SECTORS defaults to the rest of IN)"""
import os, sys
src, dst, start = sys.argv[1], sys.argv[2], int(sys.argv[3]) * 512
fi = os.open(src, os.O_RDONLY)
end = os.fstat(fi).st_size if len(sys.argv) < 5 else start + int(sys.argv[4]) * 512
fo = os.open(dst, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
os.ftruncate(fo, end - start)
pos = start
while pos < end:
    try: pos = os.lseek(fi, pos, os.SEEK_DATA)
    except OSError: break
    if pos >= end: break
    stop = min(os.lseek(fi, pos, os.SEEK_HOLE), end)
    while pos < stop:
        b = os.pread(fi, min(1 << 22, stop - pos), pos)
        if b.count(0) != len(b): os.pwrite(fo, b, pos - start)
        pos += len(b)
os.close(fi); os.close(fo)
