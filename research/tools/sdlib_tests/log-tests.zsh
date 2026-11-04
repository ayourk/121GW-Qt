#!/bin/zsh
# SPDX-License-Identifier: MIT
# SD logging and CAL.BIN in the emulator, each scenario on IMG, on stock 2.05 (EEVBlog2_05.bin) and on the
# previous build PREV (default: C-2.06) with the same card: files, sizes, attributes and contents must match stock
# apart from dates and times; a content difference from stock that PREV shows as well is a difference of the C
# rebuild (measurement timing), reported as such. Every card must pass fsck.fat -n; files IMG creates must carry
# the meter's clock as their creation time (stock: 2006-02-09 22:30). At most 6 emulator runs at once.
#   log-tests.zsh IMG OUTDIR [PREV]
set -u
img=${1:A}; out=${2:A}; mkdir -p $out
[[ -z "$(ls -A $out)" ]] || { print -r -- "$out is not empty: give a fresh OUTDIR"; exit 2 }
fw=${0:A:h:h:h}; cd $fw
T=${0:A:h}
ref=$fw/EEVBlog2_05.bin
prev=${3:-${BUILDS:-${0:A:h:h:h:h}/builds}/custom-C-2.06-gcc14.2.1-eb734f50.bin}
export MTOOLS_SKIP_CHECK=1
sd=out/sd
typeset -A pos ms keys card extra
# name: dial, ms, keys, card, extra emu_run arguments
pos[L1_dcv]=V;    ms[L1_dcv]=16000;   keys[L1_dcv]=2500:MEM:800,14000:MEM:800;  card[L1_dcv]=$sd/fat32-mbr.img
pos[L2_ohm16]=Ohm; ms[L2_ohm16]=14000; keys[L2_ohm16]=2500:MEM:800,12000:MEM:800; card[L2_ohm16]=$sd/fat16-flat.img
pos[L3_acmv]=mV;  ms[L3_acmv]=16000;  keys[L3_acmv]=2500:MODE,4000:MEM:800,14000:MEM:800; card[L3_acmv]=$sd/fat32-mbr.img
extra[L3_acmv]="--freq 1000"
pos[L4_amps]=A;   ms[L4_amps]=16000;  keys[L4_amps]=2500:MEM:800,14000:MEM:800;  card[L4_amps]=$sd/fat32-mbr.img
extra[L4_amps]="--jack A --ad1 15000"
pos[L5_hz]=Hz;    ms[L5_hz]=16000;    keys[L5_hz]=2500:MEM:800,14000:MEM:800;    card[L5_hz]=$sd/fat32-mbr.img
extra[L5_hz]="--freq 1234.5 --duty 0.25"
pos[L6_va]=VA;    ms[L6_va]=16000;    keys[L6_va]=2500:MEM:800,14000:MEM:800;    card[L6_va]=$sd/fat32-mbr.img
extra[L6_va]="--jack A --va 20000:15000"
pos[L7_minmax]=V; ms[L7_minmax]=20000; keys[L7_minmax]=2500:MEM:800,5000:MINMAX,18000:MEM:800; card[L7_minmax]=$sd/fat32-cal.img
extra[L7_minmax]="--ad1-at 8000:-12345,11000:30500"
pos[L8_poweroff]=V; ms[L8_poweroff]=14000; keys[L8_poweroff]=2500:MEM:800; card[L8_poweroff]=$sd/fat32-mbr.img
extra[L8_poweroff]="--rotary-at 9300:OFF"
pos[L9_twofiles]=V; ms[L9_twofiles]=24000; keys[L9_twofiles]=2500:MEM:800,9000:MEM:800,13000:MEM:800,20000:MEM:800
card[L9_twofiles]=$sd/fat32-mbr.img
pos[C1_save]=V;   ms[C1_save]=6000;   keys[C1_save]=50:MEM:400,800:REL:300,1600:MODE; card[C1_save]=$sd/fat32-mbr.img
extra[C1_save]="--eeprom meter-eeprom.bin"
pos[C2_load]=V;   ms[C2_load]=6000;   keys[C2_load]=50:MEM:400,800:PEAK:300,1600:MODE  # card: C1_save's result
pos[C3_overwrite]=V; ms[C3_overwrite]=6000; keys[C3_overwrite]=50:MEM:400,800:REL:300,1600:MODE; card[C3_overwrite]=$sd/fat32-cal.img
extra[C3_overwrite]="--eeprom meter-eeprom.bin"
pos[C4_unset_clock]=V; ms[C4_unset_clock]=6000; keys[C4_unset_clock]=50:MEM:400,800:REL:300,1600:MODE
card[C4_unset_clock]=$sd/fat32-mbr.img; extra[C4_unset_clock]="--rtc 2=00,4=00,6=00,10=99,12=99,14=99"

emu() {   # IMAGE NAME TAG CARD
    local x=(--eeprom ${EEPROM:-factory-eeprom-btoff.bin}); [[ ${extra[$2]:-} == *--eeprom* ]] && x=()
    python3 -I scripts/emu_run.py $1 svd/STM32L1xx.svd --rotary $pos[$2] --ms $ms[$2] --hy-only --glass $x \
        --keys $keys[$2] --screens $(( ms[$2] - 100 )) --sd $4 --sd-out $out/$2.$3.img --ee-out $out/$2.$3.ee \
        ${=extra[$2]:-} >| $out/$2.$3.log 2>&1
}
pair() { emu $img $1 new $2 & emu $ref $1 ref $2 & emu $prev $1 prev $2 & wait }

