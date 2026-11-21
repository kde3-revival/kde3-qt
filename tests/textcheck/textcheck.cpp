/*
 * textcheck: records how the Qt 3 text engine measures, lays out and
 * draws a corpus of strings in several scripts, fonts and sizes, and
 * checks a few invariants. The output is meant to be diffed between two
 * builds of the library (see README and compare.sh).
 *
 * usage: textcheck outdir [-nopng] [-noglyphs]
 *
 * Writes outdir/report.txt (one line per measurement, sorted),
 * outdir/fails.txt (FAIL lines), outdir/summary.txt (counts per check
 * and script), outdir/env.txt (fonts actually used, DPI, versions) and
 * outdir/png/<font>/<string>.png (the drawn strings).
 *
 * -noglyphs leaves out the glyph lines (glyphs.*) of the report.
 */

#include <qapplication.h>
#include <qfont.h>
#include <qfontmetrics.h>
#include <qfontinfo.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qtextedit.h>
#include <qdir.h>
#include <qfile.h>
#include <qpaintdevice.h>
#include <private/qtextlayout_p.h>
#include <private/qrichtext_p.h>
#include <private/qtextengine_p.h>
#include <private/qfontengine_p.h>

#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>

// ---------------------------------------------------------------------
// corpus

enum {
    F_SIMPLE  = 0x001,	// LTR, one char = one glyph, no marks: widths add up
    F_MARKS   = 0x002,	// combining marks
    F_RTL     = 0x004,	// right-to-left text only
    F_BIDI    = 0x008,	// mixed directions
    F_COMPLEX = 0x010,	// contextual shaping (Arabic, Indic, Thai, ...)
    F_CTRL    = 0x020,	// tabs, control or format characters
    F_NONBMP  = 0x040,	// surrogate pairs
    F_SPACE   = 0x080,	// whitespace (or nothing) only
    F_NL      = 0x100	// contains a newline
};

struct Sample {
    const char *id;
    const char *script;
    int flags;
    const char *utf8;
};

