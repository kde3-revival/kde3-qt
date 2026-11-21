#!/bin/sh
# Run textcheck and write its results to tests/textcheck/results/<label>.
#
# usage: tests/textcheck/run.sh [-b builddir] [-nopng] [-noglyphs] [label]
#
#   -b builddir  where build.sh put textcheck (default: tests/textcheck/build);
#                the binary runs with the library it was built against
#                (its rpath), or with LD_LIBRARY_PATH if that is set
#   -nopng       do not write the PNG files (hashes are still computed)
#   -noglyphs    leave the glyph lines (glyphs.*) out of the report
#   label        name of the result directory (default: the short commit
#                of the source tree, with "-dirty" if it has changes)
#
# Needs $DISPLAY. The results depend on the X server's Xft resources
# (antialias, hinting, rgba, ...), so compare runs made on the same kind
# of server, e.g. a fresh "Xephyr :86 -screen 1600x1200" (env.txt lists
# the resources). Exits with textcheck's status (1 if a check failed).
here=$(cd "$(dirname "$0")" && pwd)
b=$here/build
png=
glyphs=
while [ $# -gt 0 ]; do
    case "$1" in
    -b) b=$2; shift 2 ;;
    -nopng) png=-nopng; shift ;;
    -noglyphs) glyphs=-noglyphs; shift ;;
    *) break ;;
    esac
done
if [ -z "$DISPLAY" ]; then
    echo "run.sh: DISPLAY is not set" >&2
    exit 2
fi
top=$(cd "$here/../.." && pwd)
label=$1
if [ -z "$label" ]; then
    label=$(git -C "$top" rev-parse --short HEAD 2>/dev/null || echo current)
    if [ -n "$(git -C "$top" status --porcelain --untracked-files=no 2>/dev/null)" ]; then
	label=$label-dirty
    fi
fi
out=$here/results/$label
rm -rf "$out"
mkdir -p "$out"

# no session manager, the X server's own resources only
unset SESSION_MANAGER
"$b/textcheck" "$out" $png $glyphs
status=$?

# the fonts and the libraries the run used
{
    # relative to the source tree if it is the tree's library
    lib=$(ldd "$b/textcheck" | awk '/libkde3-qt/ { print $3 }')
    echo "lib $(realpath "$lib" | sed "s|^$top/|<tree>/|")"
    echo "fontconfig $(pkg-config --modversion fontconfig 2>/dev/null)"
    echo "freetype $(pkg-config --modversion freetype2 2>/dev/null)"
    echo "xft $(pkg-config --modversion xft 2>/dev/null)"
    echo "harfbuzz $(pkg-config --modversion harfbuzz 2>/dev/null)"
    echo "QT3_HARFBUZZ=$QT3_HARFBUZZ QT3_SUBPIXEL_POSITIONING=$QT3_SUBPIXEL_POSITIONING"
    for f in "Liberation Sans" "Noto Sans" "Liberation Serif" "Liberation Mono" "Misc Fixed Wide"; do
	echo "fc-match \"$f\" -> $(fc-match -f '%{family[0]}: %{file}' "$f")"
    done
} >> "$out/env.txt"
echo "results in $out"
exit $status
