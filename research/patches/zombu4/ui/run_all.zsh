#!/usr/bin/zsh
# run_all.zsh: every UI-area scenario of REPORT.md on IN.bin and FIX.bin (build.zsh first); outputs in
# ${TMPDIR:-/tmp}/<name>.{in,fix}.txt (+ .ee); three pairs (six emulators) at a time
cd ${0:A:h:h:h:h}
P=patches/zombu4/ui; T=${TMPDIR:-/tmp}
F=(--eeprom factory-eeprom.bin --glass)
CALK=(300:MODE,600:REL)
S1=$(seq -s, 3100 25 3800)
EE8=(--ee 4d0=a5 --ee 4d4=11 --ee 4d8=a5 --ee 4dc=19 --ee 4e0=a5 --ee 4e4=22)
K7=(--keys 2500:SETUP,2800:SETUP,3100:SETUP,3400:SETUP,3700:SETUP,4000:SETUP,4300:SETUP)
PK=$(for i in {6000..8000..2}; do printf "%d:2000803b=01," $i; done)7000:2000801c=01
typeset -a J
J+=("base -- $F --ad1 10000 --ms 8000 --keys 3000:PEAK:800,5000:RANGE --screens 2500,3500,4500,5500,6500,7500 --ble-all")
J+=("of36range --ww 2000808c:3 -- $F --ad1 300 --ms 13000 --keys $CALK,4000:SETUP,6500:RANGE --screens 6000,7000,7500,12000 --ee-out $T/of36range.@V@.ee")
J+=("of36ctl --ww 2000808c:3 -- $F --ad1 300 --ms 13000 --keys $CALK,4000:SETUP --screens 6000,11000,12000 --ee-out $T/of36ctl.@V@.ee")
J+=("of36lowz --ww 2000808c:3 -- $F --ad1 300 --ms 13000 --keys $CALK,4000:SETUP --rotary-at 6500:LowZ --screens 6000,7000,12000 --ee-out $T/of36lowz.@V@.ee")
J+=("of37ac --ww 2000809c:2 -- $F --ad1 5050 --freq 10000 --ms 9000 --ee 58=00 --ee 59=64 --ee 5a=01 --ee 5b=2c --keys $CALK,3000:MODE,3500:RANGE,4000:RANGE,6000:SETUP --screens 5500,7000,8500 --ee-out $T/of37ac.@V@.ee")
J+=("of07beep --ww 2000808c:1 -- $F --rotary Ohm --ad1 300 --ms 13000 --keys $CALK,3000:MODE,4000:SETUP --screens 3500,6000,12000 --ee-out $T/of07beep.@V@.ee")
J+=("of08 --ww 2000803c:2,20007e0c:4 -- $F --rotary uA --ad1 300 --ms 13000 $EE8 --keys $CALK,3000:MODE,4000:SETUP --screens 3500,6000,12000 --ee-out $T/of08.@V@.ee")
J+=("of08norm --ww 2000803c:2,20007e0c:4 -- $F --rotary uA --ad1 300 --ms 6000 $EE8 --keys 3000:MODE --screens 2500,4500,6000 --ee-out $T/of08norm.@V@.ee")
J+=("of34 --ww 40004404:1 --pcmax 100000 -- $F --ad1 1000 --ms 9000 --keys 2500:PEAK:800 --ad1-at 4000:600000,6500:1000")
J+=("of11 --ww 40004404:1,2000802b --pcmax 100000 -- $F --ad1 1000 --ms 8000 --keys 2500:PEAK:800,4500:PEAK:800,6440:PEAK:800 --ad1-at 5200:2000")
J+=("of45 --usart-gate --tim2-at 0801c3e0 --pc 0800d17e --pcmax 3 -- $F --ad1 10000 --ms 7000 --keys 2500:PEAK:800 --ad1-at 3500:20000 --screens 3000,4000,6500")
J+=("of10 --nvic --tim2-at 0800c32c --ww 40004404:1 --pcmax 100000 -- $F --ad1 10000 --ms 6000 --keys 2500:PEAK:800 --ble 4200:F430323032 --screens 5000")
J+=("of43 --ww 2000807b -- $F --ad1 10000 --ms 9000 --keys 3000:MODE:4500 --screens 8500")
J+=("of48 --ww 20000237 -- $F --ad1 10000 --ms 9000 --keys 2500:SETUP,2800:SETUP,3100:SETUP,3400:SETUP,4000:SETUP:800,5500:REL:1000,7500:SETUP:800 --screens 3800,7000,8800")
J+=("of48tap --ww 20000237 -- $F --ad1 10000 --ms 9000 --keys 2500:SETUP,2800:SETUP,3100:SETUP,3400:SETUP,4000:SETUP:800,5500:REL:100,6000:PEAK:100,6500:REL:100,7500:SETUP:800 --screens 3800,7000,8800")
J+=("of55 --ww 2000807b,40021018:4 --pcmax 400 -- $F --ad1 10000 --ms 16000 --keys 2500:MODE:800,13000:MODE:800 --poke 3500:20000230=0300 --rotary-at 9000:Hz,9300:V --screens 4000,8000,12000,15000")
J+=("of40 --nvic --pc-poke 0801c598:20008077=00 --tim2-at 0801c598 --ww 40021018:4 --pcmax 400 -- $F --ad1 10000 --ms 9000 --jack A --poke 3500:20000230=0300 --screens 3000,8000")
J+=("of41 --tim2-at 0801715a@5000 -- $F --ad1 12345 --ms 6500 $K7 --screens 4800,5000,5010,5250,5400,6000")
J+=("of41ctl -- $F --ad1 12345 --ms 6500 $K7 --screens 4800,5000,5010,5250,5400,6000")
J+=("of32beep --ww 40004404:1 --pcmax 100000 -- $F --rotary Ohm --ad1 10000 --ms 4000 --keys 2000:PEAK:800,3000:MODE --screens $S1")
J+=("of32open -- $F --rotary Ohm --ad1 0x7fffff --ms 4000 --keys 3000:MODE --screens $S1")
J+=("of32hz --ww 40004404:1 --pcmax 100000 -- $F --ad1 10000 --ms 4000 --keys 2000:PEAK:800 --rotary-at 3000:Hz --freq 1000 --screens $S1")
J+=("of32hz0 -- $F --ad1 10000 --ms 9000 --rotary-at 3000:Hz --freq 0.2 --screens 3500,5000,6500,8000,9000")
J+=("of38 --ww 20000237 -- $F --ad1 10000 --ms 3000 --ee 1a=0a --screens 2900")
J+=("of23 -- $F --rotary Ohm --ad1 5000 --ms 6000 --keys 2500:MODE,3000:MODE,4000:RANGE --screens 3800,5500 --ee-out $T/of23.@V@.ee")
J+=("of47 --ww 2000801b,20007fe1 --pcmax 20 -- $F --ad1 10000 --ms 9000 --sd out/sd/fat32-mbr.img --keys 2500:MEM:800 --poke $PK --screens 4500,6500,7500,8500")
for r in LowZ V mV Hz Ohm VA uA A; do J+=("reg$r -- --eeprom meter-eeprom.bin --glass --rotary $r --ad1 20000 --ms 6000 --screens 2500,3500,4500,5500"); done
n=0
for j in $J; do
  $P/pair.zsh ${=j} &
  (( ++n % 3 == 0 )) && wait
done
wait