names=(L1_dcv L2_ohm16 L3_acmv L4_amps L5_hz L6_va L7_minmax L8_poweroff L9_twofiles C1_save C3_overwrite C4_unset_clock)
n=0
for t in $names; do
    pair $t $card[$t] &
    (( ++n % 2 == 0 )) && wait
done
wait
# each side loads the CAL.BIN its own save wrote
emu $img C2_load new $out/C1_save.new.img & emu $ref C2_load ref $out/C1_save.ref.img &
emu $prev C2_load prev $out/C1_save.prev.img & wait

pass() { print -r -- "PASS $1" }; fail() { print -r -- "FAIL $1" }
mask() { sed -E 's#[0-9]{4}/[0-9]{2}/[0-9]{2}#DATE#g; s#[0-9]{2}:[0-9]{2}:[0-9]{2}#TIME#g' }
vol() { python3 -I $T/fat_dir.py $1 --start }
listing() { python3 -I $T/fat_dir.py $1 | awk '{print $1, $2, $3, $4, $5, $6}' }   # name attr nt size
files() { mdir -b -i $1@@$(vol $1) ::/ 2>/dev/null | sed 's#.*/##' | sort }
for t in $names C2_load; do
    a=$out/$t.new.img b=$out/$t.ref.img
    [[ -f $a && -f $b ]] || { fail "$t: card image missing"; continue }
    p=$out/fsck-$t.part
    python3 -I scripts/sparse_extract.py $a $p $(( $(vol $a) / 512 )) && /usr/sbin/fsck.fat -n $p >| $out/fsck-$t.log 2>&1 \
        && pass "$t fsck.fat clean" || fail "$t fsck.fat (see fsck-$t.log)"
    if [[ "$(listing $a)" == "$(listing $b)" ]]; then pass "$t same files, sizes, attributes as stock"
    else fail "$t directory differs from stock"; diff <(listing $b) <(listing $a) | sed 's/^/  /'; fi
    c=$out/$t.prev.img ok=1 asprev=1
    for f in $(files $a); do
        m=(cat); [[ $f == *.CSV ]] && m=(mask)
        if ! cmp -s <(mtype -i $a@@$(vol $a) ::/$f | $m) <(mtype -i $b@@$(vol $b) ::/$f | $m); then
            ok=0
            cmp -s <(mtype -i $a@@$(vol $a) ::/$f | $m) <(mtype -i $c@@$(vol $c) ::/$f | $m) || asprev=0
            diff <(mtype -i $b@@$(vol $b) ::/$f | $m) <(mtype -i $a@@$(vol $a) ::/$f | $m) | sed "s/^/  $f: /" | head -6
        fi
    done
    if (( ok )); then pass "$t contents identical to stock (CSV dates/times masked)"
    elif (( asprev )); then pass "$t contents identical to C-2.06's, which differ from stock the same way (rebuild timing, not SD)"
    else fail "$t contents differ from stock and from C-2.06"; fi
    # files the run created or rewrote: their creation time is the meter's clock, not 2006-02-09 22:30
    new=$(python3 -I $T/fat_dir.py $a | grep -v "^METER121" | grep -c "created 2006-02-09 22:30")
    want=2026-11-02; [[ $t == C4* ]] && want=2016-07-01
    made=$(python3 -I $T/fat_dir.py $a | grep -v "^METER121" | grep -c "created $want")
    if [[ $t == C2_load ]]; then :
    elif (( new == 0 && made > 0 )); then pass "$t creation times from the meter's clock ($want)"
    else fail "$t creation times ($new still 2006, $made at $want)"; fi
done
# CAL.BIN round trip: C1 saved meter-eeprom.bin's first 1200 bytes; C2 loaded them into a factory-EEPROM meter
cmp -s <(mtype -i $out/C1_save.new.img@@1048576 ::/CAL.BIN) =(head -c 1200 meter-eeprom.bin) \
    && pass "C1 CAL.BIN = EEPROM 0x000-0x4AF" || fail "C1 CAL.BIN content"
cmp -s =(head -c 1200 $out/C2_load.new.ee) =(head -c 1200 meter-eeprom.bin) \
    && pass "C2 EEPROM 0x000-0x4AF = the loaded CAL.BIN" || fail "C2 EEPROM after load"
cmp -s $out/C2_load.new.ee $out/C2_load.ref.ee && pass "C2 whole EEPROM = stock's after the same load" || fail "C2 EEPROM differs from stock"
for t in C1_save C2_load C3_overwrite C4_unset_clock; do
    s1=$(grep "SCREEN" $out/$t.new.log | tail -1 | sed 's/.*: //'); s2=$(grep "SCREEN" $out/$t.ref.log | tail -1 | sed 's/.*: //')
    [[ "$s1" == "$s2" ]] && pass "$t final screen = stock ($s1)" || fail "$t final screen '$s1' vs stock '$s2'"
done
for t in $names C2_load; do
    /usr/bin/grep -Eq "Traceback|handler .* faulted" $out/$t.new.log && fail "$t emulator fault" || :
done
python3 -I $T/fat_dir.py $out/L8_poweroff.new.img | grep CSV | sed 's/^/INFO L8 /'
python3 -I $T/fat_dir.py $out/C4_unset_clock.new.img | grep CAL | sed 's/^/INFO C4 /'
