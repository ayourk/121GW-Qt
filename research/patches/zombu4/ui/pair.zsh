#!/usr/bin/zsh
# pair.zsh NAME [emu_watch options --] emu_run options...: runs one scenario on IN.bin and FIX.bin (both in
# ${TMPDIR:-/tmp}), output in ${TMPDIR:-/tmp}/NAME.in.txt / NAME.fix.txt
set -e
cd ${0:A:h:h:h:h}
T=${TMPDIR:-/tmp}
name=$1; shift
pre=(); if (( ${@[(Ie)--]} )); then i=${@[(ie)--]}; pre=(${@[1,i-1]}); shift i; fi
for v in in fix; do
  img=$T/IN.bin; [[ $v == fix ]] && img=$T/FIX.bin
  python3 -I patches/zombu4/ui/emu_watch.py $pre -- $img svd/STM32L1xx.svd ${@//@V@/$v} >| $T/$name.$v.txt 2>&1 &
done
wait
