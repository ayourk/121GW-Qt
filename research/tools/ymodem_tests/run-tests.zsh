#!/bin/zsh
# Emulator tests of the Bluetooth YMODEM transfer (bt_xfer.c); at most 4 runs at once. Logs and cards in $T/res/
#   run-tests.zsh [TEST...]   (default: all)
set -u
T=${0:A:h}
R=${RES:-$T/res}; mkdir -p $R
cd ${0:A:h:h:h}
img=${IMG:-$T/app.bin}
rb=${BUILDS:-${0:A:h:h:h:h}/builds}/rebuild-2.05-stock-gcc14.2.1-0210be8f.bin
base=(python3 -I scripts/emu_run.py $img svd/STM32L1xx.svd --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --nvic --hy-only
      --ble-peer scripts/ble_peer_ymodem.py --ble-peer-arg start=4.5)
f4=f430323032
f8=f83141304230333041313431453132
typeset -A card keys ops ms
card[T01_new]=out/sd/fat32-cal.img;  keys[T01_new]=2500:PEAK:800;               ms[T01_new]=200000
ops[T01_new]="info;list;badcrc;cmd:7E;cmd:12:00;put:$T/new7k.bin:NEW7K.BIN;list;get:NEW7K.BIN:$R/T01_get.bin;wait:3"
card[T02_eev_accept]=out/sd/fat32-cal.img; keys[T02_eev_accept]=2500:PEAK:800,6000:SETUP,25000:HOLD; ms[T02_eev_accept]=400000
ops[T02_eev_accept]="put:$rb:EEVBlog.bin;list;wait:3"
card[T03_eev_reject]=out/sd/fat32-cal.img; keys[T03_eev_reject]=2500:PEAK:800,6000:MODE; ms[T03_eev_reject]=100000
ops[T03_eev_reject]="put:$rb:EEVBlog.bin;list;wait:3"
card[T04_eev_timeout]=out/sd/fat32-cal.img; keys[T04_eev_timeout]=2500:PEAK:800; ms[T04_eev_timeout]=100000
ops[T04_eev_timeout]="put:$rb:EEVBlog.bin;list;wait:3"
card[T05_overwrite]=$T/card-old.img; keys[T05_overwrite]=2500:PEAK:800,6000:SETUP; ms[T05_overwrite]=100000
ops[T05_overwrite]="list;put:$T/new7k.bin:OLD.BIN;list;wait:3"
card[T06_cal]=out/sd/fat32-cal.img; keys[T06_cal]=2500:PEAK:800; ms[T06_cal]=100000
ops[T06_cal]="put:$T/new7k.bin:CAL.BIN;put:$T/new7k.bin:cal.bin;list;wait:3"
card[T07_corrupt]=out/sd/fat32-cal.img; keys[T07_corrupt]=2500:PEAK:800; ms[T07_corrupt]=100000
ops[T07_corrupt]="put:$T/new7k.bin:NEW7K.BIN;list;wait:3"
card[T08_cut]=$T/card-old.img; keys[T08_cut]=2500:PEAK:800,6000:SETUP; ms[T08_cut]=300000
ops[T08_cut]="put:$T/new7k.bin:OLD.BIN;list;wait:125;list;wait:3"
card[T09_get]=$T/card-log.img; keys[T09_get]=2500:PEAK:800; ms[T09_get]=300000
ops[T09_get]="list;get:26110200.CSV:$R/T09_log.csv;get1k:CAL.BIN:$R/T09_cal.bin;cmd:11:00004e4f4e452e424e;wait:3"
card[T10_stock]=out/sd/fat32-cal.img; keys[T10_stock]=2500:PEAK:800; ms[T10_stock]=100000
ops[T10_stock]="echo:$f4;put128:$T/small.bin:SMALL.BIN;echo:$f4;wait:1.5;echo:$f4;echo:$f8;wait:3"
card[T11_pty_put]=out/sd/fat32-cal.img; keys[T11_pty_put]=2500:PEAK:800; ms[T11_pty_put]=200000
ops[T11_pty_put]="pty-put:sz+--ymodem+-k+$T/pty/new7k.bin;list;wait:3"
card[T12_pty_get]=$T/card-log.img; keys[T12_pty_get]=2500:PEAK:800; ms[T12_pty_get]=300000
ops[T12_pty_get]="pty-get:CAL.BIN:rz+--ymodem;wait:8"
card[T13_pty_prompt]=out/sd/fat32-cal.img; keys[T13_pty_prompt]=2500:PEAK:800,32000:SETUP; ms[T13_pty_prompt]=200000
ops[T13_pty_prompt]="pty-put:sz+--ymodem+$T/pty/EEVBlog.bin;list;wait:3"
card[T14_bt_off]=out/sd/fat32-cal.img; keys[T14_bt_off]=2500:PEAK:800,9500:PEAK:800,16000:PEAK:800; ms[T14_bt_off]=200000
ops[T14_bt_off]="put:$rb:NEWBIG.BIN;wait:10;list;wait:3"
card[T15_big_get]=$T/card-big.img; keys[T15_big_get]=2500:PEAK:800; ms[T15_big_get]=2600000
ops[T15_big_get]="get:V2.BIN:$R/T15_128.bin;get1k:V2.BIN:$R/T15_1k.bin;wait:3"
card[T16_big_put]=out/sd/fat32-cal.img; keys[T16_big_put]=2500:PEAK:800; ms[T16_big_put]=2600000
ops[T16_big_put]="put128:${0:A:h:h:h}/out/patched/v2.bin:BIG.BIN;get1k:BIG.BIN:$R/T16_back.bin;list;wait:3"
extra_T02=(--screens 5000,8000,25300,30000)
extra_T03=(--screens 5000,8000)
extra_T05=(--screens 5000,8000)
tests=(${@:-${(ok)card}})
run() {
    local t=$1 x; x=(); [[ $t == T02* ]] && x=($extra_T02); [[ $t == T03* ]] && x=($extra_T03); [[ $t == T05* ]] && x=($extra_T05)
    [[ $t == T07* ]] && x=(--ble-peer-arg corrupt=2,3 --ble-peer-arg trace=1)
    [[ $t == T08* ]] && x=(--ble-peer-arg cut=3)
    [[ $t == T14* ]] && x=(--ble-peer-arg cut=8)
    [[ $t == T12* ]] && x=(--ble-peer-arg trace=1)
    [[ $t == T12* ]] && { rm -f $T/rzout/CAL.BIN $T/rzout/cal.bin; export YMODEM_PTY_CWD=$T/rzout }
    TMPDIR=$T $base --ms $ms[$t] --sd $card[$t] --sd-out $R/$t.img --keys $keys[$t] --ble-peer-arg "ops=$ops[$t]" $x >| $R/$t.log 2>&1
    print -r -- "$t done ($?)"
}
n=0
for t in $tests; do
    run $t &
    (( ++n % 4 == 0 )) && wait
done
wait
