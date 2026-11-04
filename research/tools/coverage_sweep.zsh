#!/bin/zsh
# Coverage sweep of stock 2.05 in the emulator: every executed basic block of many fresh boots (factory-eeprom.bin)
# into out/coverage.txt. Scenarios: every dial position x MODE cycle x every key short/long, SETUP walk and edits on
# every page, boot keys, dial to OFF and back (sleep/wake), BLE on + every remote key + clock set, counter modes,
# AC 1 ms PEAK / 1 kHz LPF / AC+DC, MIN/MAX cycle, jack sense, Fahrenheit, low battery.
set -euo pipefail
cd ${0:A:h:h}
cov=out/coverage.txt
: >| $cov
covr=out/coverage-ranges.txt
: >| $covr
jobs_file=${TMPDIR:-/tmp}/cov-jobs.txt
: >| $jobs_file
emit() { print -r -- "$*" >>| $jobs_file; }      # pos ms keys [extra emu_run args]
seq_keys() { local t=$1; shift; local out=""; for k in "$@"; do out+="$t:$k,"; t=$((t + 1600)); done; print -r -- ${out%,}; }
for pos in LowZ V mV Hz Ohm VA uA A; do
    emit $pos 12000 "$(seq_keys 2500 MODE MODE MODE MODE MODE)"
    for k in MODE SETUP MINMAX MEM RANGE HOLD REL PEAK; do
        emit $pos 4500 "2500:$k"
        emit $pos 5200 "2500:$k:800"
    done
    # after each MODE step, every key once (reaches the AC, temperature, continuity, diode, cap, pulse, duty variants)
    for n in 1 2 3; do
        for k in RANGE HOLD REL PEAK MINMAX SETUP; do
            emit $pos 14000 "$(seq_keys 2500 $(repeat $n print -n 'MODE ') $k) ,$(( 2500 + 1600 * (n + 1) )):$k:800"
        done
    done
done
# SETUP: walk + edit on every page (hold SETUP = edit, REL = up, PEAK = down, hold SETUP = save)
for page in 1 2 3 4 5 6 7 8 9 10; do
    emit V 30000 "$(seq_keys 2500 $(repeat $page print -n 'SETUP ')),$((2500 + 1600 * page)):SETUP:800,$((3500 + 1600 * page)):REL,$((4500 + 1600 * page)):REL,$((5500 + 1600 * page)):PEAK,$((6500 + 1600 * page)):SETUP:800"
done
emit uA 20000 "$(seq_keys 2500 SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP),20500:SETUP:800,21500:REL,22500:SETUP:800"
emit VA 20000 "$(seq_keys 2500 SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP),22000:SETUP:800,23000:REL,24000:SETUP:800"
emit Ohm 20000 "2500:MODE,4100:SETUP,5700:SETUP:800,6700:REL,7700:SETUP:800"                         # continuity DN/UP
emit Ohm 20000 "2500:MODE,4100:MODE,5700:RANGE,7300:RANGE"                                          # diode 3/15 V
# boot keys (pressed while the 2 s boot screen is up)
emit V 6000 "50:RANGE:400"
emit V 6000 "50:SETUP:400"
emit V 9000 "50:MODE:300,500:REL:300,3500:SETUP,5000:MEM,6500:MODE"
emit V 9000 "50:MEM:400,800:REL:300,1500:MODE:300"
emit V 9000 "50:MEM:400,800:MODE:300"
# dial to OFF and back: sleep (STOP) and wake
for p in V Hz uA; do emit $p 9000 "2500:NONE" --rotary-at 3000:OFF,6000:$p; done
emit V 12000 "2500:NONE" --rotary-at 3000:mV,5000:Ohm,7000:A,9000:V
# BLE: on, every remote key short and long, then a clock-set frame
for code in 01 02 03 04 05 06 07 08 81 82 83 84 85 86 87 88; do
    hex=$(print -n "$code" | od -An -tx1 | tr -d ' \n')
    emit V 9000 "2500:PEAK:800,4800:NONE" --ble 5000:f4$hex$hex
