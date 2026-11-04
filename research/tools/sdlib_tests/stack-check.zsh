#!/bin/zsh
# SPDX-License-Identifier: MIT
# Stack high-water mark of an image over the SD paths: the 4 KB stack (0x20000240-0x20001240) starts as zeros in
# the emulator and nothing clears it, so the lowest non-zero byte at the end of a run is the deepest the stack went.
# Runs: CAL.BIN save + load, logging, and a YMODEM upload + download (on CARD, default out/sd/fat32-cal.img).
#   stack-check.zsh IMG OUTDIR [CARD_ARGS...]     e.g. --sd-file /path/exfat-64g.img
set -u
img=${1:A}; out=${2:A}; shift 2; mkdir -p $out
cd ${0:A:h:h:h}
sdargs=("$@"); (( $#sdargs )) || sdargs=(--sd out/sd/fat32-cal.img)
run() {   # NAME MS EXTRA...
    local n=$1 m=$2; shift 2
    python3 -I scripts/emu_run.py $img svd/STM32L1xx.svd --rotary V --ms $m --hy-only --eeprom ${EEPROM:-factory-eeprom-btoff.bin} --ad1 20000 \
        $sdargs --peek $(( m - 1 )):0x20000240:4096 "$@" >| $out/$n.log 2>&1
    python3 -I -c '
import re, sys
t = open(sys.argv[1]).read()
m = re.search(r"PEEK t=\d+ \[20000240\] ([0-9a-f]+)", t)
b = bytes.fromhex(m.group(1)) if m else b""
i = next((k for k, v in enumerate(b) if v), None)
print(f"{sys.argv[2]}: " + ("no peek output" if not b else f"deepest stack use {4096 - i} bytes of 4096" if i is not None else "unused"))
' $out/$n.log $n
}
run cal_save 6000 --keys 50:MEM:400,800:REL:300,1600:MODE &
run cal_load 6000 --keys 50:MEM:400,800:PEAK:300,1600:MODE &
run logging 16000 --keys 2500:MEM:800,14000:MEM:800 &
run ymodem 90000 --nvic --keys 2500:PEAK:800 --ble-peer scripts/ble_peer_ymodem.py --ble-peer-arg start=4.5 \
    --ble-peer-arg "ops=put:${0:A:h:h:h}/out/sd/cal.bin:UP.BIN;get:UP.BIN:$out/up.bin;list;wait:120" &
wait
