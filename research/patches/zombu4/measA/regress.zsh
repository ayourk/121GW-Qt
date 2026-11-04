#!/bin/zsh
# Regression: the same scenarios on two images (default base-c.bin vs final.bin); compares SCREEN lines (--glass), the
# HY3131 transactions (--hy-only), beeps / STATE and BLE TX bytes. Prints "same" or the differing lines per scenario.
#   regress.zsh [A.bin B.bin]
emulate -L zsh
R=${0:A:h:h:h:h}; O=${TMPDIR:-/tmp}/measA/regress; mkdir -p $O
A=${1:-${TMPDIR:-/tmp}/measA/base-c.bin}; B=${2:-${TMPDIR:-/tmp}/measA/final.bin}
EE=(--eeprom $R/factory-eeprom.bin)
typeset -A SC
SC[dcv]="--rotary V --ad1 20000 --keys 4000:RANGE,5000:HOLD,6000:HOLD,7000:REL"
SC[acv]="--rotary V --ad1 20000 --freq 60 --keys 4000:MODE,6000:PEAK,8000:PEAK"
SC[acdc]="--rotary V --ad1 20000 --freq 60 --keys 4000:MODE,5000:MODE"
SC[acvlpf]="--rotary V --ad1 20000 --freq 60 --keys 4000:MODE,5000:REL:800"
SC[mv]="--rotary mV --ad1 12345 --keys 4000:MINMAX,6000:MINMAX:800"
SC[temp]="--rotary mV --ad1 1000 --keys 4000:MODE,5000:MODE,7000:SETUP:800"
SC[ohm]="--rotary Ohm --ad1 30000 --keys 4000:RANGE"
SC[beep]="--rotary Ohm --ad1 300 --keys 4000:MODE"
SC[diode]="--rotary Ohm --ad1 6000 --keys 4000:MODE,4500:MODE"
SC[cap]="--rotary Ohm --ad1 6000 --freq 100 --keys 4000:MODE,4500:MODE,5000:MODE"
SC[hz]="--rotary Hz --freq 1234.5 --duty 0.3 --keys 4000:MODE,6000:MODE"
SC[lowz]="--rotary LowZ --ad1 0 --ad1-at 6000:3000,8000:0"
SC[ua]="--rotary uA --ad1 15000 --keys 4000:MODE,6000:MODE"
SC[a]="--rotary A --jack A --ad1 15000 --keys 4000:MODE"
SC[va]="--rotary VA --jack A --ad1 15000 --keys 4000:MODE"
SC[ble]="--rotary V --ad1 20000 --keys 4000:PEAK:800 --ble-all"
run() { # img tag name
  local args=(${=SC[$3]})
  python3 -I $R/scripts/emu_run.py $1 $R/svd/STM32L1xx.svd $EE $args --ms 10000 --screens 4900,5900,6900,7900,8900,9900 --glass >| $O/$3.$2.log 2>&1
  python3 -I $R/scripts/emu_run.py $1 $R/svd/STM32L1xx.svd $EE $args --ms 10000 --hy-only >| $O/$3.$2.hy 2>&1
}
names=(${(k)SC}); i=0
[[ -n $COMPARE_ONLY ]] || for n in $names; do run $A a $n & run $B b $n & (( ++i % 3 == 0 )) && wait; done; wait
for n in ${(o)names}; do
  fa=$O/$n.a.log; fb=$O/$n.b.log
  da=$(grep -hE "SCREEN|STATE|BLE|beep" $fa | sed 's/sha256 [0-9a-f]*//'); db=$(grep -hE "SCREEN|STATE|BLE|beep" $fb | sed 's/sha256 [0-9a-f]*//')
  ha=$(/usr/bin/grep -v -E "^image|^end t=" $O/$n.a.hy | md5sum); hb=$(/usr/bin/grep -v -E "^image|^end t=" $O/$n.b.hy | md5sum)
  if [[ $da == $db && $ha == $hb ]]; then print "$n: same ($(print -r -- $da | wc -l) lines, HY trace $(wc -l < $O/$n.a.hy) lines)"
  else print "$n: DIFF (screens $([[ $da == $db ]] && print same || print differ), HY $([[ $ha == $hb ]] && print same || print differ))"; diff <(print -r -- $da) <(print -r -- $db) | head -8; fi
done
