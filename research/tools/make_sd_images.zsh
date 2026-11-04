#!/bin/zsh
# SD card images for the emulator (scripts/sdcard_model.py): 64 MB each, in out/sd/.
#   fat32-mbr.img  MBR, one FAT32 partition from sector 2048 (as a real SDHC card ships)
#   fat16-flat.img FAT16 with no partition table (FatFs "super floppy")
#   fat32-cal.img  fat32-mbr.img + CAL.BIN (for the boot-time SD calibration load)
set -euo pipefail
cd ${0:A:h:h}
mkdir -p out/sd
d=out/sd
truncate -s 0 $d/fat32-mbr.img $d/fat16-flat.img $d/fat32-cal.img $d/part.img 2>/dev/null || true
truncate -s 64M $d/fat32-mbr.img
print -r -- 'start=2048, type=c' | /usr/sbin/sfdisk -q $d/fat32-mbr.img
truncate -s $(( (131072 - 2048) * 512 )) $d/part.img
/usr/sbin/mkfs.fat -F 32 -s 1 -n METER121GW $d/part.img >/dev/null
dd if=$d/part.img of=$d/fat32-mbr.img bs=512 seek=2048 conv=notrunc status=none
truncate -s 64M $d/fat16-flat.img
/usr/sbin/mkfs.fat -F 16 -n METER121GW $d/fat16-flat.img >/dev/null
truncate -s 0 $d/part.img
# fat32-cal.img: as fat32-mbr.img plus CAL.BIN (the first 1200 bytes of factory-eeprom.bin, as the meter saves it)
cp $d/fat32-mbr.img $d/fat32-cal.img
head -c 1200 factory-eeprom.bin >| $d/cal.bin
MTOOLS_SKIP_CHECK=1 /usr/bin/mcopy -o -i $d/fat32-cal.img@@1M $d/cal.bin ::/CAL.BIN
ls -l $d
