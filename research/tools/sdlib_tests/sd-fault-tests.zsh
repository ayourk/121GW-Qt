#!/bin/zsh
# SPDX-License-Identifier: MIT
# SD card faults (emu_run.py --sd-fault) on the stock card paths, on IMG and on the previous build PREV (default
# C-2.06): card initialization failure, write errors (one, then persistent), read errors, data timeouts (no flag
# at all: the driver's waits must run out). Required of IMG: the run completes and the meter still answers keys
# after the fault (no hang), the error is shown where stock shows one ("-Err-" / "SdE"), the card passes fsck, a
# failed CAL.BIN load leaves the EEPROM unchanged, and a one-off write error leaves the log identical to a run
# without faults (the stock retry rewrites the row). At most 6 emulator runs at once.
#   sd-fault-tests.zsh IMG OUTDIR [PREV]
set -u
img=${1:A}; out=${2:A}; mkdir -p $out
prev=${3:-${BUILDS:-${0:A:h:h:h:h}/builds}/custom-C-2.06-gcc14.2.1-eb734f50.bin}
fw=${0:A:h:h:h}; cd $fw
T=${0:A:h}
export MTOOLS_SKIP_CHECK=1
sd=out/sd
cal_save=50:MEM:400,800:REL:300,1600:MODE,7000:MODE
cal_load=50:MEM:400,800:PEAK:300,1600:MODE,7000:MODE
logkeys=2500:MEM:800,14000:MEM:800,17000:MODE
typeset -A keys card fault ms
# MODE after the fault (7000 / 17000 ms): the meter must still react (screen changes)
keys[I1_init_cal]=$cal_save;  card[I1_init_cal]=$sd/fat32-mbr.img;  fault[I1_init_cal]="--sd-fault ACMD41:timeout"; ms[I1_init_cal]=9000
keys[I2_init_log]=$logkeys;   card[I2_init_log]=$sd/fat32-mbr.img;  fault[I2_init_log]="--sd-fault ACMD41@4+:timeout"; ms[I2_init_log]=19000
keys[W0_nofault]=$logkeys;    card[W0_nofault]=$sd/fat32-mbr.img;   fault[W0_nofault]="";                            ms[W0_nofault]=19000
keys[W1_write_once]=$logkeys; card[W1_write_once]=$sd/fat32-mbr.img; fault[W1_write_once]="--sd-fault WRITE@4:dtimeout"; ms[W1_write_once]=19000
keys[W2_write_dead]=$logkeys; card[W2_write_dead]=$sd/fat32-mbr.img; fault[W2_write_dead]="--sd-fault WRITE@6+:dcrc"; ms[W2_write_dead]=19000
keys[W3_save_dead]=$cal_save; card[W3_save_dead]=$sd/fat32-mbr.img; fault[W3_save_dead]="--sd-fault WRITE:dtimeout"; ms[W3_save_dead]=9000
keys[R1_load_read]=$cal_load; card[R1_load_read]=$sd/fat32-cal.img; fault[R1_load_read]="--sd-fault READ@5+:dcrc"; ms[R1_load_read]=9000
keys[R2_log_read]=$logkeys;   card[R2_log_read]=$sd/fat32-mbr.img;  fault[R2_log_read]="--sd-fault READ@12+:rxoverr"; ms[R2_log_read]=19000
keys[D1_load_nodata]=$cal_load; card[D1_load_nodata]=$sd/fat32-cal.img; fault[D1_load_nodata]="--sd-fault READ@5+:nodata"; ms[D1_load_nodata]=9000
keys[D2_save_nodata]=$cal_save; card[D2_save_nodata]=$sd/fat32-mbr.img; fault[D2_save_nodata]="--sd-fault WRITE:nodata"; ms[D2_save_nodata]=9000
# D3: every data write runs into the 20000-poll wait, 40 log retries: the stop takes a few seconds, MODE comes later
keys[D3_log_nodata]=2500:MEM:800,14000:MEM:800,22000:MODE; card[D3_log_nodata]=$sd/fat32-mbr.img
fault[D3_log_nodata]="--sd-fault WRITE@5+:nodata"; ms[D3_log_nodata]=24000
names=(I1_init_cal I2_init_log W0_nofault W1_write_once W2_write_dead W3_save_dead R1_load_read R2_log_read D1_load_nodata
       D2_save_nodata D3_log_nodata)
emu() {   # IMAGE NAME TAG
    local m=$ms[$2] scr=$(seq -s, 500 500 $(( ms[$2] - 100 )))
    python3 -I scripts/emu_run.py $1 svd/STM32L1xx.svd --rotary V --ms $m --hy-only --glass --eeprom ${EEPROM:-factory-eeprom-btoff.bin} \
        --ad1 20000 --keys $keys[$2] --screens $scr,$(( m - 100 )) --sd $card[$2] --sd-out $out/$2.$3.img --ee-out $out/$2.$3.ee \
        ${=fault[$2]} >| $out/$2.$3.log 2>&1
    print -r -- "$2.$3 exit $?" >>| $out/exit.txt
}
n=0
for t in $names; do
    emu $img $t new & emu $prev $t prev &
    (( ++n % 3 == 0 )) && wait
