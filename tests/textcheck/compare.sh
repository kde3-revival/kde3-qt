#!/bin/sh
# Compare two result directories written by run.sh.
#
# usage: tests/textcheck/compare.sh [-q] dir1 dir2 [diffdir]
#
#   -q       only the counts, not every difference
#   diffdir  where to write the difference images (needs ImageMagick;
#            default: dir2/diff-<name of dir1>)
#
# Lists, per font, string and measurement, the values that differ (for
# lists of numbers the indices that differ), the FAIL/NOTE lines that are
# new or gone, and for each drawn string whose image differs the number
# of differing pixels. Exits with 0 if the reports are identical, 1 if
# not.
# the reports are sorted bytewise
LC_ALL=C
export LC_ALL

# ImageMagick 7 (magick) or 6 (convert, compare), optional
if command -v magick >/dev/null; then
    convert="magick"
    compare="magick compare"
elif command -v convert >/dev/null && command -v compare >/dev/null; then
    convert="convert"
    compare="compare"
else
    convert=
fi
pixdiff() {
    # number of pixels that differ in any channel
    $convert "$1" "$2" -compose difference -composite -alpha off \
	-threshold 0 -separate -evaluate-sequence max -format '%[fx:round(mean*w*h)]' info:
}

quiet=
if [ "$1" = -q ]; then
    quiet=1
    shift
fi
if [ $# -lt 2 ]; then
    echo "usage: compare.sh [-q] dir1 dir2 [diffdir]" >&2
    exit 2
fi
a=${1%/}
b=${2%/}
d=${3:-$b/diff-$(basename "$a")}

echo "=== env"
diff "$a/env.txt" "$b/env.txt" | grep '^[<>]'

echo "=== measurements"
# report lines: <font> <string> <metric> <value...>
awk -v quiet="$quiet" '
function key(line,   f) { split(line, f, " "); return f[1] " " f[2] " " f[3] }
function val(line,   k) { k = key(line); return substr(line, length(k) + 2) }
# the indices at which two comma separated lists differ
function listdiff(x, y,   xa, ya, nx, ny, i, out, cnt) {
    nx = split(x, xa, ","); ny = split(y, ya, ",")
    if (nx != ny)
        return ""
    out = ""; cnt = 0
    for (i = 1; i <= nx; i++)
        if (xa[i] != ya[i]) {
            if (++cnt <= 12)
                out = out " [" i - 1 "] " xa[i] "->" ya[i]
        }
    if (cnt > 12)
        out = out " ... (" cnt " differ)"
    return out
}
FNR == 1 { file++ }
/^#/ { next }
file == 1 { A[key($0)] = val($0); next }
file == 2 {
    k = key($0); v = val($0); seen[k] = 1
    split(k, kk, " "); metric = kk[3]
    if (!(k in A)) {
        added++; bym[metric]++
        if (!quiet) print "+ " k " " v
    } else if (A[k] != v) {
        changed++; bym[metric]++
        if (!quiet) {
            ld = (v ~ /^[-0-9,]+$/ && A[k] ~ /^[-0-9,]+$/) ? listdiff(A[k], v) : ""
            if (ld != "")
                print "~ " k ":" ld
            else
                print "~ " k ": " A[k] " -> " v
        }
    }
}
END {
    for (k in A)
        if (!(k in seen)) {
            removed++; split(k, kk, " "); bym[kk[3]]++
            if (!quiet) print "- " k " " A[k]
        }
    for (m in bym)
        printf "  %-24s %d differences\n", m, bym[m] | "sort"
    close("sort")
    printf "%d changed, %d new, %d gone\n", changed, added, removed
    exit (changed + added + removed) ? 1 : 0
}' "$a/report.txt" "$b/report.txt"
status=$?

echo "=== checks"
comm -13 "$a/fails.txt" "$b/fails.txt" | sed 's/^/new: /' | { if [ -n "$quiet" ]; then grep -c . | sed 's/$/ new/'; else cat; fi; }
comm -23 "$a/fails.txt" "$b/fails.txt" | sed 's/^/gone: /' | { if [ -n "$quiet" ]; then grep -c . | sed 's/$/ gone/'; else cat; fi; }
diff "$a/summary.txt" "$b/summary.txt" | grep '^[<>]'

echo "=== images"
tmp=$(mktemp -d)
for r in "$a" "$b"; do
    awk '$3 == "draw.hash" { print $1 "/" $2, $4 }' "$r/report.txt" | sort > "$tmp/$(echo "$r" | md5sum | cut -c1-8)"
done
n=0
for img in $(join "$tmp/$(echo "$a" | md5sum | cut -c1-8)" "$tmp/$(echo "$b" | md5sum | cut -c1-8)" | awk '$2 != $3 { print $1 }'); do
    n=$((n + 1))
    [ -n "$quiet" ] && continue
    if [ -f "$a/png/$img.png" ] && [ -f "$b/png/$img.png" ] && [ -n "$convert" ]; then
	mkdir -p "$d/$(dirname "$img")"
	px=$(pixdiff "$a/png/$img.png" "$b/png/$img.png")
	$compare "$a/png/$img.png" "$b/png/$img.png" "$d/$img.png" 2>/dev/null
	echo "$img: $px pixels differ ($d/$img.png)"
    else
	echo "$img: image differs"
    fi
done
rm -rf "$tmp"
echo "$n image(s) differ"
[ $n -eq 0 ] || status=1
exit $status
