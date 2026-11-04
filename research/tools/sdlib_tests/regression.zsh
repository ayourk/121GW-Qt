#!/bin/zsh
# SPDX-License-Identifier: MIT
# Regression of an SD-stack build against the previous build: scripts/compare_images.py on every 10th scenario of a
# coverage job list (6 workers: one emulator run each at a time), then EEVblog's tick-synchronized whole-image
# comparison (tools/emu/compare_app.py, in Docker, as ../compare-firmware-emu.zsh runs it) on the
# source TREE (its build/firmware/app.bin and dumps/), and on the previous build's tree for reference.
#   regression.zsh IMG TREE OUTDIR [PREV_IMG [PREV_TREE [JOBS]]]
#   defaults: PREV_IMG = C-2.06 (builds/custom-C-2.06-gcc14.2.1-eb734f50.bin), PREV_TREE = the main checkout,
#             JOBS = ${TMPDIR:-/tmp}/cov-jobs.txt
set -u
img=${1:A}; tree=${2:A}; out=${3:A}; mkdir -p $out
prev=${4:-${BUILDS:-${0:A:h:h:h:h}/builds}/custom-C-2.06-gcc14.2.1-eb734f50.bin}
ptree=${5:-${FW_TREE:-${0:A:h:h:h:h}/121gw-firmware}}
jobs=${6:-${TMPDIR:-/tmp}/cov-jobs.txt}
cd ${0:A:h:h:h}
python3 -I scripts/compare_images.py $prev $img $jobs --every 10 -P 6 --show 60 >| $out/compare-images.log 2>&1
print -r -- "compare_images exit $?" >>| $out/exit.txt
harness() {   # TREE LOG
    docker run --rm --network none -v $1:/src:ro 121gw-fw-compare:noble \
        python3 -P tools/emu/compare_app.py 20000000 0 --points --ticks=40000 >| $2 2>&1
    print -r -- "harness ${2:t} exit $?" >>| $out/exit.txt
}
harness $tree $out/harness-new.log &
harness $ptree $out/harness-prev.log &
wait