done
emit V 9000 "2500:PEAK:800,4800:NONE" --ble 5000:f8314130423032304631453030$(print -n 0E | od -An -tx1 | tr -d ' \n')
# auto power-off: countdown poked to 4 s, STOP, then a dial turn wakes it (EXTI) and re-runs init, per position
for p in Hz Ohm uA A; do emit V 16000 "2500:NONE" --poke 3000:20000230=0400 --rotary-at 9000:$p; done
# clock pages read the emulated RTC chip; logging interval ticks
emit V 20000 "$(seq_keys 2500 SETUP SETUP SETUP SETUP SETUP SETUP SETUP),15000:SETUP:800,16000:REL,17000:SETUP:800"
# counter modes with a signal
emit Hz 12000 "$(seq_keys 3000 MODE MODE MODE)" --freq 1234.5 --duty 0.25
emit Ohm 14000 "$(seq_keys 2500 MODE MODE MODE NONE NONE)" --freq 50
# AC: 1 ms PEAK, 1 kHz LPF, AC+DC
emit V 12000 "2500:MODE,4100:PEAK,5700:PEAK,7300:REL:800,9000:REL:800"
emit mV 12000 "2500:MODE,4100:PEAK,5700:REL:800,7300:MODE"
emit V 12000 "2500:MODE,4100:MODE,5700:MINMAX,7300:HOLD"
# MIN/MAX cycle, auto-hold
emit V 14000 "$(seq_keys 2500 MINMAX MINMAX MINMAX MINMAX MINMAX:800 HOLD HOLD:800)"
# jacks, Fahrenheit, low battery, other inputs
emit uA 6000 "2500:MODE" --jack mA
emit A 6000 "2500:MODE" --jack A
emit V 6000 "2500:NONE" --jack A
emit mA 6000 "2500:NONE" --jack mA 2>/dev/null || true
emit mV 9000 "2500:MODE,4100:MODE" --ee 10=00
emit V 9000 "2500:SETUP" --battery-raw 1100
emit V 9000 "2500:NONE" --ad1 -20000
emit V 9000 "2500:NONE" --ad1 200000
emit Ohm 9000 "2500:NONE" --ad1 100
# SD card (scripts/sdcard_model.py; default card = blank 64 MB, no filesystem): boot cal save (MEM, REL, MODE) and
# load (MEM, PEAK, MODE) on every card kind, logging (long MEM on, long MEM off) in several modes, twice for the index
SD=out/sd
for card in "--sd $SD/fat32-mbr.img" "--sd $SD/fat16-flat.img" "--sd $SD/fat32-cal.img" "" "--no-sd"; do
    emit V 6000 "50:MEM:400,800:REL:300,1600:MODE" $card
    emit V 6000 "50:MEM:400,800:PEAK:300,1600:MODE" $card
    emit V 14000 "2500:MEM:800,11000:MEM:800" $card
done
emit V 6000 "50:MEM:400,800:PEAK:300,1600:MODE" --sd $SD/fat32-cal.img --ee 1F=00
for p in mV Hz Ohm VA uA A LowZ; do emit $p 12000 "2500:MEM:800,10000:MEM:800" --sd $SD/fat32-mbr.img; done
emit V 16000 "2500:MODE,4100:MEM:800,9000:MEM:800,10500:MEM:800,14500:MEM:800" --sd $SD/fat16-flat.img
emit V 16000 "2500:MINMAX,4100:MEM:800,6000:MINMAX,7600:REL,9200:HOLD,12000:MEM:800" --sd $SD/fat32-mbr.img
emit VA 14000 "2500:MODE,4100:MEM:800,11000:MEM:800" --sd $SD/fat32-mbr.img
emit V 14000 "2500:MEM:800" --sd $SD/fat32-mbr.img --ee 19=0A
# calibration mode (boot MODE + REL): SETUP = offset capture at 10% of the reference, MEM = gain capture at 100%
# (AC two-point fit, docs/calibration.md); every position x MODE steps 0-3 x three reference frequencies
for pos in LowZ V mV Hz Ohm VA uA A; do
    for n in 0 1 2 3; do
        modes=""; t=3500; repeat $n { modes+="$t:MODE,"; t=$((t + 1500)); }
        for fr in 1000 2000 10000; do
            emit $pos 20000 "50:MODE:300,500:REL:300,${modes}$((t + 1000)):SETUP,$((t + 5000)):MEM,$((t + 9000)):HOLD,$((t + 10500)):RANGE" --ad1 5000 --ad1-at $((t + 3500)):50000 --freq $fr
        done
    done
