#!/usr/bin/zsh
# build.zsh: IN.bin (stock + the shared byte patches) and FIX.bin (IN + UI byte patches + cal.c + ui.c) in
# ${TMPDIR:-/tmp}
set -e
cd ${0:A:h:h:h:h}
T=${TMPDIR:-/tmp}
BASEP=(zero-trim big-card temp-round fattime neg-round)
UIP=(of43-key-saturate of37-setup-end of38-contrast-boot of50-contrast-mask of49-digit-mod of23-diode-nosave of47-lobat-stop)
python3 -I patches/zombu4/ui/build_bytes.py >/dev/null
python3 -I scripts/patch_stock.py EEVBlog2_05.bin $T/IN.bin $BASEP | tail -1
python3 -I scripts/patch_stock.py --extra patches/zombu4/ui/bytes.json EEVBlog2_05.bin $T/INB.bin $BASEP $UIP | tail -1
python3 -I patches/c/build_c.py $T/INB.bin $T/FIX.bin --hooks patches/zombu4/ui/hooks.tsv patches/c/cal.c patches/zombu4/ui/ui.c
