#!/bin/zsh
# 146 KB V2.BIN downloaded with 1 KB blocks at a mis-clocked MSI, for one or more builds.
#   emu-cal-big.zsh OUTDIR MSI_ERR NAME=IMG [NAME=IMG...]
set -u
out=$1; err=$2; shift 2; mkdir -p $out
cd ${0:A:h:h:h}
for pair in $@; do
  n=${pair%%=*}; img=${pair#*=}
  ( TMPDIR=$out python3 -I scripts/emu_run.py $img svd/STM32L1xx.svd --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --nvic --hy-only \
      --ble-peer scripts/ble_peer_ymodem.py --ble-peer-arg start=4.5 --keys 2500:PEAK:800 --msi-error $err \
      --ms 1500000 --sd scripts/ymodem_tests/card-big.img --sd-out $out/$n.img \
      --ble-peer-arg "ops=info;get1k:V2.BIN:$out/${n}_v2.bin;info;wait:3" >| $out/$n.log 2>&1 ) &
done
wait
for pair in $@; do
  n=${pair%%=*}
  r=$(cmp -s $out/${n}_v2.bin out/patched/v2.bin && print 'V2.BIN byte-identical' || print 'V2.BIN missing or different')
  print "== $n (MSI $err%): $r; $(grep -c 'NAK' $out/$n.log) NAK lines"
  grep 'PEER RESULT get1k' $out/$n.log | cut -c1-140
  grep '^CLOCK' $out/$n.log
done
