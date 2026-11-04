#!/bin/zsh
# SPDX-License-Identifier: MIT
# exFAT card images for the emulator (--sd-file: whole card, sparse), for the FatFs R0.16 tests (docs/sd-stack-v2.md).
#   make-cards.zsh OUTDIR
#   exfat-64g.img  124,735,488 sectors (a 64 GB SDXC card), MBR, one exFAT partition (type 7) from sector 32768
#   exfat-2tb.img  3,999,760,384 sectors (the 2 TB test card of docs/sd-card.md), exFAT partition from sector 131072
#   fat32-8g.img   16,777,216 sectors (8 GiB), out/sd/big8g-head.img (MBR) + big8g-part.img (FAT32) at sector
#                  8,390,656, just past 4 GiB (docs/sd-card.md "Cards over 4 GiB"), as one sparse card file
# The FAT32 2 TB card is out/sd/card-2tb.img (docs/sd-card.md "2 TB cards"). mkfs.exfat writes the partition into a
# sparse file of its own (it cannot format at an offset), sparse_place.py copies its data extents into the card;
# the partition file (OUTDIR/NAME.part, sparse) is kept for fsck comparisons.
set -euo pipefail
out=${1:?OUTDIR}; mkdir -p $out
here=${0:A:h}
mk() {   # NAME SECTORS START
    local card=$out/$1.img part=$out/$1.part
    [[ -e $card ]] && { print -r -- "$card exists, kept"; return }
    truncate -s $(( $2 * 512 )) $card
    print -r -- "start=$3, type=7" | /usr/sbin/sfdisk -q $card
    truncate -s $(( ($2 - $3) * 512 )) $part
    /usr/sbin/mkfs.exfat -q -L METER121GW $part
    python3 -I $here/sparse_place.py $part $card $3
    /usr/sbin/fsck.exfat -n $part
    du -h $card
}
mk exfat-64g 124735488 32768
mk exfat-2tb 3999760384 131072
if [[ ! -e $out/fat32-8g.img ]]; then
    truncate -s $(( 16777216 * 512 )) $out/fat32-8g.img
    python3 -I $here/sparse_place.py ${0:A:h:h:h}/out/sd/big8g-head.img $out/fat32-8g.img 0
    python3 -I $here/sparse_place.py ${0:A:h:h:h}/out/sd/big8g-part.img $out/fat32-8g.img 8390656
    du -h $out/fat32-8g.img
fi
