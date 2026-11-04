#!/usr/bin/zsh
# SPDX-License-Identifier: MIT
# Runs each scenario below on two images and compares SCREEN (--glass), STATE, buzzer, BLE TX (--ble-all) and HY3131
# lines; prints one verdict line per scenario. Logs: ${TMPDIR:-/tmp}/regress/<scenario>.<A|B>.log
#   regress.zsh A.bin B.bin [SCENARIO ...]      (no names: all; at most 6 emulator runs at a time)
emulate -L zsh
setopt err_return pipe_fail
A=$1 B=$2; shift 2
cd ${0:A:h:h:h:h}
OUT=${TMPDIR:-/tmp}/regress; mkdir -p $OUT
typeset -A SC
SC[hz-1m5]="--rotary Hz --freq 1500000 --ms 6000 --screens 2000,3000,4000,5900"
SC[hz-10k]="--rotary Hz --freq 10000 --ms 6000 --screens 2000,3000,4000,5900"
SC[acv-1m5]="--rotary V --keys 2600:MODE --freq 1500000 --ad1 10000 --ms 7000 --screens 3500,4500,5500,6900"
SC[acv-1k-pages]="--rotary V --keys 2600:MODE,3100:SETUP,3600:SETUP,4100:SETUP,4600:SETUP,5100:SETUP,5600:SETUP,6100:SETUP,6600:SETUP,7100:SETUP,7600:SETUP,8100:SETUP,8600:SETUP --freq 1000 --ad1 20000 --ms 10000 --screens 7500,8000,8500,9900"
SC[acv-ranges]="--rotary V --keys 2600:MODE,3100:SETUP,3600:SETUP,4100:SETUP,4600:SETUP,5100:SETUP,5600:SETUP,6100:SETUP,6600:SETUP,7100:SETUP,7600:SETUP,8100:SETUP,8600:RANGE,9100:RANGE,9600:RANGE,10100:RANGE,10600:RANGE --freq 1000 --ad1 20000 --ms 12000 --screens 8500,9000,9500,10000,10500,11000,11900"
SC[va-ble]="--rotary VA --keys 2600:PEAK:800,4000:SETUP,4500:SETUP,5000:SETUP,5500:SETUP,6000:SETUP,6500:SETUP,7000:SETUP,7500:SETUP,8000:SETUP,8500:SETUP,9000:SETUP,9500:MODE,10000:SETUP,10500:SETUP,11000:SETUP,11500:SETUP,12000:SETUP,12500:SETUP,13000:SETUP,13500:SETUP,14000:SETUP,14500:SETUP,15000:SETUP,15500:SETUP --ad1 20000 --ms 18000 --screens 8500,9000,9500,11000,16000,16500,17900"
SC[va-ac-ble]="--rotary VA --keys 2600:PEAK:800,3800:MODE,4500:SETUP,5000:SETUP,5500:SETUP,6000:SETUP,6500:SETUP,7000:SETUP,7500:SETUP,8000:SETUP,8500:SETUP,9000:SETUP,9500:SETUP,10000:SETUP --ad1 20000 --freq 60 --ms 12000 --screens 9000,9500,10000,11900"
SC[ohm-r6]="--rotary Ohm --ee 178=64 --keys 2000:RANGE --ad1-at 2600:0,3600:1,4600:30000,5600:55999,6600:56000 --ms 7500 --screens 3500,4500,5500,6500,7400"
SC[dial-off-on]="--rotary V --poke 3000:0x20000230=02000000 --rotary-at 8000:Ohm,11000:V --ad1 20000 --ms 13000 --screens 2900,5900,8900,12900"
SC[setup-contrast]="--rotary V --keys 2600:SETUP,3100:SETUP,3600:SETUP,4100:SETUP,4600:SETUP,5200:SETUP:800,6500:REL,7200:REL,7900:REL,8600:PEAK,9300:SETUP:800 --ms 11000 --screens 4500,5000,6400,7100,7800,8500,9200,10900"
names=($@); (( $#names )) || names=(${(ko)SC})
run1() {   # scenario image tag
  : >| $OUT/$1.$3.cov    # --coverage appends
  python3 -I scripts/emu_run.py $2 svd/STM32L1xx.svd --eeprom factory-eeprom.bin --glass --ble-all --coverage $OUT/$1.$3.cov ${=SC[$1]} \
    >| $OUT/$1.$3.log 2>&1
}
for n in $names; do
  [[ -n ${SC[$n]} ]] || { print "unknown scenario $n"; continue; }
  while (( $(jobs -r | wc -l) >= 6 )); do sleep 0.5; done
  run1 $n $A A &
  while (( $(jobs -r | wc -l) >= 6 )); do sleep 0.5; done
  run1 $n $B B &
done
wait
for n in $names; do
  for t in A B; do
    /usr/bin/grep -E '^ *(SCREEN|STATE|BUZZER|BLE|HY3131|hy )|^end ' $OUT/$n.$t.log >| $OUT/$n.$t.cmp || true
  done
  if cmp -s $OUT/$n.A.cmp $OUT/$n.B.cmp; then
    print "$n: same ($(wc -l < $OUT/$n.A.cmp) lines compared)"
  else
    print "$n: DIFFERS ($(diff $OUT/$n.A.cmp $OUT/$n.B.cmp | /usr/bin/grep -c '^[<>]') lines)"
  fi
done
