#!/bin/zsh
# SPDX-License-Identifier: MIT
# Card faults during Bluetooth transfers (bt_xfer.c). The card error must never pass silently: a transfer either
# ends "ok" with the exact file (a firmware that NAKs and repeats a failed block), or the meter cancels it (CAN, as
# docs/ble-ymodem.md specifies for a card error) and the card has no new file. C-2.06 failed this: its disk layer
# ignored write and read errors, reported the upload or download "ok" and left a file with a hole or wrong data.
# BTRECV.TMP must not stay after an "ok" upload; after a cancel it may (the delete needs card writes, which the
# fault may still be failing), and the next upload replaces it.
#   fault-tests.zsh IMG OUTDIR
set -u
img=$1; out=$2; mkdir -p $out
cd ${0:A:h:h:h}
rb=${BUILDS:-${0:A:h:h:h:h}/builds}/rebuild-2.05-stock-gcc14.2.1-0210be8f.bin
base=(python3 -I scripts/emu_run.py $img svd/STM32L1xx.svd --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --nvic --hy-only
      --ble-peer scripts/ble_peer_ymodem.py --ble-peer-arg start=4.5 --keys 2500:PEAK:800,6000:SETUP)
typeset -A fault ops ms
fault[F1_one]="--sd-fault WRITE@200:dtimeout";          ops[F1_one]="put:$rb:EEVBlog.bin;list;wait:3"
fault[F2_three]="--sd-fault WRITE@300-302:dtimeout";    ops[F2_three]="put:$rb:EEVBlog.bin;list;wait:3"
fault[F3_dead]="--sd-fault WRITE@400+:dtimeout";        ops[F3_dead]="put:$rb:EEVBlog.bin;list;wait:3"
fault[F4_read]="--sd-fault READ@t5000+@1:dtimeout";           ops[F4_read]="get:CAL.BIN:$out/F4_cal.bin;wait:2;info;wait:3"
fault[F5_readdead]="--sd-fault READ@t5000+:dtimeout";     ops[F5_readdead]="get:CAL.BIN:$out/F5_cal.bin;wait:2;info;wait:3"
for t in F1_one F2_three F3_dead F4_read F5_readdead; do
    ( TMPDIR=$out $base --ms 600000 --sd out/sd/fat32-cal.img --sd-out $out/$t.img ${=fault[$t]} \
        --ble-peer-arg "ops=$ops[$t]" >| $out/$t.log 2>&1; print "$t done ($?)" ) &
done
wait
X=(env MTOOLS_SKIP_CHECK=1)
put_ok() { /usr/bin/grep -q "PEER RESULT put:.*: ok " $out/$1.log }
put_can() { /usr/bin/grep -q "PEER RESULT put:.*: FAIL canceled" $out/$1.log }
has() { $X /usr/bin/mdir -b -i $out/$1.img@@1M ::/ | /usr/bin/grep -qi $2 }
for t in F1_one F2_three F3_dead; do
    if put_ok $t; then
        $X /usr/bin/mcopy -o -i $out/$t.img@@1M ::/EEVBLOG.BIN $out/$t.eev 2>/dev/null
        cmp -s $out/$t.eev $rb && print "PASS $t: upload ok and EEVBLOG.BIN byte-identical" \
                               || print "FAIL $t: upload reported ok but EEVBLOG.BIN differs or is missing"
        has $t BTRECV && print "FAIL $t: temp file left after an ok upload" || print "PASS $t: no temp file"
    elif put_can $t; then
        has $t EEVBLOG && print "FAIL $t: canceled, but an EEVBLOG.BIN appeared" \
                       || print "PASS $t: canceled by the meter, no EEVBLOG.BIN written (card had none)"
        has $t BTRECV && print "INFO $t: BTRECV.TMP left (the delete after the cancel failed on the card)" \
                      || print "INFO $t: no temp file"
    else
        print "FAIL $t: upload neither ok nor canceled"
    fi
    /usr/bin/grep -c 'NAK\|retry' $out/$t.log | sed "s/^/INFO $t NAK\/retry log lines: /"
done
if /usr/bin/grep -q "PEER RESULT get:CAL.BIN:.*: ok " $out/F4_read.log; then
    head -c 1200 factory-eeprom.bin | cmp -s - $out/F4_cal.bin && print "PASS F4_read: download ok and CAL.BIN byte-identical after a read fault" \
        || print "FAIL F4_read: download reported ok but differs or is missing"
elif /usr/bin/grep -q "PEER RESULT get:CAL.BIN:.*: FAIL" $out/F4_read.log; then
    print "PASS F4_read: download canceled after a read fault (no wrong data delivered)"
else
    print "FAIL F4_read: no result"
fi
cmp -s $out/F4_read.img out/sd/fat32-cal.img && print "PASS F4_read: card unchanged (reads only)" || print "FAIL F4_read: card changed"
# INFO bytes 3..18 (FEATURES bit 1): line state retries pos done cancels read_retries
info_of() { /usr/bin/grep "PEER RESULT info:" $out/$1.log | tail -1 | sed 's/.*ok OK //; s/ .*//' }
dec() { python3 -I -c 'import struct,sys; p=bytes.fromhex(sys.argv[1]); print(*struct.unpack_from("<HBBIIHH",p,3)) if len(p)>=19 and p[2]&2 else print("none")' "$1" }
read -r l st rt pos dn can rr <<< "$(dec "$(info_of F4_read)")"
[[ $can == 0 && $rr -ge 1 ]] && print "PASS F4_read: INFO reports no cancel, $rr failed card read(s) retried" \
    || print "FAIL F4_read: INFO $(info_of F4_read) (cancels $can, read retries $rr)"
/usr/bin/grep -q "PEER RESULT get:CAL.BIN:.*: FAIL" $out/F5_readdead.log && print "PASS F5_readdead: download canceled (card reads keep failing)" \
    || print "FAIL F5_readdead: download not canceled"
read -r l st rt pos dn can rr <<< "$(dec "$(info_of F5_readdead)")"
src=${BT_SRC:-${FW_TREE:-${0:A:h:h:h:h}/121gw-firmware}/firmware/src/bt_xfer.c}   # the source IMG was built from
ctx=$(sed -n "$((l-3)),${l}p" $src 2>/dev/null | /usr/bin/grep -c load_chunk)
[[ $can == 1 && $rr == 4 && $ctx -ge 1 ]] && print "PASS F5_readdead: INFO cancel at line $l (after load_chunk), all 4 card read tries failed" \
    || print "FAIL F5_readdead: INFO $(info_of F5_readdead) (line $l, cancels $can, read retries $rr)"
cmp -s $out/F5_readdead.img out/sd/fat32-cal.img && print "PASS F5_readdead: card unchanged" || print "FAIL F5_readdead: card changed"
