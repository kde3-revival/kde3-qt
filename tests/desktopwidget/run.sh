#!/bin/sh
# Run the QDesktopWidget checks on a private Xephyr server.
#
# usage: tests/desktopwidget/run.sh [builddir]
#
# "dwtest check" changes the RandR configuration of the X server it runs
# on, so it is only ever run on the Xephyr started here (display :89;
# set XEPHYR_DISPLAY to use another free one). Xephyr's window opens on
# the current $DISPLAY. Exits with the number of failed checks.
here=$(cd "$(dirname "$0")" && pwd)
b=${1:-$here/build}
D=${XEPHYR_DISPLAY:-:89}

command -v Xephyr >/dev/null || { echo "Xephyr not installed" >&2; exit 1; }
command -v xrandr >/dev/null || { echo "xrandr not installed" >&2; exit 1; }
[ -n "$DISPLAY" ] || { echo "no \$DISPLAY for the Xephyr window" >&2; exit 1; }
[ "$D" != "$DISPLAY" ] || { echo "refusing to test on \$DISPLAY" >&2; exit 1; }
n=${D#:}
if [ -e "/tmp/.X11-unix/X$n" ]; then
    echo "display $D is in use" >&2
    exit 1
fi

Xephyr "$D" -screen 1600x900 -ac -br -noreset -no-host-grab \
    -title "dwtest ($D)" >/dev/null 2>&1 &
xpid=$!
trap 'kill $xpid 2>/dev/null' EXIT INT TERM
i=0
while [ ! -e "/tmp/.X11-unix/X$n" ] && [ $i -lt 50 ]; do
    i=$((i + 1))
    sleep 0.1
done

DISPLAY=$D "$b/dwtest" print
out=$(mktemp)
DISPLAY=$D "$b/dwtest" check >"$out" 2>&1
status=$?
# xrandr complains about Xephyr's output without gamma ramps
grep -v "^xrandr: Failed to get size of gamma" "$out"
rm -f "$out"
exit $status
