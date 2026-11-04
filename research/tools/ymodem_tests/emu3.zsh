#!/bin/zsh
# Three emulator scenarios for the before/after check of emu_run.py; output in $2/
#   emu3.zsh EMU_RUN.py OUTDIR
set -eu
emu=$1 out=$2
mkdir -p $out
cd ${0:A:h:h:h}
img=${BUILDS:-${0:A:h:h:h:h}/builds}/custom-C-2.05-gcc14.2.1-01d3ca27.bin
common=(svd/STM32L1xx.svd --eeprom ${EEPROM:-factory-eeprom-btoff.bin})
rm -f $out/cov1 $out/cov2 $out/cov3
python3 -I $emu $img $common --ms 6000 --sd out/sd/fat32-cal.img --keys 3000:SETUP --screens 2500,4000,5500 --coverage-ranges $out/cov1 >| $out/s1.txt 2>&1
python3 -I $emu $img $common --ms 9000 --keys 2500:PEAK:800,4800:NONE --ble 5000:f430323032,6000:f8323631313033313530303030303f --ble-all --screens 7000 --coverage-ranges $out/cov2 >| $out/s2.txt 2>&1
python3 -I $emu $img $common --ms 12000 --nvic --sd out/sd/fat32-mbr.img --keys 2500:MEM:800,5000:MEM:800 --sd-log --screens 4000,9000 --coverage-ranges $out/cov3 >| $out/s3.txt 2>&1
