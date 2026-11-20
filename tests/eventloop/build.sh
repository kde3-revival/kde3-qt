#!/bin/sh
# Build the event loop tests against the Qt library of this source tree
# (or of $QTDIR if it is set).
#
# usage: tests/eventloop/build.sh [-asan | -asan-loop] [outdir]
#
#   -asan       compile the tests with -fsanitize=address,undefined
#   -asan-loop  like -asan, and also compile the library's event loop
#               sources (src/kernel/qeventloop*.cpp, as configured) with
#               the sanitizers into the test programs: they are exported
#               (-rdynamic) and take the place of the library's copies, so
#               that use-after-free in the event loop is caught without
#               building the whole library with ASan
#   outdir      where to put the binaries (default: tests/eventloop/build)
#
# The tests detect at run time which loop the library runs, so a binary
# can be run against another build of the library with LD_LIBRARY_PATH
# (not with -asan-loop, which contains the event loop of $QTDIR).
#
# The tests use GIO/GLib (pkg-config gio-2.0) and Xlib in addition to Qt.
set -e

here=$(cd "$(dirname "$0")" && pwd)
QTDIR=${QTDIR:-$(cd "$here/../.." && pwd)}

san=
loop=
case "$1" in
-asan)
    san="-fsanitize=address,undefined -fno-omit-frame-pointer"
    shift
    ;;
-asan-loop)
    san="-fsanitize=address,undefined -fno-omit-frame-pointer"
    loop=yes
    shift
    ;;
esac
out=${1:-$here/build}
mkdir -p "$out"
out=$(cd "$out" && pwd)

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

# the library's event loop objects, compiled with the library's flags
loopobjs=
if [ -n "$loop" ]; then
    src=$QTDIR/src
    # CXXFLAGS (with DEFINES) and INCPATH of the library's Makefile
    libflags=$(make -s --no-print-directory -C "$src" -f Makefile \
	--eval 'looptest-flags: ; @echo $(CXXFLAGS) $(INCPATH)' looptest-flags)
    mkdir -p "$out/loop"
    for f in qeventloop.cpp qeventloop_unix.cpp qeventloop_x11.cpp qeventloop_x11_glib.cpp; do
	case "$f" in
	qeventloop_x11_glib.cpp)
	    grep -q qeventloop_x11_glib "$src/Makefile" || continue ;;
	esac
	o="$out/loop/${f%.cpp}.o"
	(cd "$src" && $CXX -c $libflags -fPIC $san -o "$o" "kernel/$f")
	loopobjs="$loopobjs $o"
    done
    LIBS="-rdynamic $loopobjs $LIBS"
fi

"$QTDIR/bin/moc" "$here/looptest.cpp" -o "$out/looptest.moc"
$CXX $CXXFLAGS -I"$out" "$here/looptest.cpp" -o "$out/looptest" $LIBS
$CXX $CXXFLAGS "$here/t2.cpp" -o "$out/t2" $LIBS
echo "built $out/looptest $out/t2"
