#!/bin/zsh
# Checks the results of run-tests.zsh: card contents (mtools), fsck, unchanged cards, peer results
set -u
setopt extendedglob bareglobqual
T=${0:A:h}; R=${RES:-$T/res}
export MTOOLS_SKIP_CHECK=1
fw=${0:A:h:h:h}
rb=${BUILDS:-${0:A:h:h:h:h}/builds}/rebuild-2.05-stock-gcc14.2.1-0210be8f.bin
pass() { print -r -- "PASS $1" } ; fail() { print -r -- "FAIL $1" }
chk() { local name=$1; shift; if "$@" >/dev/null 2>&1; then pass "$name"; else fail "$name"; fi }
peer() { grep -q "PEER RESULT $2" $R/$1.log }
same_file() { mtype -i $1@@1M ::/$2 | cmp -s - $3 }
nofile() { ! mdir -b -i $1@@1M ::/ | grep -qi "$2" }
fsck_ok() { local p=$R/fsck-${1:t}.img; rm -f $p; python3 -I $fw/scripts/sparse_extract.py $1 $p 2048 && /usr/sbin/fsck.fat -n $p }
nores() { ! grep -q "PEER ERROR\|PEER RESULT .*: FAIL" $R/$1.log }
for t in $R/*.img; do chk "fsck ${t:t}" fsck_ok $t; done
chk "T01 info/list/badcrc/BAD_CMD/BAD_ARG/put/get all ok" nores T01_new
chk "T01 NEW7K.BIN on card = source" same_file $R/T01_new.img NEW7K.BIN $T/new7k.bin
chk "T01 download = source" cmp $R/T01_get.bin $T/new7k.bin
chk "T02 put ok" peer T02_eev_accept "put.*EEVBlog.bin: ok"
chk "T02 EEVBLOG.BIN = 92016-byte rebuild" same_file $R/T02_eev_accept.img EEVBLOG.BIN $rb
chk "T02 prompt shown" grep -q "SCREEN t=5000.*0UEr" $R/T02_eev_accept.log
chk "T02 HOLD works mid-transfer" grep -q "STATE t=25450 after HOLD.* hold 1 " $R/T02_eev_accept.log
chk "T03 put canceled" peer T03_eev_reject "put.*FAIL canceled"
chk "T03 card byte-identical" cmp $R/T03_eev_reject.img $fw/out/sd/fat32-cal.img
chk "T04 put canceled" peer T04_eev_timeout "put.*FAIL canceled"
chk "T04 card byte-identical" cmp $R/T04_eev_timeout.img $fw/out/sd/fat32-cal.img
chk "T05 put ok" peer T05_overwrite "put.*OLD.BIN: ok"
chk "T05 OLD.BIN = new content" same_file $R/T05_overwrite.img OLD.BIN $T/new7k.bin
chk "T05 no BTRECV.TMP" nofile $R/T05_overwrite.img BTRECV
chk "T06 CAL.BIN refused" peer T06_cal "put.*:CAL.BIN: FAIL canceled"
chk "T06 cal.bin refused" peer T06_cal "put.*:cal.bin: FAIL canceled"
chk "T06 card byte-identical" cmp $R/T06_cal.img $fw/out/sd/fat32-cal.img
chk "T07 put ok" peer T07_corrupt "put.*: ok"
chk "T07 two NAKs seen" test $(grep -c "NAK, again" $R/T07_corrupt.log) -eq 2
chk "T07 file = source" same_file $R/T07_corrupt.img NEW7K.BIN $T/new7k.bin
chk "T08 cut" grep -q "link cut after 3" $R/T08_cut.log
chk "T08 OLD.BIN unchanged" same_file $R/T08_cut.img OLD.BIN $T/old.bin
chk "T08 no BTRECV.TMP" nofile $R/T08_cut.img BTRECV
chk "T09 log download = card" cmp $R/T09_log.csv =(mtype -i $T/card-log.img@@1M ::/26110200.CSV)
chk "T09 CAL.BIN download (1K) = cal.bin" cmp $R/T09_cal.bin $fw/out/sd/cal.bin
chk "T09 missing file NOT_FOUND" peer T09_get "cmd:11.*NOT_FOUND"
chk "T09 card byte-identical (reads only)" cmp $R/T09_get.img $T/card-log.img
chk "T10 F4 echoed before the transfer, F4 and F8 echoed after the quiet period" test $(grep -c "PEER RESULT echo.*ok echoed" $R/T10_stock.log) -eq 3
chk "T10 F4 sent right after the transfer is dropped (quiet period)" test "$(grep "PEER RESULT echo" $R/T10_stock.log | sed -n 2p | grep -c 'FAIL no echo')" -eq 1
chk "T10 clock set by F8 after the transfer" grep -q "RTC writes: \[([0-9]*, 2, 38)" $R/T10_stock.log
chk "T10 SMALL.BIN = source" same_file $R/T10_stock.img SMALL.BIN $T/small.bin
chk "T11 sz upload" same_file $R/T11_pty_put.img NEW7K.BIN $T/new7k.bin
chk "T12 rz download" cmp $T/rzout/(CAL.BIN|cal.bin)(N[1]) $fw/out/sd/cal.bin
chk "T13 sz EEVBlog.bin after prompt" same_file $R/T13_pty_prompt.img EEVBLOG.BIN $T/small.bin
chk "T14 no NEWBIG.BIN" nofile $R/T14_bt_off.img NEWBIG
chk "T14 no BTRECV.TMP" nofile $R/T14_bt_off.img BTRECV
chk "T14 list after BT back on" peer T14_bt_off "list: ok CAL.BIN 1200 [0-9:-]* [0-9:]*$"
chk "T15 146 KB download, 128-byte blocks (1145, wraps 4x) = source" cmp $R/T15_128.bin $fw/out/patched/v2.bin
chk "T15 146 KB download, 1 KB blocks = source" cmp $R/T15_1k.bin $fw/out/patched/v2.bin
chk "T15 card byte-identical (reads only)" cmp $R/T15_big_get.img $T/card-big.img
chk "T16 146 KB upload, 128-byte blocks (receiver wraps) = source" same_file $R/T16_big_put.img BIG.BIN $fw/out/patched/v2.bin
chk "T16 read back (1 KB) = source" cmp $R/T16_back.bin $fw/out/patched/v2.bin
chk "T16 no BTRECV.TMP" nofile $R/T16_big_put.img BTRECV
for t in $R/*.log; do
    n=$(grep "PEER F2 status packets during wait" $t | tail -1 | sed 's/.*): //; s/ at.*//')
    if [[ ${n:-0} -ge 2 ]]; then pass "${t:t:r} F2 status packets resume ($n in the last wait)"; else fail "${t:t:r} F2 resume ($n)"; fi
    m=$(grep "PEER F2 status packets during \(put\|get\|pty\)" $t | sed 's/.*): //; s/ at.*//' | paste -sd+ | bc)
    print -r -- "INFO ${t:t:r} F2 packets during transfer ops: ${m:-0}"
done
