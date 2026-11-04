#!/bin/zsh
# Bluetooth on at power-on as long PEAK last left it (EEPROM 0x1C: 1 = off, else on; custom firmware):
#   A factory EEPROM, no keys      -> BT symbol on, 0x1C untouched (0)
#   B factory EEPROM + long PEAK   -> BT off, 0x1C = 1
#   C B's EEPROM (power cycle)     -> BT off at power-on, 0x1C still 1
#   D B's EEPROM + long PEAK       -> BT on, 0x1C = 0
#   bt-default.zsh IMG OUTDIR
set -u
img=$1; out=$2; mkdir -p $out
cd ${0:A:h:h:h}
emu() { python3 -I scripts/emu_run.py $img svd/STM32L1xx.svd --hy-only --nvic --ms 7000 --screens 6000 --eeprom $1 --ee-out $out/$2.eep ${3:+--keys} ${3:-} >| $out/$2.log 2>&1 }
emu factory-eeprom.bin A
emu factory-eeprom.bin B 2500:PEAK:800
emu $out/B.eep C
emu $out/B.eep D 2500:PEAK:800
fails=0
chk() { local name=$1 run=$2 bt=$3 ee=$4
  local s=$(grep -m1 'SCREEN t=6000' $out/$run.log); local b=$(python3 -I -c "print(open('$out/$run.eep','rb').read()[0x1C])")
  local has=0; [[ $s == *" BT "* ]] && has=1
  if [[ $has == $bt && $b == $ee ]]; then print "PASS $name (BT symbol $has, EEPROM 0x1C = $b)"
  else print "FAIL $name (BT symbol $has want $bt, EEPROM 0x1C = $b want $ee): $s"; fails=$((fails+1)); fi }
chk "A: factory EEPROM, Bluetooth on at power-on" A 1 0
chk "B: long PEAK turns it off and saves 1" B 0 1
chk "C: power cycle keeps it off" C 0 1
chk "D: long PEAK turns it back on and saves 0" D 1 0
exit $fails
