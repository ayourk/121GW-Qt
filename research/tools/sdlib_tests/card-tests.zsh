#!/bin/zsh
# SPDX-License-Identifier: MIT
# Card formats and sizes in the emulator: on each card, four runs chained (each starts from the card the previous
# one left): R1 CAL.BIN save (from meter-eeprom.bin), R2 CAL.BIN load into a factory-EEPROM meter, R3 logging
# (V, 12 s), R4 Bluetooth YMODEM upload of a 7000-byte file, then download of it and of CAL.BIN. Checks: the
# saved and loaded calibration, the CSV (identical, dates/times masked, to the one logged on the first card), the
# uploaded file on the card and both downloads byte-identical, fsck of the final card (fsck.fat -n / fsck.exfat -n),
# and for the big cards the highest card offset written.
#   card-tests.zsh IMG OUTDIR CARD...      CARD = NAME (see below); cards made by make-cards.zsh live in CARDDIR
#   env CARDDIR (default ${TMPDIR:-/tmp}/sdlib/cards)
#   NAME: fat16 fat32 (out/sd images), fat32-8g fat32-2tb (sparse card files: --sd-file), exfat-64g exfat-2tb
# Three cards at a time (one emulator run per card chain at a time).
set -u
img=${1:A}; out=${2:A}; shift 2; mkdir -p $out
fw=${0:A:h:h:h}; cd $fw
T=${0:A:h}
cards=${CARDDIR:-${TMPDIR:-/tmp}/sdlib/cards}
export MTOOLS_SKIP_CHECK=1
typeset -A src kind
src[fat16]=out/sd/fat16-flat.img;      kind[fat16]=img
src[fat32]=out/sd/fat32-mbr.img;       kind[fat32]=img
src[fat32-8g]=$cards/fat32-8g.img;     kind[fat32-8g]=file
src[fat32-2tb]=out/sd/card-2tb.img;    kind[fat32-2tb]=file
src[exfat-64g]=$cards/exfat-64g.img;   kind[exfat-64g]=file
src[exfat-2tb]=$cards/exfat-2tb.img;   kind[exfat-2tb]=file
[[ -f $out/up7k.bin ]] || python3 -I -c "import random,sys; random.seed(121); sys.stdout.buffer.write(bytes(random.randrange(256) for _ in range(7000)))" >| $out/up7k.bin
head -c 1200 meter-eeprom.bin >| $out/meter-1200.bin

emu() {   # CARD RUN IN OUT EXTRA...
    local c=$1 r=$2 in=$3 o=$4; shift 4
    local sdargs=(--sd $in --sd-out $o); [[ $kind[$c] == file ]] && sdargs=(--sd-file $in --sd-file-out $o)
    python3 -I scripts/emu_run.py $img svd/STM32L1xx.svd --hy-only --glass $sdargs "$@" >| $out/$c.$r.log 2>&1
}
chain() {   # CARD
    local c=$1 p=$out/$1
    emu $c R1 $src[$c] $p.R1.img --rotary V --ms 6000 --eeprom meter-eeprom.bin --keys 50:MEM:400,800:REL:300,1600:MODE --screens 5900
    emu $c R2 $p.R1.img $p.R2.img --rotary V --ms 6000 --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --keys 50:MEM:400,800:PEAK:300,1600:MODE \
        --screens 5900 --ee-out $p.R2.ee
    emu $c R3 $p.R1.img $p.R3.img --rotary V --ms 16000 --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --ad1 20000 --keys 2500:MEM:800,14000:MEM:800 \
        --screens 15900
    emu $c R4 $p.R3.img $p.R4.img --rotary V --ms 150000 --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --nvic --keys 2500:PEAK:800 \
        --ble-peer scripts/ble_peer_ymodem.py --ble-peer-arg start=4.5 \
        --ble-peer-arg "ops=put:$out/up7k.bin:UP7K.BIN;get:UP7K.BIN:$p.get7k.bin;get1k:CAL.BIN:$p.getcal.bin;list;wait:3"
    print -r -- "$c done"
}
n=0
for c in "$@"; do chain $c & (( ++n % 3 == 0 )) && wait; done
wait

