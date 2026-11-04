#!/bin/zsh
# Real-meter Bluetooth download error rate vs the meter's output pacing: CAL.BIN downloaded REPEAT times per pace
# (bytes per 100 ms burst, 1..20), with 128-byte and/or 1 KB blocks; counts damaged blocks per block sent.
#   pace-test.zsh OUTDIR "PACES" REPEAT [--1k] [GAP]   e.g. pace-test.zsh /tmp/x "4 8 16 20" 4 "" 1
#   (GAP: extra 100 ms between bursts, 0..7; PACES may be 0 = the meter's default)
set -u
out=$1; paces=(${=2}); rep=$3; k=${4:-}; gap=${5:-0}; mkdir -p $out
cd ${0:A:h}
for p in $paces; do
  bad=0; good=0; fails=0
  for i in {1..$rep}; do
    log=$out/p$p-g$gap-$i${k:+-1k}.log
    print -n >| $out/p$p-$i.bin                   # a stale copy from an earlier run must not pass the compare
    timeout 200 python3 -I ymodem_ble.py get CAL.BIN ${k} --pace $p --gap $gap -o $out/p$p-$i.bin >| $log 2>&1
    if cmp -s $out/p$p-$i.bin ${CAL_REF:?set CAL_REF to the CAL.BIN being downloaded}; then
      nb=$(grep -c 'bad block' $log); bad=$((bad + nb))
      if [[ -n $k ]]; then good=$((good + 2)); else good=$((good + 10)); fi   # 1200 B: 1 KB + 128, or 10 x 128
    else
      fails=$((fails + 1))
    fi
    sleep 3
  done
  print "pace $p gap $gap${k:+ (1 KB)}: $rep downloads, $fails failed; $bad damaged blocks resent for $good delivered"
done
