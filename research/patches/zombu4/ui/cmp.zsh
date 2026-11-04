#!/usr/bin/zsh
# cmp.zsh NAME: diff of the comparable lines (screens, states, buzzer, BLE, HY3131 traffic, watches) IN vs FIX
T=${TMPDIR:-/tmp}
f() { /usr/bin/grep -E '^ *(SCREEN|STATE|BUZZER|BLE|WW|PC|WATCH|HY3131|hy t=)|^end' $T/$1.$2.txt | /usr/bin/sed -E 's/ pc [0-9a-f]{8}//' ; }
diff <(f $1 in) <(f $1 fix) && echo "$1: identical"
