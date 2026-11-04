#!/usr/bin/zsh
# SPDX-License-Identifier: MIT
# Regression scenarios for the measB patches: runs each scenario on two images and diffs the outputs that matter
# (decoded glass, buzzer, BLE, HY3131 configuration writes), first as sequences of distinct states (time removed),
# then reports how many timed SCREEN samples differ.  regress.zsh REF.bin NEW.bin OUTDIR [scenario...]
setopt err_return no_clobber
ref=$1 new=$2 out=$3; shift 3
root=${0:A:h:h:h:h}
mkdir -p $out
typeset -A SC
S=$(seq -s, 2000 100 12000)
SC[dcv]="--rotary V --ms 12000 --ad1-at 6000:-15000,9000:400000 --keys 4000:RANGE,5000:RANGE:800"
SC[acv]="--rotary V --ms 12000 --keys 2500:MODE --freq 60"
SC[acmv]="--rotary mV --ms 12000 --keys 2500:MODE --freq 1000"
SC[acmv_lpf]="--rotary mV --ms 12000 --keys 2500:MODE,4000:SETUP:800 --freq 1000"
SC[hz]="--rotary Hz --ms 12000 --freq 1234"
SC[duty]="--rotary Hz --ms 12000 --freq 50 --duty 0.25 --keys 2500:MODE,4500:MODE"
SC[ohm]="--rotary Ohm --ms 12000 --ad1-at 6000:3000"
SC[cap]="--rotary Ohm --ms 12000 --keys 2500:MODE,3500:MODE"
SC[ma]="--rotary VA --jack mA --ms 12000 --keys 3000:MODE"
SC[temp]="--rotary mV --ms 12000 --keys 2500:MODE,3500:MODE"
SC[ble]="--rotary V --ms 12000 --keys 2500:PEAK:800"
SC[lowz]="--rotary LowZ --ms 12000"
(( $# )) || set -- ${(k)SC}
one() {
  local s=$1 im
  for im in ref new; do
    python3 -I $root/scripts/emu_run.py ${(P)im} $root/svd/STM32L1xx.svd --eeprom $root/meter-eeprom.bin --ad1 1430000 \
      --glass --screens $S ${=SC[$s]} >| $out/$s.$im.txt 2>&1 &
  done
  wait
  for im in ref new; do
    { /usr/bin/grep -E 'SCREEN' $out/$s.$im.txt | sed 's/.*: //' | uniq
      /usr/bin/grep -E '^BUZZER|^BLE TX [0-9]' $out/$s.$im.txt
      /usr/bin/grep -E 'hy t=.* W @(2|1B|1F)' $out/$s.$im.txt | sed 's/.*hy t= *[0-9.]* //' | uniq
    } >| $out/$s.$im.norm
  done
  nd=$(diff $out/$s.ref.norm $out/$s.new.norm | /usr/bin/grep -c '^[<>]' || true)
  td=$(paste -d'\n' <(/usr/bin/grep SCREEN $out/$s.ref.txt) <(/usr/bin/grep SCREEN $out/$s.new.txt) | paste - - | awk -F'\t' '$1!=$2' | wc -l)
  print "$s: $nd differing state lines, $td of $(/usr/bin/grep -c SCREEN $out/$s.ref.txt) timed screens differ" >| $out/$s.sum
}
n=0
for s in $@; do                     # three scenarios (six emulator runs) at a time
  one $s &
  (( ++n % 3 )) || wait
done
wait
for s in $@; do cat $out/$s.sum; done
