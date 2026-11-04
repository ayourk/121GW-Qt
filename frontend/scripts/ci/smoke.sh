#!/bin/sh
# SPDX-License-Identifier: MIT
# scripts/ci/smoke.sh: start 121gw-qt with the demo meter, no display and no Bluetooth, and check
# that the front panel loaded and drew.
#
#   scripts/ci/smoke.sh [COMMAND ...]      default: 121gw-qt from PATH
#
# The QML is loaded at run time, so a build that compiles can still fail here: a missing QML module
# (a package's Depends) or a broken import makes the program exit 1 before it draws anything. The
# check is the program's own exit status plus a PNG of the window that is not trivially small.
# Settings and sessions go to a scratch directory, never the caller's home (on Linux and macOS;
# Windows keeps them in %APPDATA%, which on a CI runner is thrown away).
# SMOKE_PLATFORM: the Qt platform plugin (default offscreen; macOS's deployed bundle has cocoa only).
# SMOKE_KEEP: copy the screenshot there.
set -eu
[ $# -gt 0 ] || set -- 121gw-qt
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
shot=$scratch/front-panel.png
env HOME="$scratch" XDG_CONFIG_HOME="$scratch/config" XDG_DATA_HOME="$scratch/data" \
    XDG_RUNTIME_DIR="$scratch" QT_QPA_PLATFORM="${SMOKE_PLATFORM:-offscreen}" QT_QUICK_BACKEND=software \
    "$@" --demo --no-mcp --screenshot "$shot" --screenshot-delay 3000
[ -s "$shot" ] || { echo "error: no screenshot; the front panel did not draw" >&2; exit 1; }
size=$(wc -c < "$shot")
# A 1000-pixel-tall window of the meter's face compresses to far more than this; an empty
# window of one color would not.
[ "$size" -gt 20000 ] || { echo "error: screenshot is only $size bytes; the window looks empty" >&2; exit 1; }
echo "front panel drew: $size-byte screenshot"
if [ -n "${SMOKE_KEEP:-}" ]; then cp "$shot" "$SMOKE_KEEP"; fi
