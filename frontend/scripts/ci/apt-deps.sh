#!/bin/sh
# SPDX-License-Identifier: MIT
# scripts/ci/apt-deps.sh: install 121gw-qt's dependencies on Debian or Ubuntu from debian/control,
# the only list of them.
#
#   scripts/ci/apt-deps.sh [--runtime] [--extra PKG ...]
#
# Always: debian/control's Build-Depends (apt-get build-dep on the source tree).
# --runtime: also the qml6-module-* packages of the binary package's Depends, which the program
#            loads at run time (what a build-and-run job needs; a .deb gets them from apt).
# --extra:   any further packages a job needs (lintian, file ...).
# Runs apt through sudo when not root, so it works on a hosted runner and in a bare container.
set -eu
here=$(cd "$(dirname "$0")/../.." && pwd)
control=$here/debian/control
[ -f "$control" ] || { echo "error: $control not found" >&2; exit 2; }

runtime=0
extra=""
while [ $# -gt 0 ]; do
    case $1 in
        --runtime) runtime=1 ;;
        --extra) shift; while [ $# -gt 0 ]; do extra="$extra $1"; shift; done; break ;;
        *) echo "usage: $0 [--runtime] [--extra PKG ...]" >&2; exit 2 ;;
    esac
    shift
done

SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO=sudo
export DEBIAN_FRONTEND=noninteractive

qml=""
if [ "$runtime" -eq 1 ]; then
    # Every qml6-module-* named in a Depends field (comment lines skipped).
    qml=$(grep -v '^#' "$control" | grep -o 'qml6-module-[a-z0-9-]*' | sort -u | tr '\n' ' ')
    [ -n "$qml" ] || { echo "error: no qml6-module-* in $control's Depends" >&2; exit 1; }
fi

$SUDO apt-get update
$SUDO -E apt-get install -y --no-install-recommends build-essential
# apt-get build-dep on a directory reads its debian/control.
$SUDO -E apt-get build-dep -y "$here"
# shellcheck disable=SC2086  # word splitting of the package lists is intended
[ -z "$qml$extra" ] || $SUDO -E apt-get install -y --no-install-recommends $qml $extra
