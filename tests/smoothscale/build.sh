#!/bin/sh
# Build sscheck against the Qt library of this source tree (or $QTDIR).
#
# usage: tests/smoothscale/build.sh [outdir]
set -e

here=$(cd "$(dirname "$0")" && pwd)
QTDIR=${QTDIR:-$(cd "$here/../.." && pwd)}
out=${1:-$here/build}
mkdir -p "$out"
out=$(cd "$out" && pwd)

CXX=${CXX:-g++}
$CXX -Wall -W -O2 -g -I"$QTDIR/include" "$here/sscheck.cpp" -o "$out/sscheck" \
    -L"$QTDIR/lib" -Wl,-rpath,"$QTDIR/lib" -lkde3-qt
echo "built $out/sscheck"
