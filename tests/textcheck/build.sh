#!/bin/sh
# Build textcheck against the Qt library of this source tree (or of
# $QTDIR if it is set).
#
# usage: tests/textcheck/build.sh [outdir]
#
#   outdir      where to put the binary (default: tests/textcheck/build)
#
# textcheck uses QTextLayout, QTextEngine and the rich text classes from
# the private headers (include/private), so it has to be built against
# the headers of the library it is run with (and QTDIR has to be a source
# tree: qfontengine_p.h needs src/3rdparty/opentype).
set -e

here=$(cd "$(dirname "$0")" && pwd)
QTDIR=${QTDIR:-$(cd "$here/../.." && pwd)}

out=${1:-$here/build}
mkdir -p "$out"
out=$(cd "$out" && pwd)

CXX=${CXX:-g++}
# the glyph dump includes qfontengine_p.h: Xft, FreeType and the
# OpenType headers of the source tree
XFT_CFLAGS=$(pkg-config --cflags xft fontconfig freetype2 2>/dev/null || echo -I/usr/include/freetype2)
CXXFLAGS="-Wall -W -O1 -g -DQT_THREAD_SUPPORT -I$QTDIR/include -I$QTDIR/src/3rdparty/opentype $XFT_CFLAGS"
LIBS="-L$QTDIR/lib -Wl,-rpath,$QTDIR/lib -lkde3-qt -lX11"

$CXX $CXXFLAGS "$here/textcheck.cpp" -o "$out/textcheck" $LIBS
echo "built $out/textcheck"
