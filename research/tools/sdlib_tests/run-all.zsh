#!/bin/zsh
# SPDX-License-Identifier: MIT
# Every emulator suite of docs/sd-stack-v2.md for one image, one after the other (each keeps to 6 runs at once).
#   run-all.zsh IMG OUTDIR STEP     STEP 1 = R0.08a build (FAT cards only), 2 = R0.16 build (exFAT cards too)
#   needs the YMODEM suite copies in ${TMPDIR:-/tmp}/ymodem (run-tests.zsh, verify.zsh and their files)
set -u
img=$1 out=$2 step=$3
fw=${0:A:h:h:h}; Y=${TMPDIR:-/tmp}/ymodem
mkdir -p $out
cd $out && { RES=$out/ym IMG=$img $Y/run-tests.zsh; RES=$out/ym $Y/verify.zsh } >| $out/ym.log 2>&1
cd $fw && zsh scripts/ymodem_tests/fault-tests.zsh $img $out/fault >| $out/fault.log 2>&1
zsh scripts/sdlib_tests/log-tests.zsh $img $out/log >| $out/log.log 2>&1
zsh scripts/sdlib_tests/sd-fault-tests.zsh $img $out/sdfault >| $out/sdfault.log 2>&1
cards=(fat16 fat32 fat32-8g fat32-2tb); (( step == 2 )) && cards+=(exfat-64g exfat-2tb)
zsh scripts/sdlib_tests/card-tests.zsh $img $out/cards $cards >| $out/cards.log 2>&1
print -r -- "all done"