pass() { print -r -- "PASS $1" }; fail() { print -r -- "FAIL $1" }
mask() { sed -E 's#[0-9]{4}/[0-9]{2}/[0-9]{2}#DATE#g; s#[0-9]{2}:[0-9]{2}:[0-9]{2}#TIME#g' }
vol() { python3 -I $T/fat_dir.py $1 --start }
isexfat() { [[ $1 == exfat* ]] }
names() { if isexfat $1; then python3 -I $T/exfat_tool.py ls $2 | awk '{print $1}'; else mdir -b -i $2@@$(vol $2) ::/ | sed 's#.*/##'; fi }
cat_file() { if isexfat $1; then python3 -I $T/exfat_tool.py cat $2 $3; else mtype -i $2@@$(vol $2) ::/$3; fi }
first=""
for c in "$@"; do
    p=$out/$c
    for r in R1 R2 R3 R4; do [[ -f $p.$r.img ]] || fail "$c $r: no card image"; /usr/bin/grep -Eq "Traceback|handler .* faulted" $out/$c.$r.log && fail "$c $r: emulator fault"; done
    [[ -f $p.R4.img ]] || continue
    s=$(grep SCREEN $out/$c.R1.log | tail -1 | sed 's/.*: //'); [[ $s == *5AuED* ]] && pass "$c R1 cal save: SAvEd" || fail "$c R1 cal save screen '$s'"
    cmp -s =(cat_file $c $p.R4.img CAL.BIN) $out/meter-1200.bin && pass "$c CAL.BIN = saved EEPROM 0x000-0x4AF" || fail "$c CAL.BIN content"
    s=$(grep SCREEN $out/$c.R2.log | tail -1 | sed 's/.*: //'); [[ $s == *5AuED* ]] && pass "$c R2 cal load: SAvEd" || fail "$c R2 cal load screen '$s'"
    cmp -s =(head -c 1200 $p.R2.ee) $out/meter-1200.bin && pass "$c R2 EEPROM 0x000-0x4AF = CAL.BIN" || fail "$c R2 EEPROM after load"
    csv=$(names $c $p.R4.img | grep -i '\.csv$' | head -1)
    if [[ -n $csv ]]; then
        cat_file $c $p.R4.img $csv | mask >| $p.csv.masked
        rows=$(grep -c '^[0-9]*,DATE,TIME,DCV,' $p.csv.masked)
        if [[ -z $first ]]; then first=$c; (( rows >= 8 )) && pass "$c log $csv: $rows rows" || fail "$c log $csv: $rows rows"
        else cmp -s $p.csv.masked $out/$first.csv.masked && pass "$c log $csv identical to $first's (dates/times masked, $rows rows)" \
                 || { fail "$c log $csv differs from $first's"; diff $out/$first.csv.masked $p.csv.masked | head -6 | sed 's/^/  /' }
        fi
        [[ $csv == [0-9]*.CSV ]] && pass "$c log name $csv (upper case, as stock)" || fail "$c log name '$csv'"
    else fail "$c no log file"; fi
    cmp -s =(cat_file $c $p.R4.img UP7K.BIN) $out/up7k.bin && pass "$c uploaded UP7K.BIN on card = source" || fail "$c UP7K.BIN on card"
    cmp -s $p.get7k.bin $out/up7k.bin && pass "$c download UP7K.BIN = source" || fail "$c download UP7K.BIN"
    cmp -s $p.getcal.bin $out/meter-1200.bin && pass "$c download CAL.BIN (1K blocks) = saved calibration" || fail "$c download CAL.BIN"
    /usr/bin/grep -Eq "PEER ERROR|PEER RESULT .*: FAIL" $out/$c.R4.log && fail "$c peer reported an error" || pass "$c peer ops all ok"
    names $c $p.R4.img | tr '\n' ' ' | sed "s/^/INFO $c files: /"; print
    part=$p.R4.part
    python3 -I scripts/sparse_extract.py $p.R4.img $part $(( $(vol $p.R4.img) / 512 ))
    if isexfat $c; then /usr/sbin/fsck.exfat -n $part >| $p.fsck.log 2>&1; else /usr/sbin/fsck.fat -n $part >| $p.fsck.log 2>&1; fi \
        && pass "$c fsck clean ($(tail -1 $p.fsck.log))" || { fail "$c fsck"; tail -5 $p.fsck.log | sed 's/^/  /' }
    if [[ $kind[$c] == file ]]; then
        python3 -I -c '
import os, sys
def ext(p):
    f = os.open(p, os.O_RDONLY); n = os.fstat(f).st_size; pos, out = 0, []
    while pos < n:
        try: pos = os.lseek(f, pos, os.SEEK_DATA)
        except OSError: break
        e = os.lseek(f, pos, os.SEEK_HOLE); out.append((pos, e)); pos = e
    return out, f
a, fa = ext(sys.argv[1]); b, fb = ext(sys.argv[2])
hi = 0
for s, e in b:
    for o in range(s, e, 1 << 20):
        n = min(1 << 20, e - o)
        if os.pread(fb, n, o) != os.pread(fa, n, o): hi = max(hi, o + n)
print(f"INFO highest changed MiB of the card: {hi / 2**20:.0f} MiB (card {os.fstat(fb).st_size / 2**20:.0f} MiB)")
' $src[$c] $p.R4.img | sed "s/^INFO/INFO $c/"
    fi
done
