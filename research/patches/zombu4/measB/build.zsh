#!/usr/bin/zsh
# SPDX-License-Identifier: MIT
# Builds the measB images into OUTDIR: IN.bin (stock 2.05 + the shared byte patches), BASE.bin (IN + cal.c: the
# reference for regressions), INB.bin (IN + the measB byte patches) and FIX.bin (INB + cal.c + the measB C hooks).
#   build.zsh OUTDIR
setopt err_exit no_clobber
out=$1; here=${0:A:h}; root=${here:h:h:h}
cd $root
mkdir -p $out
shared=(zero-trim big-card temp-round fattime neg-round)
mine=(cap-jump-blank log-mean adc-init-zero adc-smpr-off tc-k-270)
python3 -I $here/mk.py
python3 -I scripts/patch_stock.py EEVBlog2_05.bin $out/IN.bin $shared
python3 -I patches/c/build_c.py $out/IN.bin $out/BASE.bin patches/c/cal.c
python3 -I scripts/patch_stock.py --extra $here/bytes.json EEVBlog2_05.bin $out/INB.bin $shared $mine
python3 -I patches/c/build_c.py $out/INB.bin $out/FIX.bin --hooks $here/hooks.tsv patches/c/cal.c $here/*.c
