#!/bin/zsh
# Calibrated firmware vs a mis-clocked MSI: INFO, a 1 KB download of CAL.BIN, INFO again, at several MSI errors.
#   emu-cal.zsh IMG OUTDIR [ERR...]   (default errors: 2.5 -2.0 0)
set -u
img=$1; out=$2; shift 2; if (( $# )); then errs=($@); else errs=(2.5 -2.0 0); fi; mkdir -p $out
cd ${0:A:h:h:h}
base=(python3 -I scripts/emu_run.py $img svd/STM32L1xx.svd --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --nvic --hy-only
      --ble-peer scripts/ble_peer_ymodem.py --ble-peer-arg start=4.5 --keys 2500:PEAK:800)
for e in $errs; do
  ( TMPDIR=$out $base --ms 120000 --sd out/sd/fat32-cal.img --sd-out $out/msi$e.img --msi-error $e \
      --ble-peer-arg "ops=info;get1k:CAL.BIN:$out/msi${e}_cal.bin;info;wait:3" >| $out/msi$e.log 2>&1 ) &
done
wait
for e in $errs; do
  hz=$(grep 'PEER RESULT info:' $out/msi$e.log | tail -1 | sed 's/.*ok OK //; s/ .*//' | python3 -I -c 'import sys,struct; p=bytes.fromhex(sys.stdin.read().strip()); print(struct.unpack_from("<I",p,19)[0] if len(p)>=23 else -1)')
  want=$(python3 -I -c "print(round(4194304*(1+$e/100)))")
  dev=$(python3 -I -c "print(f'{($hz/$want-1)*1e6:+.0f} ppm' if $hz>0 else 'none')")
  if cmp -s $out/msi${e}_cal.bin out/sd/cal.bin; then dl=ok; else dl=FAIL; fi
  print "msi $e%: meter measured $hz Hz (true $want, $dev); 1 KB download $dl; $(grep '^CLOCK' $out/msi$e.log | sed 's/.*BRR/BRR/; s/; TIM11/ | TIM11/')"
  nopll=$(grep '^CLOCK' $out/msi$e.log | sed -n 's/.*SDIO accesses with the PLL off \([0-9]*\).*/\1/p')
  [[ ${nopll:-x} == 0 ]] && print "PASS msi $e%: no SDIO access with the PLL off" || print "FAIL msi $e%: SDIO accessed with the PLL off (${nopll:-no report}): card sessions would fail (SD_ERROR)"
done
