#!/bin/zsh
# DC VA (mode 24) VA_VoltageCorrect unit cases: stock-fixed base vs the va-dc-sign image. Prints V (and I) after the call.
emulate -L zsh
R=${0:A:h:h:h:h}; S=${1:-${TMPDIR:-/tmp}/measA}; U=$R/patches/zombu4/measA/ucall.py
for fdf in 0 1; do for rg in 0 1 2 3; do for vi in "20000 3000" "20000 -3000" "-20000 3000" "-20000 -3000"; do
  set -- ${=vi}
  args=(08008ed0 --b 2000803c=24 --b 2000803d=$rg --b 200080cc=2 --b 20007fdf=$fdf --w 20007d7c=$1 --w 20007d80=$2 --rd 20007d7c --rd 20007d80)
  a=$(python3 -I $U $S/base.bin $args); b=$(python3 -I $U $S/t-va-dc-sign.bin $args)
  print -r -- "FDF=$fdf r$rg V=$1 I=$2 | stock $a | fixed $b"
done; done; done
