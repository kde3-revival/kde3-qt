#!/bin/sh
# Run all event loop tests built by build.sh.
#
# usage: tests/eventloop/run.sh [builddir]
#
# The GUI checks run only if $DISPLAY is set. Exits with the number of
# failed test programs.
here=$(cd "$(dirname "$0")" && pwd)
b=${1:-$here/build}
failed=0

# Qt 3 leaks some memory on purpose (QApplication::construct, qt_init);
# LeakSanitizer would fail the sanitizer builds (-asan, -asan-loop) at exit
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}

run() {
    echo "=== $*"
    if ! "$@"; then
        failed=$((failed + 1))
    fi
}

run "$b/looptest"
run "$b/looptest" recreate
run "$b/looptest" early
run "$b/looptest" workerfirst
run "$b/looptest" workerfirst before
if [ -n "$DISPLAY" ]; then
    run "$b/looptest" gui
    run "$b/looptest" recreate gui
    run "$b/looptest" early gui
    run "$b/looptest" workerfirst gui
    run "$b/looptest" workerfirst before gui
fi
run "$b/t2"
run "$b/looptest" cpu 5

echo "=== $failed test program(s) failed"
exit $failed
