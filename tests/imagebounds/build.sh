#!/bin/sh
# Build imagebounds against the Qt library of this source tree (or $QTDIR).
#
# usage: tests/imagebounds/build.sh [outdir] [extra compiler flags]
#   e.g. build.sh build-asan -fsanitize=address   (for a Qt built with ASan)
# The program finds the library through RUNPATH, so LD_LIBRARY_PATH can
# point it at another build of the same tree (a before/after comparison).
set -e

here=$(cd "$(dirname "$0")" && pwd)
QTDIR=${QTDIR:-$(cd "$here/../.." && pwd)}
out=${1:-$here/build}
[ $# -gt 0 ] && shift
mkdir -p "$out"
out=$(cd "$out" && pwd)

CXX=${CXX:-g++}
$CXX -Wall -W -O1 -g "$@" -I"$QTDIR/include" "$here/imagebounds.cpp" -o "$out/imagebounds" \
    -L"$QTDIR/lib" -Wl,--enable-new-dtags,-rpath,"$QTDIR/lib" -lkde3-qt -lz
echo "built $out/imagebounds"