done
# paths found in the 2026-11-02 naming pass: auto-hold (second HOLD) with steps above/below its 999-count capture,
# ACV SETUP pages 10/11 (Hz, dBm), double long REL zero trim per current mode, an 8 GiB card with its partition past
# 4 GiB (address wrap), VA high inputs on the A jack (correction tables), Fahrenheit thermocouple
for p in V mV Ohm uA; do for v in 5000 500 -20000; do emit $p 10000 "2500:HOLD,4000:HOLD" --ad1-at 6000:$v; done; done
emit V 24000 "2500:MODE,$(seq_keys 4100 SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP)"
emit mV 24000 "2500:MODE,$(seq_keys 4100 SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP)"
for p in uA A; do for m in 0 1; do
    emit $p 12000 "$(repeat $m print -n '2500:MODE,')3500:REL:800,6000:REL:800,8500:RANGE,10000:RANGE" --jack $([[ $p == A ]] && print A || print mA)
done; done
emit V 6000 "50:MEM:400,800:REL:300,1600:MODE" --sd $SD/big8g-head.img --sd-virtual-mb 8192 --sd-place 8390656:$SD/big8g-part.img
emit V 9000 "2500:MEM:800" --sd $SD/big8g-head.img --sd-virtual-mb 8192 --sd-place 8390656:$SD/big8g-part.img
for m in 0 1 2; do for v in 52000 58000 -52000 9000; do
    emit VA 12000 "$(repeat $m print -n '2500:MODE,')5000:NONE" --jack A --ad1 $v
    emit VA 12000 "$(repeat $m print -n '2500:MODE,')5000:NONE" --jack mA --ad1 $v --pf8-low
done; done
emit mV 9000 "2500:MODE,4100:MODE" --ee 10=00 --thermistor-raw 3000
emit mV 9000 "2500:MODE,4100:MODE" --ad1 -15000 --thermistor-raw 800
# frontier after naming (2026-11-02): ACV correction on meters without the 0x1F marker; current/VA burden display on
# (SETUP edit) with BLE on; AC calibration points on upper ranges (manual RANGE first); capacitance calibration
for fr in 1000 10000 50; do emit V 9000 "2500:MODE" --ee 1F=00 --freq $fr; emit mV 9000 "2500:MODE" --ee 1F=00 --freq $fr; done
for p in uA A; do
    emit $p 30000 "$(seq_keys 2500 SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP),20500:SETUP:800,21500:REL,22500:SETUP:800,24000:PEAK:800" --ble 27000:f40101
    emit $p 30000 "2500:MODE,$(seq_keys 4100 SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP),22100:SETUP:800,23100:REL,24100:SETUP:800,25500:PEAK:800" --ble 28000:f40101
done
emit VA 30000 "$(seq_keys 2500 SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP),22000:SETUP:800,23000:REL,24000:SETUP:800,25500:PEAK:800" --ble 28000:f40101
for pos in V mV uA; do for rg in 1 2 3; do for fr in 1000 10000; do
    emit $pos 22000 "50:MODE:300,500:REL:300,3500:MODE,$(seq_keys 5000 $(repeat $rg print -n 'RANGE ')),$((5000 + 1600 * rg + 1000)):SETUP,$((5000 + 1600 * rg + 5000)):MEM" --ad1 5000 --ad1-at $((5000 + 1600 * rg + 3500)):50000 --freq $fr
done; done; done
emit Ohm 22000 "50:MODE:300,500:REL:300,3500:MODE,5000:MODE,6500:MODE,8000:SETUP,12000:MEM" --freq 200 --ad1 5000 --ad1-at 10500:50000
while read -r pos ms keys extra; do
    print -r -- "python3 -I scripts/emu_run.py EEVBlog2_05.bin svd/STM32L1xx.svd --rotary $pos --ms $ms --hy-only --eeprom factory-eeprom.bin --ad1 20000 --keys '$keys' $extra --coverage $cov --coverage-ranges $covr >/dev/null 2>&1 || true"
done < $jobs_file | xargs -P 12 -I{} zsh -c '{}'
print -r -- "runs: $(wc -l < $jobs_file); blocks: $(sort -u $cov | wc -l)"
