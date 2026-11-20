#!/bin/sh
# Build the event loop tests against the Qt library of this source tree
# (or of $QTDIR if it is set).
#
# usage: tests/eventloop/build.sh [-asan] [outdir]
#
#   -asan    compile the tests with -fsanitize=address,undefined
#   outdir   where to put the binaries (default: tests/eventloop/build)
#
# The tests use GIO/GLib (pkg-config gio-2.0) and Xlib in addition to Qt.
set -e

here=$(cd "$(dirname "$0")" && pwd)
QTDIR=${QTDIR:-$(cd "$here/../.." && pwd)}

san=
if [ "$1" = "-asan" ]; then
    san="-fsanitize=address,undefined -fno-omit-frame-pointer"
    shift
fi
out=${1:-$here/build}
mkdir -p "$out"

# libpulse's GLib main loop is optional (one check)
pkgs=gio-2.0
pulse=
if pkg-config --exists libpulse-mainloop-glib; then
    pkgs="$pkgs libpulse-mainloop-glib"
    pulse=-DHAVE_PULSE
fi

CXX=${CXX:-g++}
CXXFLAGS="-Wall -W -O1 -g $san -DQT_THREAD_SUPPORT $pulse -I$QTDIR/include $(pkg-config --cflags $pkgs)"
LIBS="-L$QTDIR/lib -Wl,-rpath,$QTDIR/lib -lkde3-qt $(pkg-config --libs $pkgs) -lX11"

"$QTDIR/bin/moc" "$here/looptest.cpp" -o "$out/looptest.moc"
$CXX $CXXFLAGS -I"$out" "$here/looptest.cpp" -o "$out/looptest" $LIBS
$CXX $CXXFLAGS "$here/t2.cpp" -o "$out/t2" $LIBS
echo "built $out/looptest $out/t2"
