#!/bin/zsh
# End-to-end clock-model checks in emu_run.py: no-flag regression, and a 1 KB BLE download with a baud-mismatched
# (MSI +2.5%) and a nominal (MSI 0%) meter clock.
#   emu-baud.zsh IMG OUTDIR [REF_T09_LOG]
set -u
img=$1; out=$2; ref=${3:-}; mkdir -p $out
cd ${0:A:h:h:h}
T=scripts/ymodem_tests
base=(python3 -I scripts/emu_run.py $img svd/STM32L1xx.svd --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --nvic --hy-only
      --ble-peer scripts/ble_peer_ymodem.py --ble-peer-arg start=4.5 --keys 2500:PEAK:800)
( TMPDIR=$out $base --ms 300000 --sd $T/card-log.img --sd-out $out/T09.img \
    --ble-peer-arg "ops=list;get:26110200.CSV:$out/T09_log.csv;get1k:CAL.BIN:$out/T09_cal.bin;cmd:11:00004e4f4e452e424e;wait:3" \
    >| $out/T09_noflag.log 2>&1; print "noflag done" ) &
for e in 2.5 0; do
  ( TMPDIR=$out $base --ms 200000 --sd out/sd/fat32-cal.img --sd-out $out/msi$e.img --msi-error $e \
      --ble-peer-arg "ops=get1k:CAL.BIN:$out/msi${e}_cal.bin;wait:3" >| $out/msi$e.log 2>&1; print "msi $e done" ) &
done
wait
if [[ -n $ref ]]; then
  diff <(grep -v -E 'image sha256|/tmp|jobs/' $ref | sed 's|[^ ]*/T09|T09|g') <(grep -v -E 'image sha256|/tmp|jobs/' $out/T09_noflag.log | sed 's|[^ ]*/T09|T09|g') >| $out/T09.diff
  [[ -s $out/T09.diff ]] && print "FAIL no-flag run differs from the reference ($(wc -l < $out/T09.diff) diff lines)" || print "PASS no-flag run identical to the reference"
fi
for e in 2.5 0; do
  print "msi $e: $(grep -c 'NAK' $out/msi$e.log) NAK lines; $(grep 'PEER RESULT get1k' $out/msi$e.log | cut -c1-110)"
  grep '^CLOCK' $out/msi$e.log
done