static const Sample corpus[] = {
    { "latin",        "Latin",      F_SIMPLE, "The quick brown fox jumps over the lazy dog." },
    { "latin-kern",   "Latin",      F_SIMPLE, "AVAWAY To Ta Ty LT Wo fi fl ffi ffl" },
    { "latin-digits", "Latin",      F_SIMPLE, "0123456789 1,234.56 (x+y)=[z]" },
    { "latin-ro",     "Latin",      F_SIMPLE, "\xc8\x9a\x61r\xc4\x83 \xc8\x99i \xc8\x9b\x61r\xc3\xa2 \xc3\x8en\xc8\x9b" "elegere \xc5\x82\xc3\xb3" "d\xc5\xba \xc5\x91\xc5\xb1" },
    { "latin-comb",   "Latin",      F_MARKS,  "cafe\xcc\x81 cafe\xcc\x81 a\xcc\x88 n\xcc\x83 o\xcc\x82\xcc\xa3 q\xcc\x87\xcc\xa3 x\xcd\x9c" "y" },
    { "latin-precomp","Latin",      F_SIMPLE, "caf\xc3\xa9 \xc3\xa4 \xc3\xb1 \xe1\xbb\x99" },
    { "greek",        "Greek",      F_SIMPLE, "\xce\x9e\xce\xb5\xcf\x83\xce\xba\xce\xb5\xcf\x80\xce\xac\xce\xb6\xcf\x89 \xcf\x84\xce\xb7\xce\xbd \xcf\x88\xcf\x85\xcf\x87\xce\xbf\xcf\x86\xce\xb8\xcf\x8c\xcf\x81\xce\xb1" },
    { "greek-comb",   "Greek",      F_MARKS,  "\xce\xb1\xcc\x81\xce\xb5\xcc\x88 \xe1\xbc\x80\xcd\x85" },
    { "cyrillic",     "Cyrillic",   F_SIMPLE, "\xd0\xa1\xd1\x8a\xd0\xb5\xd1\x88\xd1\x8c \xd0\xb6\xd0\xb5 \xd0\xb5\xd1\x89\xd1\x91 \xd1\x8d\xd1\x82\xd0\xb8\xd1\x85 \xd0\xbc\xd1\x8f\xd0\xb3\xd0\xba\xd0\xb8\xd1\x85 \xd0\xb1\xd1\x83\xd0\xbb\xd0\xbe\xd0\xba" },
    { "arabic",       "Arabic",     F_RTL|F_COMPLEX, "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 \xd8\xa8\xd8\xa7\xd9\x84\xd8\xb9\xd8\xa7\xd9\x84\xd9\x85" },
    { "arabic-harakat","Arabic",    F_RTL|F_COMPLEX|F_MARKS, "\xd8\xa8\xd9\x90\xd8\xb3\xd9\x92\xd9\x85\xd9\x90 \xd9\xb1\xd9\x84\xd9\x84\xd9\x91\xd9\x8e\xd9\xb0\xd9\x87\xd9\x90" },
    { "arabic-lamalef","Arabic",    F_RTL|F_COMPLEX, "\xd9\x84\xd8\xa7 \xd8\xb3\xd9\x84\xd8\xa7\xd9\x85 \xd9\x84\xd8\xa3" },
    { "bidi-ar-num",  "Arabic",     F_RTL|F_BIDI|F_COMPLEX, "\xd8\xa7\xd9\x84\xd8\xb9\xd8\xaf\xd8\xaf 12345 \xd9\x87\xd9\x86\xd8\xa7" },
    { "bidi-en-ar",   "Arabic",     F_BIDI|F_COMPLEX, "abc \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 123 def" },
    { "hebrew",       "Hebrew",     F_RTL|F_MARKS, "\xd7\xa9\xd7\x81\xd6\xb8\xd7\x9c\xd7\x95\xd6\xb9\xd7\x9d \xd7\xa2\xd7\x95\xd6\xb9\xd7\x9c\xd6\xb8\xd7\x9d" },
    { "hebrew-plain", "Hebrew",     F_RTL,    "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d \xd7\xa2\xd7\x95\xd7\x9c\xd7\x9d" },
    { "bidi-en-he",   "Hebrew",     F_BIDI,   "Hello \xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d world (1)" },
    { "devanagari",   "Devanagari", F_COMPLEX|F_MARKS, "\xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87 \xe0\xa4\xa6\xe0\xa5\x81\xe0\xa4\xa8\xe0\xa4\xbf\xe0\xa4\xaf\xe0\xa4\xbe \xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7\xe0\xa4\xa4\xe0\xa5\x8d\xe0\xa4\xb0\xe0\xa4\xbf\xe0\xa4\xaf" },
    { "thai",         "Thai",       F_COMPLEX|F_MARKS, "\xe0\xb8\xaa\xe0\xb8\xa7\xe0\xb8\xb1\xe0\xb8\xaa\xe0\xb8\x94\xe0\xb8\xb5\xe0\xb8\x8a\xe0\xb8\xb2\xe0\xb8\xa7\xe0\xb9\x82\xe0\xb8\xa5\xe0\xb8\x81 \xe0\xb8\xa0\xe0\xb8\xb2\xe0\xb8\xa9\xe0\xb8\xb2\xe0\xb9\x84\xe0\xb8\x97\xe0\xb8\xa2 \xe0\xb8\x81\xe0\xb8\xb3\xe0\xb9\x89" },
    { "cjk",          "Han",        F_SIMPLE, "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c\xef\xbc\x8c\xe4\xb8\xad\xe6\x96\x87\xe6\xb5\x8b\xe8\xaf\x95\xe3\x80\x82" },
    { "japanese",     "Han",        F_SIMPLE, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe3\x83\x86\xe3\x82\xad\xe3\x82\xb9\xe3\x83\x88" },
    { "cjk-mixed",    "Han",        F_SIMPLE, "Qt3 \xe6\x96\x87\xe5\xad\x97 abc" },
    { "hangul",       "Hangul",     F_SIMPLE, "\xec\x95\x88\xeb\x85\x95\xed\x95\x98\xec\x84\xb8\xec\x9a\x94 \xec\x84\xb8\xea\xb3\x84" },
    { "hangul-jamo",  "Hangul",     F_COMPLEX, "\xe1\x84\x80\xe1\x85\xa1\xe1\x86\xa8 \xe1\x84\x92\xe1\x85\xa1\xe1\x86\xab" },
    { "emoji",        "Emoji",      F_NONBMP, "ok \xf0\x9f\x98\x80 \xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd \xe2\x9d\xa4\xef\xb8\x8f \xf0\x9f\x87\xb7\xf0\x9f\x87\xb4" },
    { "math-alnum",   "Symbols",    F_NONBMP, "\xf0\x9d\x94\xb8\xf0\x9d\x95\x93\xf0\x9d\x92\xb8 x" },
    { "symbols",      "Symbols",    F_SIMPLE, "\xe2\x86\x92\xe2\x86\x90\xe2\x86\x94 \xe2\x88\x91\xe2\x88\xab\xe2\x88\x9a\xe2\x89\xa0\xe2\x89\xa4\xe2\x89\xa5 \xe2\x82\xac\xc2\xa3\xc2\xa5 \xc2\xa9\xc2\xae\xe2\x84\xa2 \xe2\x80\xa6\xe2\x80\xa2\xe2\x80\xb0" },
    { "box",          "Symbols",    F_SIMPLE, "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\xac\xe2\x94\x80\xe2\x94\x90\xe2\x94\x82\xe2\x94\x9c\xe2\x94\xbc\xe2\x94\xa4\xe2\x95\x94\xe2\x95\x90\xe2\x95\x97\xe2\x96\x91\xe2\x96\x92\xe2\x96\x93\xe2\x96\x88" },
    { "tabs",         "Latin",      F_CTRL,   "a\tbb\tccc\td" },
    { "controls",     "Latin",      F_CTRL,   "a\x01" "b\x7f" "c\xc2\xad" "d\xe2\x80\x8b" "e\xe2\x80\x8d" "f\xe2\x80\x8c" "g\xef\xbb\xbf" "h" },
    { "bidi-marks",   "Hebrew",     F_BIDI|F_CTRL, "a\xe2\x80\x8f" "b \xe2\x80\xab\xd7\x90\xd7\x91 c\xe2\x80\xac d\xe2\x80\x8e" },
    { "spaces",       "Latin",      F_SPACE,  "   \xc2\xa0\xe2\x80\x83" },
    { "empty",        "Latin",      F_SPACE,  "" },
    { "newline",      "Latin",      F_SIMPLE|F_NL|F_CTRL, "line1\nline2" },
};
static const int ncorpus = sizeof(corpus) / sizeof(corpus[0]);

// ---------------------------------------------------------------------
// fonts

struct FontSpec {
    const char *label;
    const char *family;
    int weight;
    bool italic;
    int size;		// points, or pixels if pixel is set
    bool pixel;
};

static std::vector<FontSpec> fontSpecs()
{
    struct Fam { const char *label; const char *family; };
    // families checked with fc-list on the machine the baseline was made
    // on; env.txt records what each request resolved to
    static const Fam fams[] = {
	{ "sans",  "Liberation Sans" },
	{ "noto",  "Noto Sans" },
	{ "serif", "Liberation Serif" },
	{ "mono",  "Liberation Mono" },
	{ "cjk",   "Misc Fixed Wide" },
    };
    static const int pts[] = { 8, 9, 10, 12, 16 };
    static const int pxs[] = { 11, 13, 20 };
    std::vector<FontSpec> v;
    for ( unsigned f = 0; f < sizeof(fams) / sizeof(fams[0]); ++f ) {
	for ( unsigned i = 0; i < sizeof(pts) / sizeof(pts[0]); ++i ) {
	    FontSpec s = { fams[f].label, fams[f].family, QFont::Normal, false, pts[i], false };
	    v.push_back( s );
	}
	for ( unsigned i = 0; i < sizeof(pxs) / sizeof(pxs[0]); ++i ) {
	    FontSpec s = { fams[f].label, fams[f].family, QFont::Normal, false, pxs[i], true };
	    v.push_back( s );
	}
    }
    FontSpec b = { "sansB", "Liberation Sans", QFont::Bold, false, 10, false };
    v.push_back( b );
    FontSpec it = { "serifI", "Liberation Serif", QFont::Normal, true, 10, false };
    v.push_back( it );
    return v;
}

static std::string fontKey( const FontSpec &s )
{
    char buf[64];
    snprintf( buf, sizeof(buf), "%s-%d%s", s.label, s.size, s.pixel ? "px" : "pt" );
    return buf;
}

static QFont makeFont( const FontSpec &s )
{
    QFont f( s.family );
    if ( s.pixel )
	f.setPixelSize( s.size );
    else
	f.setPointSize( s.size );
    f.setWeight( s.weight );
    f.setItalic( s.italic );
    return f;
}

// ---------------------------------------------------------------------
// output helpers

static std::string fmt( const char *f, ... )
{
    char buf[1024];
    va_list ap;
    va_start( ap, f );
    vsnprintf( buf, sizeof(buf), f, ap );
    va_end( ap );
    return buf;
}

static std::string ints( const std::vector<int> &v )
{
    std::string s;
    for ( unsigned i = 0; i < v.size(); ++i ) {
	if ( i )
	    s += ',';
	if ( v[i] == INT_MIN )
	    s += '-';
	else
	    s += fmt( "%d", v[i] );
    }
    return s.empty() ? std::string( "." ) : s;
}

static std::string rectStr( const QRect &r )
{
    if ( r.isNull() )
	return "null";
    return fmt( "%d,%d,%dx%d", r.x(), r.y(), r.width(), r.height() );
}

// printable form of a string: ASCII as is, everything else \uXXXX
static std::string escaped( const QString &s )
{
    std::string r;
    for ( unsigned i = 0; i < s.length(); ++i ) {
	ushort u = s[i].unicode();
	if ( u >= 0x20 && u < 0x7f && u != '\\' )
	    r += (char)u;
	else
	    r += fmt( "\\u%04x", u );
    }
    return r;
}

static std::vector<std::string> report;	// "font string metric value"
static std::vector<std::string> fails;
static std::vector<std::string> notes;

struct Count { int checked; int failed; };
// check -> script -> counts
static std::map<std::string, std::map<std::string, Count> > counts;
// script -> tofu numbers
struct Tofu { int clusters; int missing; int box; int blank; };
static std::map<std::string, Tofu> tofu;

static void put( const std::string &fk, const char *sid, const char *metric, const std::string &value )
{
    report.push_back( fk + " " + sid + " " + metric + " " + value );
}

/*
  Records the result of one check. Asserted checks that fail give FAIL
  lines; the others (properties that Qt 3 does not promise for that kind
  of text) give NOTE lines, so that summary.txt shows how far they hold.
*/
static void check( bool ok, bool asserted, const char *name, const std::string &fk,
		   const Sample &smp, const std::string &detail )
{
    Count &c = counts[ std::string( name ) + ( asserted ? "" : " (not asserted)" ) ][ smp.script ];
    c.checked++;
    if ( ok )
	return;
    c.failed++;
    std::string line = std::string( asserted ? "FAIL " : "NOTE " ) + name + " " + fk + " " + smp.id + " " + detail;
    if ( asserted )
	fails.push_back( line );
    else
	notes.push_back( line );
}

// ---------------------------------------------------------------------
// drawing

static const int white = 0xffffff;

struct Ink {
    QRect box;		// relative to the origin the text was drawn at
    int pixels;		// non-white pixels
    Q_UINT64 hash;
    int w, h;
    QImage crop;	// the inked pixels (box), if any
    bool clipped;	// ink at the border of the pixmap
};

static Ink drawString( const QFont &f, const QString &s, const QString &png )
{
    QFontMetrics fm( f );
    // room for overhangs, and for text drawn wider than width() says
    // (ink-clipped checks that it was enough)
    int pad = fm.height() + 4;
    int w = QMAX( QMAX( fm.width( s ), fm.size( 0, s ).width() ), 1 ) * 3 / 2 + 2 * pad;
    int h = fm.height() + 2 * pad;
    QPixmap pm( w, h );
    pm.fill( Qt::white );
    QPainter p( &pm );
    p.setFont( f );
    p.setPen( Qt::black );
    int ox = pad, oy = pad + fm.ascent();
    p.drawText( ox, oy, s );
    p.end();
    QImage img = pm.convertToImage().convertDepth( 32 );

    Ink ink;
    ink.w = w;
    ink.h = h;
    ink.pixels = 0;
    Q_UINT64 hash = 14695981039346656037ULL;	// FNV-1a over w, h, RGB
    hash = ( hash ^ (Q_UINT64)w ) * 1099511628211ULL;
    hash = ( hash ^ (Q_UINT64)h ) * 1099511628211ULL;
    int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
    for ( int y = 0; y < h; ++y ) {
	const QRgb *line = (const QRgb *)img.scanLine( y );
	for ( int x = 0; x < w; ++x ) {
	    int rgb = line[x] & 0xffffff;
	    for ( int b = 0; b < 3; ++b )
		hash = ( hash ^ (Q_UINT64)( ( rgb >> ( 8 * b ) ) & 0xff ) ) * 1099511628211ULL;
	    if ( rgb != white ) {
		ink.pixels++;
		x0 = QMIN( x0, x );
		x1 = QMAX( x1, x );
		y0 = QMIN( y0, y );
		y1 = QMAX( y1, y );
	    }
	}
    }
    ink.hash = hash;
    ink.clipped = ink.pixels && ( x0 == 0 || y0 == 0 || x1 == w - 1 || y1 == h - 1 );
    if ( ink.pixels ) {
	ink.box = QRect( QPoint( x0 - ox, y0 - oy ), QPoint( x1 - ox, y1 - oy ) );
	ink.crop = img.copy( x0, y0, x1 - x0 + 1, y1 - y0 + 1 );
    }
    if ( !png.isEmpty() )
	img.save( png, "PNG" );
    return ink;
}

/*
  The images of the "missing glyph" of the current font: unassigned code
  points of several scripts (so that the engines of several fonts are
  asked) and lone surrogates. A cluster whose image contains one of them
  is counted as a box (tofu).
*/
static std::vector<QImage> boxRefs;

static void makeBoxRefs( const QFont &f, const std::string &fk )
{
    static const ushort missing[] = {
	0x0378, 0x05ff, 0x070e, 0x0e3b, 0x0fff, 0x2b74, 0x3130, 0xfa6e, 0xfffe, 0xd83d, 0xdc00
    };
    boxRefs.clear();
    std::vector<Q_UINT64> seen;
    std::string refs;
    for ( unsigned i = 0; i < sizeof(missing) / sizeof(missing[0]); ++i ) {
	Ink ink = drawString( f, QString( QChar( missing[i] ) ), QString::null );
	refs += fmt( "%s%04x:", i ? " " : "", missing[i] );
	if ( !ink.pixels ) {
	    refs += "blank";
	    continue;
	}
	// a hash of the cropped image only (drawString hashes the whole pixmap)
	Q_UINT64 h = 14695981039346656037ULL;
	for ( int y = 0; y < ink.crop.height(); ++y )
	    for ( int x = 0; x < ink.crop.width(); ++x )
		h = ( h ^ (Q_UINT64)( ink.crop.pixel( x, y ) & 0xffffff ) ) * 1099511628211ULL;
	refs += fmt( "%dx%d", ink.crop.width(), ink.crop.height() );
	if ( std::find( seen.begin(), seen.end(), h ) != seen.end() )
	    continue;
	seen.push_back( h );
	boxRefs.push_back( ink.crop );
    }
    put( fk, "-", "boxes", refs );
}

static bool hasBox( const QImage &img )
{
    for ( unsigned r = 0; r < boxRefs.size(); ++r ) {
	const QImage &ref = boxRefs[r];
	int w = ref.width(), h = ref.height();
	for ( int y0 = 0; y0 + h <= img.height(); ++y0 ) {
	    for ( int x0 = 0; x0 + w <= img.width(); ++x0 ) {
		bool eq = TRUE;
		for ( int y = 0; y < h && eq; ++y ) {
		    const QRgb *a = (const QRgb *)img.scanLine( y0 + y ) + x0;
		    const QRgb *b = (const QRgb *)ref.scanLine( y );
		    for ( int x = 0; x < w; ++x ) {
			if ( ( a[x] & 0xffffff ) != ( b[x] & 0xffffff ) ) {
			    eq = FALSE;
			    break;
			}
		    }
		}
		if ( eq )
		    return TRUE;
	    }
	}
    }
    return FALSE;
}

/*
  Checks that x-to-cursor(cursor-to-x(i)) gives back i, or at least a
  position at the same x (zero width characters, ligatures without caret
  positions inside: several positions share an x, and x-to-cursor can
  only return one of them).
*/
static inline bool isHigh( QChar c ) { return c.unicode() >= 0xd800 && c.unicode() < 0xdc00; }
static inline bool isLow( QChar c ) { return c.unicode() >= 0xdc00 && c.unicode() < 0xe000; }

// no valid cursor position between the two halves of a surrogate pair,
// and one in front of every pair
static void surrogateStops( const char *who, const std::string &fk, const Sample &smp,
			    const QString &s, const std::vector<int> &valid )
{
    if ( !( smp.flags & F_NONBMP ) )
	return;
    std::string inside, before;
    for ( int i = 1; i < (int)s.length() && i < (int)valid.size(); ++i ) {
	if ( isHigh( s[i - 1] ) && isLow( s[i] ) && valid[i] )
	    inside += fmt( " %d", i );
	if ( isHigh( s[i] ) && i + 1 < (int)s.length() && isLow( s[i + 1] ) && !valid[i] )
	    before += fmt( " %d", i );
    }
    check( inside.empty(), true, ( std::string( who ) + "-surrogate-inside" ).c_str(), fk, smp,
	   "valid cursor positions inside a pair:" + inside );
    // a modifier or regional indicator may continue a cluster
    check( before.empty(), false, ( std::string( who ) + "-surrogate-before" ).c_str(), fk, smp,
	   "no cursor position in front of a pair:" + before );
}

static void roundTrip( const char *who, const std::string &fk, const Sample &smp,
		       const std::vector<int> &valid, const std::vector<int> &cx,
		       const std::vector<int> &back, int n )
{
    std::string bad, same;
    for ( int i = 0; i <= n; ++i ) {
	if ( !valid[i] || back[i] == i )
	    continue;
	int b = back[i];
	if ( b >= 0 && b <= n && valid[b] && cx[b] == cx[i] )
	    same += fmt( " %d->x%d->%d", i, cx[i], b );
	else
	    bad += fmt( " %d->x%d->%d", i, cx[i], b );
    }
    check( bad.empty(), true, ( std::string( who ) + "-roundtrip" ).c_str(), fk, smp, bad );
    check( same.empty(), false, ( std::string( who ) + "-roundtrip-exact" ).c_str(), fk, smp, same );
}

// ---------------------------------------------------------------------
// QLineEdit's use of QTextLayout (QLineEditPrivate::updateTextLayout(),
// cursorRect() and xToPos(), without the frame and scroll offsets)

struct LineEditModel {
    QString text;
    QTextLayout layout;

    LineEditModel( const QString &s, const QFont &f ) : text( s )
    {
	QString str( s.unicode(), s.length() );
	for ( unsigned i = 0; i < str.length(); ++i )
	    if ( !str[i].isPrint() )
		str[i] = QChar( 0x0020 );
	layout.setText( str, f );
	layout.setDirection( QChar::DirON );
	layout.beginLayout( QTextLayout::SingleLine );
	layout.beginLine( INT_MAX );
	while ( !layout.atEnd() )
	    layout.addCurrentItem();
	int ascent = 0;
	layout.endLine( 0, 0, Qt::AlignLeft | Qt::SingleLine, &ascent );
    }
    /*
      QTextLayout::validCursorPosition( length ) reads the character
      attributes one past the end of the array (the result changes from
      run to run); the end of the text is always a valid position.
    */
    bool valid( int pos ) const
    {
	return pos == (int)text.length() || layout.validCursorPosition( pos );
    }
    int cursorToX( int cursor ) const
    {
	int cix = 0;
	QTextItem ci = layout.findItem( cursor );
	if ( ci.isValid() ) {
	    if ( cursor != (int)text.length() && cursor == ci.from() + ci.length()
		 && ci.isRightToLeft() != text.isRightToLeft() )
		ci = layout.findItem( cursor + 1 );
	    cix += ci.x() + ci.cursorToX( cursor - ci.from() );
	}
	return cix;
    }
    int xToPos( int x ) const
    {
	for ( int i = 0; i < layout.numItems(); ++i ) {
	    QTextItem ti = layout.itemAt( i );
	    QRect tir = ti.rect();
	    if ( x >= tir.left() && x <= tir.right() )
		return ti.xToCursor( x - tir.x(), QTextItem::BetweenCharacters ) + ti.from();
	}
	return x < 0 ? 0 : text.length();
    }
};

/*
  Access to two private members, QFont::d and QTextLayout::d, for the
  glyph dump: a pointer to a private member may be named in an explicit
  template instantiation, which stores it in Member<Tag>::ptr.
*/
template <typename Tag> struct Member { static typename Tag::type ptr; };
template <typename Tag> typename Tag::type Member<Tag>::ptr;
template <typename Tag, typename Tag::type M> struct StoreMember {
    StoreMember() { Member<Tag>::ptr = M; }
    static StoreMember instance;
};
template <typename Tag, typename Tag::type M> StoreMember<Tag, M> StoreMember<Tag, M>::instance;

struct FontD { typedef QFontPrivate *QFont::*type; };
template struct StoreMember<FontD, &QFont::d>;
struct LayoutD { typedef QTextEngine *QTextLayout::*type; };
template struct StoreMember<LayoutD, &QTextLayout::d>;

static QFontPrivate *fontPrivate( const QFont &f ) { return f.*Member<FontD>::ptr; }
static QTextEngine *layoutEngine( const QTextLayout &l ) { return l.*Member<LayoutD>::ptr; }

// QTextEdit with access to its cursor
class TextEdit : public QTextEdit
{
public:
    TextEdit() : QTextEdit( 0, "textcheck" ) {}
    QTextCursor *cursor() const { return textCursor(); }
};

// ---------------------------------------------------------------------

/*
  The glyphs of item i of a shaped QTextEngine:
  "<from>+<length> s<script> l<bidi level>[,space][,tab][,pos] font <font
  engine> | g <glyph ids> | a <advances> | o <x/y offsets> | c <log
  clusters, one per character> | t <glyph attributes>", the attributes of
  a glyph being c (cluster start), m (mark), z (zero width) and the
  combining class if not 0, or "-" for none.
*/
static std::string glyphItem( QTextEngine *e, int i )
{
    const QScriptItem &si = e->items[i];
    const int len = e->length( i );
    std::string r = fmt( "%d+%d s%d l%d%s%s%s", si.position, len, (int)si.analysis.script,
			 (int)si.analysis.bidiLevel, si.isSpace ? ",space" : "", si.isTab ? ",tab" : "",
			 si.hasPositioning ? ",pos" : "" );
    if ( si.fontEngine ) {
	QString family = si.fontEngine->fontDef.family;
	family.replace( ' ', '_' );
	r += fmt( " font %s:%s:%dpx", si.fontEngine->name() ? si.fontEngine->name() : "?",
		  family.isEmpty() ? "?" : (const char *)family.utf8(), (int)si.fontEngine->fontDef.pixelSize );
    } else {
	r += " font -";
    }
    const glyph_t *g = e->glyphs( &si );
    const advance_t *a = e->advances( &si );
    const qoffset_t *o = e->offsets( &si );
    const GlyphAttributes *ga = e->glyphAttributes( &si );
    const unsigned short *lc = e->logClusters( &si );
    std::string gs, as, os, cs, ts;
    for ( int k = 0; k < si.num_glyphs; ++k ) {
	const char *sep = k ? "," : "";
	gs += fmt( "%s%u", sep, (unsigned)g[k] );
	as += fmt( "%s%d", sep, (int)a[k] );
	os += fmt( "%s%d/%d", sep, (int)o[k].x, (int)o[k].y );
	std::string t;
	if ( ga[k].clusterStart )
	    t += 'c';
	if ( ga[k].mark )
	    t += 'm';
	if ( ga[k].zeroWidth )
	    t += 'z';
	if ( ga[k].combiningClass )
	    t += fmt( "%d", (int)ga[k].combiningClass );
	ts += sep + ( t.empty() ? std::string( "-" ) : t );
    }
    for ( int k = 0; k < len; ++k )
	cs += fmt( "%s%u", k ? "," : "", (unsigned)lc[k] );
    if ( si.num_glyphs == 0 )
	gs = as = os = ts = ".";
    if ( len == 0 )
	cs = ".";
    return r + " | g " + gs + " | a " + as + " | o " + os + " | c " + cs + " | t " + ts;
}

static void putGlyphs( const std::string &fk, const char *sid, const char *what, QTextEngine *e )
{
    put( fk, sid, ( std::string( what ) + ".n" ).c_str(), fmt( "%d", e->items.size() ) );
    for ( int i = 0; i < e->items.size(); ++i )
	put( fk, sid, fmt( "%s.%02d", what, i ).c_str(), glyphItem( e, i ) );
}

static bool dumpGlyphs = TRUE;

static void runSample( const FontSpec &spec, const QFont &f, const Sample &smp,
		       TextEdit *te, const QString &pngdir, int widthBefore )
{
    const std::string fk = fontKey( spec );
    const char *sid = smp.id;
    QFontMetrics fm( f );
    QString s = QString::fromUtf8( smp.utf8 );
    const int n = s.length();
    const bool ltrOnly = !( smp.flags & ( F_RTL | F_BIDI ) );

    // QFontMetrics
    int width = fm.width( s );
    put( fk, sid, "width", fmt( "%d", width ) );
    std::vector<int> prefix, cw, qcw, inf;
    for ( int k = 0; k <= n; ++k )
	prefix.push_back( fm.width( s, k ) );
    int sum = 0;
    for ( int i = 0; i < n; ++i ) {
	cw.push_back( fm.charWidth( s, i ) );
	qcw.push_back( fm.width( s[i] ) );
	inf.push_back( fm.inFont( s[i] ) ? 1 : 0 );
	sum += cw.back();
    }
    put( fk, sid, "fm.prefix", ints( prefix ) );
    put( fk, sid, "fm.charWidth", ints( cw ) );
    put( fk, sid, "fm.widthQChar", ints( qcw ) );
    put( fk, sid, "fm.inFont", ints( inf ) );
    QRect br = fm.boundingRect( s );
    put( fk, sid, "fm.boundingRect", rectStr( br ) );
    // what QLabel, QButton and friends use
    put( fk, sid, "fm.boundingRectFlags",
	 rectStr( fm.boundingRect( 0, 0, 2000, 500, Qt::AlignLeft | Qt::AlignTop | Qt::ExpandTabs, s ) ) );
    {
	QSize sz = fm.size( Qt::ExpandTabs, s );
	put( fk, sid, "fm.size", fmt( "%dx%d", sz.width(), sz.height() ) );
    }

    check( widthBefore == width, true, "width-stable", fk, smp,
	   fmt( "width() was %d before the other measurements, %d after", widthBefore, width ) );
    check( prefix[n] == width, true, "width-len", fk, smp,
	   fmt( "width(s, len)=%d width(s)=%d", prefix[n], width ) );
    check( sum == width, true, "width-additive", fk, smp,
	   fmt( "width=%d sum(charWidth)=%d", width, sum ) );
    {
	int bad = -1;
	for ( int k = 0; k < n && bad < 0; ++k )
	    if ( prefix[k + 1] < prefix[k] )
		bad = k;
	check( bad < 0, true, "prefix-monotonic", fk, smp,
	       bad < 0 ? std::string() : fmt( "width(s,%d)=%d > width(s,%d)=%d", bad, prefix[bad], bad + 1, prefix[bad + 1] ) );
    }

    // QLineEdit's layout
    LineEditModel le( s, f );
    {
	std::string items;
	int used = 0;
	for ( int i = 0; i < le.layout.numItems(); ++i ) {
	    QTextItem ti = le.layout.itemAt( i );
	    if ( i )
		items += ';';
	    items += fmt( "%d+%d@%d,%d,w%d,a%d,d%d%s%s%s", ti.from(), ti.length(), ti.x(), ti.y(),
			  ti.width(), ti.ascent(), ti.descent(), ti.isRightToLeft() ? ",rtl" : "",
			  ti.isSpace() ? ",space" : "", ti.isTab() ? ",tab" : "" );
	    used = QMAX( used, ti.x() + ti.width() );
	}
	put( fk, sid, "layout.items", items.empty() ? std::string( "." ) : items );
	put( fk, sid, "layout.widthUsed", fmt( "%d", le.layout.widthUsed() ) );
	// QLineEdit lays out the text with non-printable characters replaced
	QString shown( s.unicode(), s.length() );
	for ( int i = 0; i < n; ++i )
	    if ( !shown[i].isPrint() )
		shown[i] = QChar( 0x0020 );
	int sw = fm.width( shown );
	check( used == sw, true, "layout-width", fk, smp,
	       fmt( "line edit layout %d px, QFontMetrics::width %d px", used, sw ) );

	std::vector<int> valid, cx, back;
	for ( int i = 0; i <= n; ++i ) {
	    bool v = le.valid( i );
	    valid.push_back( v ? 1 : 0 );
	    if ( !v ) {
		cx.push_back( INT_MIN );
		back.push_back( INT_MIN );
		continue;
	    }
	    cx.push_back( le.cursorToX( i ) );
	    back.push_back( le.xToPos( cx.back() ) );
	}
	put( fk, sid, "le.valid", ints( valid ) );
	put( fk, sid, "le.cursorToX", ints( cx ) );
	put( fk, sid, "le.xToCursor", ints( back ) );
	roundTrip( "le", fk, smp, valid, cx, back, n );
	surrogateStops( "le", fk, smp, s, valid );
	// the cursor moves in one direction through text of one direction
	if ( !( smp.flags & F_BIDI ) ) {
	    const bool rtl = smp.flags & F_RTL;
	    std::string bad;
	    int last = rtl ? INT_MAX : INT_MIN;
	    for ( int i = 0; i <= n; ++i ) {
		if ( !valid[i] )
		    continue;
		if ( rtl ? cx[i] > last : cx[i] < last )
		    bad += fmt( " x(%d)=%d%s%d", i, cx[i], rtl ? ">" : "<", last );
		last = cx[i];
	    }
	    check( bad.empty(), true, "le-monotonic", fk, smp, bad );
	}
    }

    // QTextEdit (rich text engine, plain text format, no wrapping);
    // only the first paragraph
    {
	te->setFont( f );
	te->setText( s );
	te->sync();
	QTextCursor *c = te->cursor();
	te->setCursorPosition( 0, 0 );
	QTextParagraph *para = c->paragraph();
	QRect pr = te->paragraphRect( 0 );
	put( fk, sid, "te.paragraphRect", rectStr( pr ) );
	int plen = para->length() - 1;	// without the trailing space
	std::vector<int> x, w, valid, cx, back;
	for ( int i = 0; i < plen; ++i ) {
	    x.push_back( para->at( i )->x );
	    w.push_back( para->string()->width( i ) );
	}
	for ( int i = 0; i <= plen; ++i ) {
	    bool v = para->string()->validCursorPosition( i );
	    valid.push_back( v ? 1 : 0 );
	    if ( !v ) {
		cx.push_back( INT_MIN );
		back.push_back( INT_MIN );
		continue;
	    }
	    te->setCursorPosition( 0, i );
	    cx.push_back( c->x() );
	    int p = -1;
	    int idx = te->charAt( QPoint( pr.x() + cx.back(), pr.y() + 1 ), &p );
	    back.push_back( p == 0 ? idx : -1000 - p );
	}
	put( fk, sid, "te.charX", ints( x ) );
	put( fk, sid, "te.charWidth", ints( w ) );
	put( fk, sid, "te.valid", ints( valid ) );
	put( fk, sid, "te.cursorX", ints( cx ) );
	put( fk, sid, "te.charAt", ints( back ) );
	roundTrip( "te", fk, smp, valid, cx, back, plen );
	surrogateStops( "te", fk, smp, s, valid );
	// the rich text engine places the characters one after the other
	std::string bad;
	if ( ltrOnly && !( smp.flags & F_CTRL ) ) {
	    for ( int i = 0; i + 1 < plen; ++i )
		if ( x[i + 1] != x[i] + w[i] )
		    bad += fmt( " x[%d]=%d!=%d+%d", i + 1, x[i + 1], x[i], w[i] );
	    check( bad.empty(), true, "te-adjacent", fk, smp, bad );
	}
	// and agrees with QFontMetrics (tabs and newlines excepted)
	if ( !( smp.flags & ( F_CTRL | F_NL ) ) ) {
	    int tw = 0;
	    for ( int i = 0; i < plen; ++i )
		tw += w[i];
	    check( tw == width, true, "te-width", fk, smp,
		   fmt( "sum(te char widths)=%d width=%d", tw, width ) );
	}
	// and with QLineEdit (left-to-right text only: the paragraph's
	// left margin is the cursor x of position 0)
	if ( ltrOnly && !( smp.flags & ( F_CTRL | F_NL ) ) && plen == n ) {
	    std::string diff;
	    for ( int i = 0; i <= n; ++i )
		if ( valid[i] && le.valid( i ) && cx[i] - cx[0] != le.cursorToX( i ) )
		    diff += fmt( " %d:te%d/le%d", i, cx[i] - cx[0], le.cursorToX( i ) );
	    check( diff.empty(), true, "te-le-cursor", fk, smp, diff );
	}
    }

    // drawing
    QString png;
    if ( !pngdir.isEmpty() )
	png = pngdir + "/" + sid + ".png";
    Ink ink = drawString( f, s, png );
    put( fk, sid, "draw.size", fmt( "%dx%d", ink.w, ink.h ) );
    put( fk, sid, "draw.ink", rectStr( ink.box ) );
    put( fk, sid, "draw.pixels", fmt( "%d", ink.pixels ) );
    put( fk, sid, "draw.hash", fmt( "%016llx", (unsigned long long)ink.hash ) );
    if ( ink.pixels ) {
	check( br.contains( ink.box ), true, "ink-in-boundingRect", fk, smp,
	       fmt( "ink %s boundingRect %s", rectStr( ink.box ).c_str(), rectStr( br ).c_str() ) );
    }
    if ( !( smp.flags & F_SPACE ) )
	check( ink.pixels > 0, true, "ink-nonempty", fk, smp, "nothing drawn" );
    check( !ink.clipped, true, "ink-clipped", fk, smp, "ink at the border of the pixmap (textcheck's pixmap too small)" );

    // clusters drawn one by one: missing (inFont() false), drawn as a box
    // (contain the "missing glyph" image of one of the fonts, see
    // boxRefs) or blank (nothing drawn for a visible character)
    {
	QTextLayout cl( s, f );
	Tofu &t = tofu[ smp.script ];
	std::string miss, box, blank;
	int i = 0;
	while ( i < n ) {
	    int next = cl.nextCursorPosition( i );
	    if ( next <= i || next > n )
		next = i + 1;
	    // QTextLayout puts cursor stops inside surrogate pairs
	    // (see le-surrogate-inside); keep the pairs whole
	    if ( next < n && isHigh( s[next - 1] ) && isLow( s[next] ) )
		++next;
	    QString sub = s.mid( i, next - i );
	    bool visible = FALSE, missing = FALSE;
	    for ( int k = 0; k < (int)sub.length(); ++k ) {
		QChar ch = sub[k];
		ushort u = ch.unicode();
		bool surrogate = u >= 0xd800 && u < 0xe000;
		if ( !surrogate && ( !ch.isPrint() || ch.category() == QChar::Other_Format ) )
		    continue;
		if ( !surrogate && !fm.inFont( ch ) )
		    missing = TRUE;
		if ( !ch.isSpace() && ch.category() != QChar::Mark_NonSpacing )
		    visible = TRUE;
	    }
	    if ( visible || missing ) {
		t.clusters++;
		if ( missing ) {
		    t.missing++;
		    miss += fmt( " %d", i );
		}
		Ink ci = drawString( f, sub, QString::null );
		if ( ci.pixels && hasBox( ci.crop ) ) {
		    t.box++;
		    box += fmt( " %d", i );
		}
		if ( visible && !ci.pixels ) {
		    t.blank++;
		    blank += fmt( " %d", i );
		}
	    }
	    i = next;
	}
	put( fk, sid, "clusters.missing", miss.empty() ? std::string( "." ) : miss.substr( 1 ) );
	put( fk, sid, "clusters.box", box.empty() ? std::string( "." ) : box.substr( 1 ) );
	put( fk, sid, "clusters.blank", blank.empty() ? std::string( "." ) : blank.substr( 1 ) );
    }

    /*
      The glyphs, last, so that they cannot change the measurements above:
      glyphs.le.* are the items of QLineEdit's layout (le, shaped by
      QTextLayout::addCurrentItem()), glyphs.draw.* those that
      QPainter::drawText( x, y, s ) shapes (the same QTextEngine calls,
      without a painter).
    */
    if ( dumpGlyphs ) {
	putGlyphs( fk, sid, "glyphs.le", layoutEngine( le.layout ) );
	if ( n > 0 ) {
	    QTextEngine engine( s, fontPrivate( f ) );
	    engine.itemize( s.simpleText() ? QTextEngine::NoBidi | QTextEngine::SingleLine
			    : QTextEngine::Full | QTextEngine::SingleLine );
	    for ( int i = 0; i < engine.items.size(); ++i )
		engine.shape( i );
	    putGlyphs( fk, sid, "glyphs.draw", &engine );
	} else {
	    put( fk, sid, "glyphs.draw.n", "0" );
	}
    }
}

static void writeLines( const QString &file, const std::string &head, std::vector<std::string> lines, bool sort )
{
    if ( sort )
	std::sort( lines.begin(), lines.end() );
    FILE *f = fopen( QFile::encodeName( file ), "w" );
    if ( !f ) {
	fprintf( stderr, "textcheck: cannot write %s\n", (const char *)QFile::encodeName( file ) );
	exit( 2 );
    }
    fputs( head.c_str(), f );
    for ( unsigned i = 0; i < lines.size(); ++i ) {
	fputs( lines[i].c_str(), f );
	fputc( '\n', f );
    }
    fclose( f );
}

static const char *scriptName( QFont::Script s )
{
    switch ( s ) {
    case QFont::Latin: return "Latin";
    case QFont::Greek: return "Greek";
    case QFont::Cyrillic: return "Cyrillic";
    case QFont::Hebrew: return "Hebrew";
    case QFont::Arabic: return "Arabic";
    case QFont::Devanagari: return "Devanagari";
    case QFont::Thai: return "Thai";
    case QFont::Han: return "Han";
    case QFont::Hiragana: return "Hiragana";
    case QFont::Katakana: return "Katakana";
    case QFont::Hangul: return "Hangul";
    case QFont::CombiningMarks: return "CombiningMarks";
    case QFont::MathematicalOperators: return "MathematicalOperators";
    case QFont::TechnicalSymbols: return "TechnicalSymbols";
    case QFont::GeometricSymbols: return "GeometricSymbols";
    case QFont::CurrencySymbols: return "CurrencySymbols";
    case QFont::Unicode: return "Unicode";
    default: return "?";
    }
}

int main( int argc, char **argv )
{
    QApplication app( argc, argv );
    if ( app.argc() < 2 ) {
	fprintf( stderr, "usage: textcheck outdir [-nopng] [-noglyphs]\n" );
	return 2;
    }
    QString out = QFile::decodeName( app.argv()[1] );
    bool pngs = TRUE;
    for ( int i = 2; i < app.argc(); ++i ) {
	if ( !strcmp( app.argv()[i], "-nopng" ) ) {
	    pngs = FALSE;
	} else if ( !strcmp( app.argv()[i], "-noglyphs" ) ) {
	    dumpGlyphs = FALSE;
	} else {
	    fprintf( stderr, "usage: textcheck outdir [-nopng] [-noglyphs]\n" );
	    return 2;
	}
    }
    QDir().mkdir( out );

    // point sizes are converted with the DPI of the X server (or Xft.dpi);
    // fix it so that the results do not depend on the server's screen
    // size. TEXTCHECK_DPI=0 keeps the server's.
    int serverDpi = QPaintDevice::x11AppDpiY();
    int dpi = 96;
    if ( getenv( "TEXTCHECK_DPI" ) )
	dpi = atoi( getenv( "TEXTCHECK_DPI" ) );
    if ( dpi > 0 ) {
	QPaintDevice::x11SetAppDpiX( dpi );
	QPaintDevice::x11SetAppDpiY( dpi );
    }

    std::vector<FontSpec> specs = fontSpecs();
    std::vector<std::string> env;
    env.push_back( fmt( "qt %s", qVersion() ) );
    env.push_back( fmt( "dpi %d %d (server %d)", QPaintDevice::x11AppDpiX(), QPaintDevice::x11AppDpiY(), serverDpi ) );
    env.push_back( fmt( "depth %d", QPaintDevice::x11AppDepth() ) );
    // the X resources that XftDefaultSubstitute() applies to every font
    static const char *xftres[] = { "antialias", "hinting", "hintstyle", "rgba", "lcdfilter", "autohint", "embolden", "dpi" };
    for ( unsigned i = 0; i < sizeof(xftres) / sizeof(xftres[0]); ++i ) {
	const char *v = XGetDefault( QPaintDevice::x11AppDisplay(), "Xft", xftres[i] );
	env.push_back( fmt( "Xft.%s %s", xftres[i], v ? v : "(unset)" ) );
    }

    for ( int i = 0; i < ncorpus; ++i ) {
	QString s = QString::fromUtf8( corpus[i].utf8 );
	report.push_back( fmt( "corpus %s script %s", corpus[i].id, corpus[i].script ) );
	report.push_back( std::string( "corpus " ) + corpus[i].id + " text " + escaped( s ) );
    }

    static const QFont::Script scripts[] = {
	QFont::Latin, QFont::Greek, QFont::Cyrillic, QFont::Hebrew, QFont::Arabic,
	QFont::Devanagari, QFont::Thai, QFont::Han, QFont::Hiragana, QFont::Katakana,
	QFont::Hangul, QFont::CombiningMarks, QFont::MathematicalOperators,
	QFont::TechnicalSymbols, QFont::GeometricSymbols, QFont::CurrencySymbols,
	QFont::Unicode
    };

    TextEdit *te = new TextEdit;
    te->setTextFormat( Qt::PlainText );
    te->setWordWrap( QTextEdit::NoWrap );
    te->resize( 1200, 300 );

    for ( unsigned fi = 0; fi < specs.size(); ++fi ) {
	const FontSpec &spec = specs[fi];
	const std::string fk = fontKey( spec );
	QFont f = makeFont( spec );
	QFontMetrics fm( f );
	QFontInfo info( f );
	env.push_back( fmt( "font %s request \"%s\" weight %d italic %d -> \"%s\" %dpx weight %d italic %d fixed %d exact %d",
			    fk.c_str(), spec.family, spec.weight, spec.italic,
			    (const char *)info.family().utf8(), info.pixelSize(), info.weight(),
			    info.italic(), info.fixedPitch(), info.exactMatch() ) );
	for ( unsigned k = 0; k < sizeof(scripts) / sizeof(scripts[0]); ++k ) {
	    QFontInfo si( f, scripts[k] );
	    env.push_back( fmt( "font %s script %s -> \"%s\" %dpx", fk.c_str(), scriptName( scripts[k] ),
				(const char *)si.family().utf8(), si.pixelSize() ) );
	}
	put( fk, "-", "metrics", fmt( "ascent %d descent %d height %d leading %d lineSpacing %d maxWidth %d minLeftBearing %d minRightBearing %d underlinePos %d strikeOutPos %d lineWidth %d",
				     fm.ascent(), fm.descent(), fm.height(), fm.leading(), fm.lineSpacing(),
				     fm.maxWidth(), fm.minLeftBearing(), fm.minRightBearing(),
				     fm.underlinePos(), fm.strikeOutPos(), fm.lineWidth() ) );

	QString pngdir;
	if ( pngs ) {
	    pngdir = out + "/png";
	    QDir().mkdir( pngdir );
	    pngdir += "/" + QString( fk.c_str() );
	    QDir().mkdir( pngdir );
	}
	// width() first for all strings: it must not depend on what was
	// measured before (QFontMetrics caches widths per character)
	makeBoxRefs( f, fk );
	std::vector<int> before;
	for ( int i = 0; i < ncorpus; ++i )
	    before.push_back( fm.width( QString::fromUtf8( corpus[i].utf8 ) ) );
	for ( int i = 0; i < ncorpus; ++i )
	    runSample( spec, f, corpus[i], te, pngdir, before[i] );
    }
    delete te;

    // summary
    std::vector<std::string> sum;
    for ( std::map<std::string, std::map<std::string, Count> >::const_iterator it = counts.begin();
	  it != counts.end(); ++it ) {
	int c = 0, fl = 0;
	std::string per;
	for ( std::map<std::string, Count>::const_iterator s = it->second.begin(); s != it->second.end(); ++s ) {
	    c += s->second.checked;
	    fl += s->second.failed;
	    if ( s->second.failed )
		per += fmt( " %s:%d/%d", s->first.c_str(), s->second.failed, s->second.checked );
	}
	sum.push_back( fmt( "check %-36s %5d checked %5d failed", it->first.c_str(), c, fl ) + per );
    }
    for ( std::map<std::string, Tofu>::const_iterator it = tofu.begin(); it != tofu.end(); ++it )
	sum.push_back( fmt( "clusters %-12s %5d drawn %5d missing (inFont) %5d box %5d blank", it->first.c_str(),
			    it->second.clusters, it->second.missing, it->second.box, it->second.blank ) );
    sum.push_back( fmt( "total %d FAIL, %d NOTE", (int)fails.size(), (int)notes.size() ) );

    writeLines( out + "/report.txt", "# textcheck report: <font> <string> <metric> <value>\n", report, true );
    std::vector<std::string> fl = fails;
    fl.insert( fl.end(), notes.begin(), notes.end() );
    writeLines( out + "/fails.txt", "", fl, true );
    writeLines( out + "/summary.txt", "", sum, false );
    writeLines( out + "/env.txt", "", env, false );

    printf( "textcheck: %d FAIL, %d NOTE (%s/fails.txt)\n", (int)fails.size(), (int)notes.size(),
	    (const char *)QFile::encodeName( out ) );
    return fails.empty() ? 0 : 1;
}
