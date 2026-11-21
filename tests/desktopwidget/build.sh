#!/bin/sh
# Build dwtest against the Qt library of this source tree (or $QTDIR).
#
# usage: tests/desktopwidget/build.sh [outdir]
set -e

here=$(cd "$(dirname "$0")" && pwd)
QTDIR=${QTDIR:-$(cd "$here/../.." && pwd)}
out=${1:-$here/build}
mkdir -p "$out"
out=$(cd "$out" && pwd)

CXX=${CXX:-g++}
"$QTDIR/bin/moc" "$here/dwtest.cpp" -o "$out/dwtest.moc"
$CXX -Wall -W -O1 -g -DQT_THREAD_SUPPORT -I"$QTDIR/include" -I"$out" \
    "$here/dwtest.cpp" -o "$out/dwtest" \
    -L"$QTDIR/lib" -Wl,-rpath,"$QTDIR/lib" -lkde3-qt
echo "built $out/dwtest"