done
wait

pass() { print -r -- "PASS $1" }; fail() { print -r -- "FAIL $1" }; info() { print -r -- "INFO $1" }
mask() { sed -E 's#[0-9]{4}/[0-9]{2}/[0-9]{2}#DATE#g; s#[0-9]{2}:[0-9]{2}:[0-9]{2}#TIME#g' }
screens() { grep "SCREEN t=" $1 | sed -E 's/.*SCREEN t=([0-9]+): /\1 /' }
for t in $names; do
    a=$out/$t.new.log
    G=/usr/bin/grep
    $G -q "^$t.new exit 0" $out/exit.txt && $G -q "^end " $a && pass "$t run completed" || fail "$t run did not complete"
    $G -Eq "Traceback|handler .* faulted" $a && fail "$t emulator fault"
    err=$(screens $a | $G -Eci "err|5dE"); perr=$(screens $out/$t.prev.log | $G -Eci "err|5dE")
    k=${${keys[$t]##*,}%%:*}
    last=$(screens $a | tail -1 | cut -d' ' -f2-); before=$(screens $a | awk -v k=$k '$1 < k' | tail -1 | cut -d' ' -f2-)
    plast=$(screens $out/$t.prev.log | tail -1 | cut -d' ' -f2-)
    # alive: the last key changed the screen, or the meter ends where C-2.06 ends (the boot-time "-Err-" ignores keys)
    if [[ "$last" != "$before" ]]; then pass "$t meter answers the key after the fault ('$before' -> '$last')"
    elif [[ "$last" == "$plast" ]]; then pass "$t final screen as C-2.06 ('$last'; the key is ignored there too)"
    else fail "$t no reaction to the key at $k ms ('$last'; C-2.06 '$plast')"; fi
    info "$t error screens: $err (C-2.06: $perr); C-2.06 last screen $(screens $out/$t.prev.log | tail -1 | cut -d' ' -f2-)"
    part=$out/$t.part
    python3 -I scripts/sparse_extract.py $out/$t.new.img $part 2048; /usr/sbin/fsck.fat -n $part >| $out/$t.fsck 2>&1
    # a card that failed mid-update may leak a cluster or leave FSInfo's free count stale (FAT updates are not
    # atomic): no file is damaged by that; anything else fsck finds is a failure
    other=$($G -v "^fsck.fat\|Free cluster summary wrong\|Auto-correcting\|Reclaimed .* unused cluster\|Leaving filesystem unchanged\|files, .* clusters\|^$" $out/$t.fsck)
    if [[ -z $other ]] && ! $G -q "Free cluster summary wrong\|Reclaimed" $out/$t.fsck; then pass "$t fsck.fat clean"
    elif [[ -z $other ]]; then pass "$t fsck.fat: files intact ($($G -o "Free cluster summary wrong\|Reclaimed [0-9]* unused cluster" $out/$t.fsck | tr '\n' ' '))"
    else fail "$t fsck.fat"; tail -4 $out/$t.fsck | sed 's/^/  /'; fi
    mdir -b -i $out/$t.new.img@@1M ::/ 2>/dev/null | sed 's#.*/##' | tr '\n' ' ' | sed "s/^/INFO $t files: /"; print
done
for t in I1_init_cal R1_load_read D1_load_nodata W3_save_dead D2_save_nodata; do
    e=$(screens $out/$t.new.log | grep -ci "err"); (( e > 0 )) && pass "$t error shown (-Err-)" || fail "$t no error shown"
done
for t in R1_load_read D1_load_nodata; do
    cmp -s $out/$t.new.ee ${EEPROM:-factory-eeprom-btoff.bin} && pass "$t failed load left the EEPROM unchanged" || fail "$t EEPROM changed"
done
for t in I1_init_cal W3_save_dead D2_save_nodata; do
    mdir -b -i $out/$t.new.img@@1M ::/ 2>/dev/null | grep -qi CAL.BIN \
        && { [[ $(mtype -i $out/$t.new.img@@1M ::/CAL.BIN | wc -c) == 1200 ]] && info "$t CAL.BIN present, 1200 bytes" || fail "$t a short CAL.BIN was left" } \
        || pass "$t no CAL.BIN written"
done
c0=$(mtype -i $out/W0_nofault.new.img@@1M ::/26110200.CSV | mask); c1=$(mtype -i $out/W1_write_once.new.img@@1M ::/26110200.CSV | mask)
[[ -n $c0 && $c0 == $c1 ]] && pass "W1 one write error: log identical to the fault-free run (dates/times masked)" || fail "W1 log differs from the fault-free run"
for t in I2_init_log W2_write_dead R2_log_read D3_log_nodata; do
    e=$(screens $out/$t.new.log | /usr/bin/grep -Eci "err|5dE"); info "$t error screens shown (-Err-/SdE.nn): $e"
done
