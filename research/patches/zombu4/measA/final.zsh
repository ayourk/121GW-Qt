#!/bin/zsh
# Full measA image: base.bin (stock + fixed set) + every measA byte patch, then the C patches (cal.c + sf.c with
# hooks). Also base-c.bin = base.bin + cal.c only, the reference for regression runs.
emulate -L zsh; setopt err_exit
R=${0:A:h:h:h:h}; H=$R/patches/zombu4/measA; O=${1:-${TMPDIR:-/tmp}/measA}
$H/build.zsh $O >/dev/null
python3 -I $R/patches/c/build_c.py $O/base.bin $O/base-c.bin $R/patches/c/cal.c | tail -1
python3 -I $R/patches/c/build_c.py $O/all.bin $O/final.bin --hooks $H/hooks.tsv $R/patches/c/cal.c $H/sf.c | tail -1
