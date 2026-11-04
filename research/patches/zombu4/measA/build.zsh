#!/bin/zsh
# Builds into OUT (default ${TMPDIR:-/tmp}/measA): base.bin = stock 2.05 + the fixed set; one image per measA
# patch (t-<name>.bin); all.bin = base + every measA patch. Run from anywhere.
emulate -L zsh; setopt err_exit
R=${0:A:h:h:h:h}; H=$R/patches/zombu4/measA; O=${1:-${TMPDIR:-/tmp}/measA}; mkdir -p $O
python3 -I $H/mk.py
BASE=(zero-trim big-card temp-round fattime neg-round)
python3 -I $R/scripts/patch_stock.py $R/EEVBlog2_05.bin $O/base.bin $BASE >/dev/null
NAMES=(${(f)"$(python3 -I -c 'import json,sys;print("\n".join(json.load(open(sys.argv[1]))))' $H/bytes.json)"})
for n in $NAMES; do python3 -I $R/scripts/patch_stock.py --extra $H/bytes.json $R/EEVBlog2_05.bin $O/t-$n.bin $BASE $n >/dev/null; done
python3 -I $R/scripts/patch_stock.py --extra $H/bytes.json $R/EEVBlog2_05.bin $O/all.bin $BASE $NAMES | tail -1
print -r -- "patches: $NAMES"
