// imagebounds: the image readers against crafted files (sizes and offsets
// from header fields, palettes, LZW tables) and a digest of what they make
// of ordinary images, for a before/after comparison of reader changes.
//
//   imagebounds gen DIR       writes the synthetic ordinary images made here
//                             (GIFs with frames at offsets, disposal,
//                             interlace, ...; BMP, PNM, XPM, XBM) to DIR
//   imagebounds crafted [N]   runs the crafted cases (or the one named N),
//                             each way of reading in its own process: the
//                             result, the largest allocation asked for, the
//                             peak of live heap and the process's peak RSS;
//                             FAIL where the result is not the expected one
//                             (no crash, no sanitizer report, no allocation
//                             beyond the limits); exits with the failures
//   imagebounds write DIR     writes the crafted files to DIR (named as the cases)
//   imagebounds one CASE load|decode
//                             one crafted case in this process
//   imagebounds dump FILE...  for each file: QImage::load() and every frame
//                             of a QImageDecoder (as QMovie and KHTML read
//                             GIF, PNG and MNG): size, depth, colors, digest
//
// IMGB_PLUGINS: the directory whose imageformats/ holds the JPEG and MNG
// plugins to use (default: those of the Qt the program runs with).
#include <qapplication.h>
#include <qasyncimageio.h>
#include <qfile.h>
#include <qimage.h>
#include <qstringlist.h>
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>
#include <string>
#include <vector>

#if defined(__SANITIZE_ADDRESS__)
#  define IMGB_ASAN 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define IMGB_ASAN 1
#  endif
#endif

typedef std::vector<unsigned char> Bytes;

// ------------------------------------------------------------ allocations
// Without ASan the program's own malloc family counts what Qt and the
// image libraries ask for: the largest single request and the peak of
// live heap.  (With ASan its allocator is in charge; RSS only.)
#ifndef IMGB_ASAN
extern "C" {
void *__libc_malloc( size_t );
void *__libc_calloc( size_t, size_t );
void *__libc_realloc( void *, size_t );
void __libc_free( void * );
void *__libc_memalign( size_t, size_t );
}
static size_t g_live = 0, g_peak = 0, g_largest = 0;
static inline void counted( void *p, size_t n )
{
    if ( n > g_largest )
	g_largest = n;
    if ( p ) {
	g_live += malloc_usable_size( p );
	if ( g_live > g_peak )
	    g_peak = g_live;
    }
}
extern "C" void *malloc( size_t n )
{
    void *p = __libc_malloc( n );
    counted( p, n );
    return p;
}
extern "C" void *calloc( size_t n, size_t s )
{
    void *p = __libc_calloc( n, s );
    counted( p, s && n > (size_t)-1 / s ? (size_t)-1 : n * s );
    return p;
}
extern "C" void *realloc( void *o, size_t n )
{
    const size_t old = o ? malloc_usable_size( o ) : 0;
    void *p = __libc_realloc( o, n );
    if ( p || !n )
	g_live -= old;
    counted( p, n );
    return p;
}
extern "C" void free( void *p )
{
    if ( p )
	g_live -= malloc_usable_size( p );
    __libc_free( p );
}
extern "C" void *memalign( size_t a, size_t n )
{
    void *p = __libc_memalign( a, n );
    counted( p, n );
    return p;
}
extern "C" int posix_memalign( void **r, size_t a, size_t n )
{
    *r = __libc_memalign( a, n );
    counted( *r, n );
    return *r ? 0 : 12;	// ENOMEM
}
extern "C" void *aligned_alloc( size_t a, size_t n )
{
    void *p = __libc_memalign( a, n );
    counted( p, n );
    return p;
}
#endif

// ------------------------------------------------------------ byte helpers
static void put( Bytes &b, int c ) { b.push_back( (unsigned char)c ); }
static void put16( Bytes &b, int v ) { put( b, v & 0xff ); put( b, ( v >> 8 ) & 0xff ); }
static void put32( Bytes &b, unsigned v ) { put16( b, v & 0xffff ); put16( b, v >> 16 ); }
static void put32be( Bytes &b, unsigned v ) { put( b, v >> 24 ); put( b, v >> 16 ); put( b, v >> 8 ); put( b, v ); }
static void puts( Bytes &b, const char *s ) { while ( *s ) put( b, *s++ ); }
static void append( Bytes &b, const Bytes &c ) { b.insert( b.end(), c.begin(), c.end() ); }

// ------------------------------------------------------------ GIF writer
static QRgb palColor( int i ) { return qRgb( ( i * 37 + 11 ) & 255, ( i * 91 + 7 ) & 255, ( i * 53 + 3 ) & 255 ); }

static void gifPalette( Bytes &b, int bits )
{
    for ( int i = 0; i < ( 1 << bits ); ++i ) {
	const QRgb c = palColor( i );
	put( b, qRed( c ) ); put( b, qGreen( c ) ); put( b, qBlue( c ) );
    }
}

// logical screen; gctBits 1..8, 0 = no global color table
static void gifHeader( Bytes &b, int sw, int sh, int gctBits, int bg = 0 )
{
    puts( b, "GIF89a" );
    put16( b, sw ); put16( b, sh );
    put( b, gctBits ? 0x80 | ( ( gctBits - 1 ) << 4 ) | ( gctBits - 1 ) : 0 );
    put( b, bg ); put( b, 0 );
    if ( gctBits )
	gifPalette( b, gctBits );
}

static void gifGce( Bytes &b, int disposal, int delay, int trans )
{
    put( b, '!' ); put( b, 0xf9 ); put( b, 4 );
    put( b, ( disposal << 2 ) | ( trans >= 0 ? 1 : 0 ) );
    put16( b, delay ); put( b, trans >= 0 ? trans : 0 ); put( b, 0 );
}

static void gifLoop( Bytes &b )
{
    put( b, '!' ); put( b, 0xff ); put( b, 11 ); puts( b, "NETSCAPE2.0" );
    put( b, 3 ); put( b, 1 ); put16( b, 0 ); put( b, 0 );
}

struct BitPacker {
    Bytes out;
    unsigned acc;
    int bits;
    BitPacker() : acc( 0 ), bits( 0 ) {}
    void code( int c, int size )
    {
	acc |= (unsigned)c << bits;
	bits += size;
	while ( bits >= 8 ) { put( out, acc & 0xff ); acc >>= 8; bits -= 8; }
    }
    void flush() { if ( bits > 0 ) put( out, acc & 0xff ); acc = 0; bits = 0; }
};

static void subBlocks( Bytes &b, const Bytes &data )
{
    for ( size_t i = 0; i < data.size(); i += 255 ) {
	const size_t n = data.size() - i < 255 ? data.size() - i : 255;
	put( b, (int)n );
	b.insert( b.end(), data.begin() + i, data.begin() + i + n );
    }
    put( b, 0 );
}

// LZW of the pixel indexes (GIF variant, clear when the table is full)
static Bytes lzw( const std::vector<unsigned char> &px, int minSize )
{
    const int clear = 1 << minSize, eoi = clear + 1;
    std::vector<short> dict( 4096 * 256, -1 );
    BitPacker p;
    int size = minSize + 1, next = clear + 2;
    p.code( clear, size );
    int prefix = -1;
    for ( size_t i = 0; i < px.size(); ++i ) {
	const int c = px[i];
	if ( prefix < 0 ) { prefix = c; continue; }
	const int k = dict[prefix * 256 + c];
	if ( k >= 0 ) { prefix = k; continue; }
	p.code( prefix, size );
	if ( next < 4096 ) {
	    dict[prefix * 256 + c] = (short)next++;
	    if ( next > ( 1 << size ) && size < 12 )
		++size;
	} else {
	    p.code( clear, size );
	    std::fill( dict.begin(), dict.end(), (short)-1 );
	    size = minSize + 1;
	    next = clear + 2;
	}
	prefix = c;
    }
    if ( prefix >= 0 )
	p.code( prefix, size );
    p.code( eoi, size );
    p.flush();
    return p.out;
}

// one image: pixel (x,y) = pattern; interlaced rows in GIF order
static void gifImage( Bytes &b, int l, int t, int w, int h, int ncolBits, int seed,
		      bool interlace = false, int lctBits = 0 )
{
    put( b, ',' ); put16( b, l ); put16( b, t ); put16( b, w ); put16( b, h );
    put( b, ( lctBits ? 0x80 | ( lctBits - 1 ) : 0 ) | ( interlace ? 0x40 : 0 ) );
    if ( lctBits )
	gifPalette( b, lctBits );
    std::vector<int> rows;
    if ( interlace ) {
	for ( int y = 0; y < h; y += 8 ) rows.push_back( y );
	for ( int y = 4; y < h; y += 8 ) rows.push_back( y );
	for ( int y = 2; y < h; y += 4 ) rows.push_back( y );
	for ( int y = 1; y < h; y += 2 ) rows.push_back( y );
    } else {
	for ( int y = 0; y < h; ++y ) rows.push_back( y );
    }
    const int ncols = 1 << ncolBits;
    std::vector<unsigned char> px;
    px.reserve( (size_t)w * h );
    for ( size_t r = 0; r < rows.size(); ++r )
	for ( int x = 0; x < w; ++x )
	    px.push_back( (unsigned char)( ( x / 3 + rows[r] / 2 * 5 + seed ) % ncols ) );
    const int minSize = ncolBits < 2 ? 2 : ncolBits;
    put( b, minSize );
    subBlocks( b, lzw( px, minSize ) );
}

// an image whose data is the given codes (all of the given width)
static void gifRawImage( Bytes &b, int l, int t, int w, int h, int minSize,
			 const std::vector<int> &codes, int width, bool interlace = false )
{
    put( b, ',' ); put16( b, l ); put16( b, t ); put16( b, w ); put16( b, h );
    put( b, interlace ? 0x40 : 0 );
    put( b, minSize );
    BitPacker p;
    for ( size_t i = 0; i < codes.size(); ++i )
	p.code( codes[i], width );
    p.flush();
    subBlocks( b, p.out );
}

// ------------------------------------------------------------ other formats
static Bytes bmp( int w, int h, int bits, int comp, int clrUsed, const Bytes &pixels, int ncolsWritten )
{
    Bytes info;
    put32( info, 40 ); put32( info, w ); put32( info, h ); put16( info, 1 ); put16( info, bits );
    put32( info, comp ); put32( info, pixels.size() ); put32( info, 2835 ); put32( info, 2835 );
    put32( info, clrUsed ); put32( info, 0 );
    for ( int i = 0; i < ncolsWritten; ++i ) {
	const QRgb c = palColor( i );
	put( info, qBlue( c ) ); put( info, qGreen( c ) ); put( info, qRed( c ) ); put( info, 0 );
    }
    Bytes b;
    puts( b, "BM" ); put32( b, 14 + info.size() + pixels.size() ); put32( b, 0 ); put32( b, 14 + info.size() );
    append( b, info );
    append( b, pixels );
    return b;
}

static void pngChunk( Bytes &b, const char *type, const Bytes &data )
{
    put32be( b, data.size() );
    Bytes td;
    puts( td, type );
    append( td, data );
    append( b, td );
    put32be( b, crc32( 0, &td[0], td.size() ) );
}

static Bytes pngIHDR( unsigned w, unsigned h, int depth, int ctype )
{
    Bytes d;
    put32be( d, w ); put32be( d, h ); put( d, depth ); put( d, ctype ); put( d, 0 ); put( d, 0 ); put( d, 0 );
    return d;
}

static Bytes zlibOf( const Bytes &raw )
{
    uLongf n = compressBound( raw.size() );
    Bytes z( n );
    compress2( &z[0], &n, &raw[0], raw.size(), 9 );
    z.resize( n );
    return z;
}

// the start of a zlib stream (flushed, not finished: more rows to come)
static Bytes zlibStart( const Bytes &raw )
{
    z_stream z;
    memset( &z, 0, sizeof z );
    deflateInit( &z, 9 );
    Bytes out( compressBound( raw.size() ) + 64 );
    z.next_in = (Bytef *)&raw[0];
    z.avail_in = raw.size();
    z.next_out = &out[0];
    z.avail_out = out.size();
    deflate( &z, Z_SYNC_FLUSH );
    out.resize( out.size() - z.avail_out );
    deflateEnd( &z );
    return out;
}

static Bytes readFile( const char *path )
{
    Bytes b;
    FILE *f = fopen( path, "rb" );
    if ( !f )
	return b;
    unsigned char buf[65536];
    size_t n;
    while ( ( n = fread( buf, 1, sizeof buf, f ) ) > 0 )
	b.insert( b.end(), buf, buf + n );
    fclose( f );
    return b;
}

// a small JPEG made by cjpeg, its frame header patched to w x h
static Bytes jpegResized( const unsigned char *jpg, size_t len, int w, int h )
{
    Bytes b( jpg, jpg + len );
    for ( size_t i = 2; i + 9 < b.size(); ) {
	if ( b[i] != 0xff ) break;
	const int m = b[i + 1];
	const int l = ( b[i + 2] << 8 ) | b[i + 3];
	if ( m >= 0xc0 && m <= 0xc2 ) {
	    b[i + 5] = h >> 8; b[i + 6] = h & 0xff;
	    b[i + 7] = w >> 8; b[i + 8] = w & 0xff;
	    break;
	}
	i += 2 + l;
    }
    return b;
}

// ------------------------------------------------------------ digests
static unsigned long long fnv( unsigned long long h, const void *p, size_t n )
{
    const unsigned char *c = (const unsigned char *)p;
    for ( size_t i = 0; i < n; ++i ) { h ^= c[i]; h *= 1099511628211ULL; }
    return h;
}

static std::string digest( const QImage &im )
{
    char s[200];
    if ( im.isNull() )
	return "null";
    unsigned long long h = 14695981039346656037ULL;
    const int bitsPerLine = im.width() * im.depth();
    for ( int y = 0; y < im.height(); ++y ) {
	const uchar *l = im.scanLine( y );
	h = fnv( h, l, bitsPerLine / 8 );
	if ( bitsPerLine % 8 ) {	// the bits of the last byte that are pixels
	    const int keep = bitsPerLine % 8;
	    uchar last = l[bitsPerLine / 8];
	    last &= im.bitOrder() == QImage::LittleEndian ? ( 1 << keep ) - 1 : 0xff << ( 8 - keep );
	    h = fnv( h, &last, 1 );
	}
    }
    if ( im.numColors() )
	h = fnv( h, im.colorTable(), im.numColors() * sizeof( QRgb ) );
    sprintf( s, "%dx%d d%d c%d a%d %016llx", im.width(), im.height(), im.depth(), im.numColors(),
	     im.hasAlphaBuffer() ? 1 : 0, h );
    return s;
}

// every pixel through the color table, and a conversion to 32 bpp: what
// any user of an indexed image does (reads beyond a short color table)
volatile unsigned g_sum = 0;
static void touchPixels( const QImage &im )
{
    if ( im.isNull() )
	return;
    volatile unsigned &sum = g_sum;
    for ( int y = 0; y < im.height(); y += 1 + im.height() / 512 )
	for ( int x = 0; x < im.width(); x += 1 + im.width() / 512 )
	    sum += im.pixel( x, y );
    QImage c = im.convertDepth( 32 );
    sum += c.isNull() ? 0 : c.pixel( 0, 0 );
}

class Consumer : public QImageConsumer {
public:
    QImageDecoder *decoder;
    int frames;
    std::string log;
    bool keepLog;
    Consumer() : decoder( 0 ), frames( 0 ), keepLog( true ) {}
    void note( const char *fmt, int a = 0, int b = 0, int c = 0, int d = 0 )
    {
	if ( !keepLog )
	    return;
	char s[200];
	sprintf( s, fmt, a, b, c, d );
	log += s;
    }
    void end() { note( "  end\n" ); }
    void changed( const QRect &r ) { note( "  changed %d,%d %dx%d\n", r.x(), r.y(), r.width(), r.height() ); }
    void frameDone() { frameDone( QPoint( 0, 0 ), QRect() ); }
    void frameDone( const QPoint &p, const QRect &r )
    {
	++frames;
	if ( !keepLog )
	    return;
	char s[300];
	sprintf( s, "  frame %d at %d,%d rect %d,%d %dx%d: %s\n", frames, p.x(), p.y(), r.x(), r.y(),
		 r.width(), r.height(), digest( decoder->image() ).c_str() );
	log += s;
    }
    void setLooping( int n ) { note( "  looping %d\n", n ); }
    void setFramePeriod( int ms ) { note( "  period %d\n", ms ); }
    void setSize( int w, int h ) { note( "  size %dx%d\n", w, h ); }
};

// all frames, fed in 2048 byte pieces as QMovie and QImageIO do
static int decodeAll( const Bytes &data, Consumer &c, QImage *last = 0 )
{
    QImageDecoder decoder( &c );
    c.decoder = &decoder;
    size_t pos = 0;
    int r = 0;
    while ( pos < data.size() ) {
	const int n = data.size() - pos < 2048 ? data.size() - pos : 2048;
	const uchar *p = &data[pos];
	int left = n;
	while ( left > 0 ) {
	    r = decoder.decode( p, left );
	    if ( r <= 0 )
		break;
	    p += r;
	    left -= r;
	}
	if ( r < 0 )
	    break;
	if ( r == 0 && left > 0 ) {	// the decoder needs more than this piece
	    pos += n - left;
	    if ( pos + left >= data.size() )
		break;
	    continue;
	}
	pos += n;
    }
    if ( last )
	*last = decoder.image();
    c.decoder = 0;
    return r;
}

// ------------------------------------------------------------ crafted cases
static Bytes g_tinyJpeg, g_tinyProgJpeg, g_tinyMultiScanJpeg;

struct Case {
    const char *name;
    const char *what;
    Bytes data;
    const char *expect;		// "null", "WxH", or "any" (only: no crash)
    double limitMB;		// the largest allocation allowed after the fix
    const char *expectDecode;	// for the decoder, when not expect
    const char *params;		// QImageIO::setParameters() of the load
    // checked on the decoder's image (load's when there is no decoder):
    int frames;			// frames decoded (0: not checked)
    int pixX, pixY;		// pixel (pixX < 0: not checked)
    QRgb pixel;
    Case() : name( 0 ), what( 0 ), expect( 0 ), limitMB( 64 ), expectDecode( 0 ), params( 0 ),
	     frames( 0 ), pixX( -1 ), pixY( -1 ), pixel( 0 ) {}
};

// GIF, PNG and MNG have a QImageDecoder (incremental, all frames)
static bool hasDecoder( const Bytes &d )
{
    return d.size() >= 4 && ( !memcmp( &d[0], "GIF8", 4 ) || !memcmp( &d[1], "PNG", 3 )
			      || !memcmp( &d[1], "MNG", 3 ) );
}

static std::vector<Case> cases()
{
    std::vector<Case> v;
    Case c;
    // --- GIF
    {	// the 29-byte GIF of the KMail review
	Bytes b; gifHeader( b, 3000, 3000, 0 );
	gifRawImage( b, 20000, 20000, 1, 1, 2, std::vector<int>(), 3 );
	b.resize( b.size() - 1 );	// one block as the review's: clear, 1, end
	put( b, 2 ); put( b, 0x4c ); put( b, 0x01 ); put( b, 0 ); put( b, ';' );
	c.name = "gif-offset-bomb"; c.what = "screen 3000x3000 disbelieved, 1x1 frame at 20000,20000: 20001x20001 (1.6 GB)";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {	// a later frame re-derives the screen beyond the first frame's bitmap
	Bytes b; gifHeader( b, 2010, 2010, 1 );
	gifImage( b, 0, 0, 201, 201, 1, 0 );
	gifImage( b, 60000, 2009, 1, 1, 1, 1 );
	put( b, ';' );
	c.name = "gif-later-frame"; c.what = "2010x2010 bitmap, second frame 1x1 at 60000,2009 (KMail unit qtgif)";
	c.data = b; c.expect = "2010x2010"; c.limitMB = 64; v.push_back( c );
    }
    {	// ... and is disposed with the background (fill at x 60000)
	Bytes b; gifHeader( b, 2010, 2010, 1 );
	gifImage( b, 0, 0, 201, 201, 1, 0 );
	gifGce( b, 2, 10, -1 );
	gifImage( b, 60000, 2009, 1, 1, 1, 1 );
	gifGce( b, 0, 10, -1 );
	gifImage( b, 0, 0, 10, 10, 1, 1 );
	put( b, ';' );
	c.name = "gif-later-frame-restorebg"; c.what = "as gif-later-frame, the frame disposed to the background";
	c.data = b; c.expect = "2010x2010"; c.limitMB = 64; v.push_back( c );
    }
    {	// ... and with the previous image (backing store copied from x 60000)
	Bytes b; gifHeader( b, 2010, 2010, 1 );
	gifImage( b, 0, 0, 201, 201, 1, 0 );
	gifGce( b, 3, 10, -1 );
	gifImage( b, 60000, 1000, 500, 1000, 1, 1 );
	gifGce( b, 0, 10, -1 );
	gifImage( b, 0, 0, 10, 10, 1, 1 );
	put( b, ';' );
	c.name = "gif-later-frame-restoreimage"; c.what = "as gif-later-frame, 500x1000 at 60000,1000 restored to the previous image";
	c.data = b; c.expect = "2010x2010"; c.limitMB = 64; v.push_back( c );
    }
    {	// believed screen of 1 GiB
	Bytes b; gifHeader( b, 16384, 16384, 1 );
	gifRawImage( b, 0, 0, 16384, 16384, 2, std::vector<int>( 1, 5 ), 3 );
	put( b, ';' );
	c.name = "gif-screen-1g"; c.what = "screen and frame 16384x16384 (1 GiB bitmap) from 40 bytes";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {	// largest screen: QImage::create() refuses it (> 2 GiB)
	Bytes b; gifHeader( b, 65535, 65535, 1 );
	gifRawImage( b, 0, 0, 65535, 65535, 2, std::vector<int>( 1, 5 ), 3 );
	put( b, ';' );
	c.name = "gif-screen-max"; c.what = "screen and frame 65535x65535 (16 GiB)";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {	// just above the GIF limit
	Bytes b; gifHeader( b, 8193, 8192, 1 );
	gifRawImage( b, 0, 0, 8193, 8192, 2, std::vector<int>( 1, 5 ), 3 );
	put( b, ';' );
	c.name = "gif-8193x8192"; c.what = "screen and frame 8193x8192 (256 MiB + 32 KiB): one column above the limit";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {	// no bitmap at all, then a disposal of the frame
	Bytes b; gifHeader( b, 0, 0, 1 );
	gifGce( b, 2, 10, -1 );
	gifRawImage( b, 0, 0, 0, 0, 2, std::vector<int>( 1, 5 ), 3 );
	gifGce( b, 0, 10, -1 );
	gifRawImage( b, 0, 0, 0, 0, 2, std::vector<int>( 1, 5 ), 3 );
	put( b, ';' );
	c.name = "gif-empty-dispose"; c.what = "screen 0x0, frame 0x0 disposed to the background (null bitmap)";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {	// interlaced frame right of the image: row duplication of -50 pixels
	Bytes b; gifHeader( b, 100, 100, 2 );
	gifImage( b, 150, 0, 10, 20, 2, 0, true );
	put( b, ';' );
	c.name = "gif-interlace-outside"; c.what = "100x100, interlaced 10x20 frame at 150,0 (memcpy of a negative width)";
	c.data = b; c.expect = "100x100"; c.limitMB = 64; v.push_back( c );
    }
    {	// a cycle in the LZW table: the decoder's stack fills up
	// clear, 259 (first), 258 (KwKwK: table[258] = 259), 0 (table[259] = 258),
	// 258 -> 259 -> 258 -> ... until the stack is full, end
	Bytes b; gifHeader( b, 200, 200, 8 );
	std::vector<int> codes;
	codes.push_back( 256 ); codes.push_back( 259 ); codes.push_back( 258 );
	codes.push_back( 0 ); codes.push_back( 258 ); codes.push_back( 257 );
	gifRawImage( b, 0, 0, 200, 200, 8, codes, 9 );
	put( b, ';' );
	c.name = "gif-lzw-cycle"; c.what = "LZW codes 258 <-> 259 in a cycle: 8192 entries on the decoder stack";
	c.data = b; c.expect = "null"; c.expectDecode = "200x200"; c.limitMB = 64; v.push_back( c );
	c.expectDecode = 0;
    }
    // valid animations the decoder must not break; frames and a pixel of
    // the last image checked (gifImage(): index (x/3 + y/2*5 + seed) % ncols)
    for ( int zeroHeight = 0; zeroHeight < 2; ++zeroHeight ) {
	// an empty frame with "restore previous": QImage::create() of an
	// empty backing store failed and ended the decode
	std::vector<int> empty; empty.push_back( 4 ); empty.push_back( 5 );	// clear, end
	Bytes b; gifHeader( b, 20, 20, 2 ); gifLoop( b );
	gifGce( b, 0, 10, -1 ); gifImage( b, 0, 0, 20, 20, 2, 1 );
	gifGce( b, 3, 10, -1 ); gifRawImage( b, 5, 5, zeroHeight ? 5 : 0, zeroHeight ? 0 : 5, 2, empty, 3 );
	gifGce( b, 0, 10, -1 ); gifImage( b, 2, 2, 10, 10, 2, 2 );
	put( b, ';' );
	c.name = zeroHeight ? "gif-restoreprev-height-0" : "gif-restoreprev-width-0";
	c.what = zeroHeight ? "20x20, second frame 5x0 at 5,5 restored to the previous image, a third frame"
			    : "20x20, second frame 0x5 at 5,5 restored to the previous image, a third frame";
	c.data = b; c.expect = "20x20"; c.frames = 3;
	c.pixX = 3; c.pixY = 2; c.pixel = palColor( 2 );	// third frame
	v.push_back( c );
    }
    {	// a wide screen (>= 2010): the old decoder re-derived the screen from
	// the small second frame and clipped the third frame to its width
	Bytes b; gifHeader( b, 2100, 30, 2 );
	gifGce( b, 1, 10, -1 ); gifImage( b, 0, 0, 2100, 30, 2, 1 );
	gifGce( b, 1, 10, -1 ); gifImage( b, 0, 0, 100, 10, 2, 2 );
	gifGce( b, 1, 10, -1 ); gifImage( b, 0, 0, 2100, 30, 2, 3 );
	put( b, ';' );
	c.name = "gif-wide-small-then-full"; c.what = "2100x30, frames 2100x30, 100x10, 2100x30 (third frame clipped)";
	c.data = b; c.expect = "2100x30"; c.frames = 3;
	c.pixX = 2000; c.pixY = 20; c.pixel = palColor( ( 2000 / 3 + 20 / 2 * 5 + 3 ) % 4 );
	v.push_back( c );
    }
    {	// "restore previous" growing the backing store cleared the bitmap
	Bytes b; gifHeader( b, 40, 40, 2 );
	gifGce( b, 1, 10, -1 ); gifImage( b, 0, 0, 40, 40, 2, 1 );
	gifGce( b, 3, 10, -1 ); gifImage( b, 10, 10, 5, 5, 2, 2 );
	gifGce( b, 1, 10, -1 ); gifImage( b, 30, 30, 2, 2, 2, 3 );
	put( b, ';' );
	c.name = "gif-restoreprev-canvas"; c.what = "40x40, second frame 5x5 at 10,10 restored to the previous image (first frame)";
	c.data = b; c.expect = "40x40"; c.frames = 3;
	c.pixX = 10; c.pixY = 10; c.pixel = palColor( ( 10 / 3 + 10 / 2 * 5 + 1 ) % 4 );
	v.push_back( c );
    }
    c.frames = 0; c.pixX = c.pixY = -1; c.pixel = 0;
    // --- BMP
    {
	Bytes px( 4, 0 );
	c.name = "bmp-clrused-huge"; c.what = "8 bpp 1x1, biClrUsed 0x7fffffff (8 GiB color table)";
	c.data = bmp( 1, 1, 8, 0, 0x7fffffff, px, 0 ); c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes px; put( px, 0 ); put( px, 1 );	// end of bitmap
	c.name = "bmp-rle8-1g"; c.what = "RLE8 34000x34000 (1.1 GiB), 2 bytes of data";
	c.data = bmp( 34000, 34000, 8, 1, 0, px, 256 ); c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes px; put( px, 1 ); put( px, 7 ); put( px, 0 ); put( px, 0 ); put( px, 0 ); put( px, 0 );
	c.name = "bmp-rle8-eol"; c.what = "RLE8 1x1: a pixel, end of line, end of line (jump table read before line 0)";
	c.data = bmp( 1, 1, 8, 1, 0, px, 256 ); c.expect = "1x1"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes px; for ( int i = 0; i < 8; ++i ) put( px, 200 + i );
	c.name = "bmp-index-beyond"; c.what = "8 bpp 8x1, 2 colors, pixels 200..207 (color table read beyond)";
	c.data = bmp( 8, 1, 8, 0, 2, px, 2 ); c.expect = "8x1"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes px; put( px, 0xff ); put( px, 0 ); put( px, 0 ); put( px, 0 );
	c.name = "bmp-1bpp-one-color"; c.what = "1 bpp 8x1, 1 color, pixels 1";
	c.data = bmp( 8, 1, 1, 0, 1, px, 1 ); c.expect = "8x1"; c.limitMB = 64; v.push_back( c );
    }
    // --- PNM
    {
	Bytes b; puts( b, "P6\n17000 17000\n255\n" ); for ( int i = 0; i < 300; ++i ) put( b, i );
	c.name = "ppm-1g"; c.what = "P6 17000x17000 (1.1 GiB), 300 bytes of data";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes b; puts( b, "P5\n16 1\n1\n" ); for ( int i = 0; i < 16; ++i ) put( b, i < 8 ? i & 1 : 200 + i );
	c.name = "pgm-sample-beyond"; c.what = "P5 16x1, maxval 1, samples 1 and 208..215 (color table read beyond)";
	c.data = b; c.expect = "16x1"; c.limitMB = 64; v.push_back( c );
    }
    // --- XPM, XBM
    {
	Bytes b; puts( b, "/* XPM */\nstatic char *x[] = {\n\"2 1 0 -3\",\n\"aaaaaa\"};\n" );
	c.name = "xpm-cpp-negative"; c.what = "XPM with -3 characters per pixel (stack write before a buffer)";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes b; puts( b, "/* XPM */\nstatic char *x[] = {\n\"40000 30000 1 1\",\n\"a c #ff0000\",\n\"a\"};\n" );
	c.name = "xpm-1g"; c.what = "XPM 40000x30000 at 8 bpp (1.1 GiB), one line of pixels";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes b; puts( b, "#define a_width 8\n#define a_height 1\nstatic char a_bits[] = {\n" );
	for ( int i = 0; i < 297; ++i ) put( b, ' ' );
	puts( b, "0x" ); puts( b, "ff };\n" );
	c.name = "xbm-0x-at-buffer-end"; c.what = "XBM, \"0x\" as the last characters of the 300 byte line buffer";
	c.data = b; c.expect = "8x1"; c.limitMB = 64; v.push_back( c );
    }
    {	// the rest of the bitmap was uninitialized heap (ASan fills it with 0xbe)
	Bytes b; puts( b, "#define a_width 16\n#define a_height 4\nstatic char a_bits[] = {\n 0x5a, 0x03 };\n" );
	c.name = "xbm-truncated"; c.what = "XBM 16x4, data for the first line only (last pixel 0, white)";
	c.data = b; c.expect = "16x4"; c.pixX = 15; c.pixY = 3; c.pixel = qRgb( 255, 255, 255 );
	v.push_back( c );
	c.pixX = c.pixY = -1; c.pixel = 0;
    }
    // --- PNG
    {
	Bytes b; const unsigned char sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
	b.insert( b.end(), sig, sig + 8 );
	pngChunk( b, "IHDR", pngIHDR( 17000, 17000, 8, 2 ) );
	Bytes row( 1 + 17000 * 3, 0 );
	pngChunk( b, "IDAT", zlibStart( row ) );
	pngChunk( b, "IEND", Bytes() );
	c.name = "png-1g"; c.what = "PNG RGB 17000x17000 (1.1 GiB as 32 bpp), no image data";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes b; const unsigned char sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
	b.insert( b.end(), sig, sig + 8 );
	// palette: setup_qt() starts libpng's rows before QImage::create()
	pngChunk( b, "IHDR", pngIHDR( 50000, 50000, 8, 3 ) );
	Bytes pal; for ( int i = 0; i < 4; ++i ) { put( pal, i * 60 ); put( pal, 10 ); put( pal, 20 ); }
	pngChunk( b, "PLTE", pal );
	Bytes raw;	// two rows
	for ( int r = 0; r < 2; ++r ) { put( raw, 0 ); for ( int i = 0; i < 50000; ++i ) put( raw, i & 3 ); }
	pngChunk( b, "IDAT", zlibStart( raw ) );
	pngChunk( b, "IEND", Bytes() );
	c.name = "png-rows-no-image"; c.what = "PNG palette 50000x50000 (2.3 GiB, QImage::create() fails), with rows";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    {
	Bytes b; const unsigned char sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
	b.insert( b.end(), sig, sig + 8 );
	pngChunk( b, "IHDR", pngIHDR( 8, 1, 8, 3 ) );
	Bytes pal; for ( int i = 0; i < 2; ++i ) { put( pal, i * 200 ); put( pal, 10 ); put( pal, 20 ); }
	pngChunk( b, "PLTE", pal );
	Bytes raw; put( raw, 0 ); for ( int i = 0; i < 8; ++i ) put( raw, 250 + ( i & 3 ) );
	pngChunk( b, "IDAT", zlibOf( raw ) );
	pngChunk( b, "IEND", Bytes() );
	c.name = "png-index-beyond"; c.what = "PNG palette of 2, indexes 250..253 (color table read beyond)";
	c.data = b; c.expect = "8x1"; c.limitMB = 64; v.push_back( c );
    }
    // --- JPEG (tiny cjpeg output, frame header patched)
    if ( !g_tinyJpeg.empty() ) {
	c.name = "jpeg-baseline-1g"; c.what = "baseline JPEG patched to 17000x17000 (1.1 GiB)";
	c.data = jpegResized( &g_tinyJpeg[0], g_tinyJpeg.size(), 17000, 17000 );
	c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    if ( !g_tinyProgJpeg.empty() ) {
	c.name = "jpeg-progressive-1g"; c.what = "progressive JPEG patched to 17000x17000 (1.1 GiB + coefficients)";
	c.data = jpegResized( &g_tinyProgJpeg[0], g_tinyProgJpeg.size(), 17000, 17000 );
	c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    if ( !g_tinyMultiScanJpeg.empty() ) {
	// libjpeg keeps the whole coefficient image of a sequential JPEG
	// with the components in separate scans too, also for a scaled decode
	c.name = "jpeg-multiscan-scaled-1g";
	c.what = "sequential JPEG, one scan per component, 17000x17000, read with Scale( 64, 64, ScaleFree )";
	c.data = jpegResized( &g_tinyMultiScanJpeg[0], g_tinyMultiScanJpeg.size(), 17000, 17000 );
	c.params = "Scale( 64, 64, ScaleFree )";
	c.expect = "null"; c.limitMB = 64; v.push_back( c );
	c.params = 0;
    }
    // --- MNG
    {
	Bytes b; const unsigned char sig[8] = { 138, 'M', 'N', 'G', 13, 10, 26, 10 };
	b.insert( b.end(), sig, sig + 8 );
	Bytes mhdr; put32be( mhdr, 30000 ); put32be( mhdr, 30000 ); put32be( mhdr, 1000 );
	put32be( mhdr, 0 ); put32be( mhdr, 0 ); put32be( mhdr, 0 ); put32be( mhdr, 0 );
	pngChunk( b, "MHDR", mhdr );
	pngChunk( b, "MEND", Bytes() );
	c.name = "mng-3g"; c.what = "MNG 30000x30000 (3.6 GB, QImage::create() fails)";
	c.data = b; c.expect = "null"; c.limitMB = 64; v.push_back( c );
    }
    return v;
}

static int runCase( const Case &c, bool viaDecoder, const char *tmpdir )
{
    QImage im;
    int frames = 0;
    if ( viaDecoder ) {
	Consumer cons;
	cons.keepLog = false;
	decodeAll( c.data, cons, &im );
	frames = cons.frames;
    } else if ( c.params ) {
	std::string path = std::string( tmpdir ) + "/" + c.name;
	FILE *f = fopen( path.c_str(), "wb" );
	fwrite( &c.data[0], 1, c.data.size(), f );
	fclose( f );
	QImageIO io( QString::fromLatin1( path.c_str() ), 0 );
	io.setParameters( c.params );
	if ( io.read() )
	    im = io.image();
	unlink( path.c_str() );
    } else {
	// through a file, as an application loads one
	std::string path = std::string( tmpdir ) + "/" + c.name;
	FILE *f = fopen( path.c_str(), "wb" );
	fwrite( &c.data[0], 1, c.data.size(), f );
	fclose( f );
	im.load( QString::fromLatin1( path.c_str() ) );
	unlink( path.c_str() );
    }
    touchPixels( im );
    printf( "%s", im.isNull() ? "null" : "" );
    if ( !im.isNull() )
	printf( "%dx%d", im.width(), im.height() );
    // what the image should show, on the decoder's image if there is one
    if ( viaDecoder == hasDecoder( c.data ) ) {
	if ( c.frames && frames != c.frames )
	    printf( ",frames=%d", frames );
	if ( c.pixX >= 0 && !im.isNull() && im.valid( c.pixX, c.pixY )
	     && im.pixel( c.pixX, c.pixY ) != c.pixel )
	    printf( ",pixel=%08x", im.pixel( c.pixX, c.pixY ) );
    }
#ifndef IMGB_ASAN
    printf( " %.1f %.1f", g_largest / 1048576.0, g_peak / 1048576.0 );
#else
    printf( " - -" );
#endif
    printf( "\n" );
    fflush( stdout );
    return 0;
}

static int crafted( const char *only, const char *tmpdir )
{
    std::vector<Case> v = cases();
    int failures = 0;
    printf( "%-29s %-5s %-6s %-11s %9s %9s %8s  %s\n", "case", "bytes", "read", "result",
	    "largestMB", "heapMB", "rssMB", "verdict" );
    for ( size_t i = 0; i < v.size(); ++i ) {
	const Case &c = v[i];
	if ( only && strcmp( only, c.name ) )
	    continue;
	for ( int mode = 0; mode < 2; ++mode ) {
	    if ( mode == 1 && !hasDecoder( c.data ) )
		continue;
	    const char *expect = mode == 1 && c.expectDecode ? c.expectDecode : c.expect;
	    int fds[2];
	    if ( pipe( fds ) )
		return 1;
	    fflush( stdout );
	    pid_t pid = fork();
	    if ( pid == 0 ) {
		close( fds[0] );
		dup2( fds[1], 1 );
		int devnull = open( "/dev/null", O_WRONLY );
		if ( !getenv( "IMGB_STDERR" ) && devnull >= 0 )
		    dup2( devnull, 2 );
#ifndef IMGB_ASAN
		struct rlimit rl = { 6UL << 30, 6UL << 30 };	// keep the machine usable
		setrlimit( RLIMIT_AS, &rl );
#endif
		alarm( 120 );
#ifndef IMGB_ASAN
		g_largest = 0;		// what this case asks for only
		g_peak = g_live;
#endif
		runCase( c, mode == 1, tmpdir );
		_exit( 0 );
	    }
	    close( fds[1] );
	    char out[512] = "";
	    int n = 0, r;
	    while ( n < (int)sizeof out - 1 && ( r = read( fds[0], out + n, sizeof out - 1 - n ) ) > 0 )
		n += r;
	    out[n] = 0;
	    close( fds[0] );
	    int status = 0;
	    struct rusage ru;
	    wait4( pid, &status, 0, &ru );
	    char result[64] = "", largest[32] = "-", heap[32] = "-";
	    sscanf( out, "%63s %31s %31s", result, largest, heap );
	    std::string verdict;
	    bool ok = true;
	    if ( WIFSIGNALED( status ) ) {
		char s[64]; sprintf( s, "signal %d (%s)", WTERMSIG( status ), strsignal( WTERMSIG( status ) ) );
		strcpy( result, "CRASH" ); verdict = s; ok = false;
	    } else if ( WEXITSTATUS( status ) != 0 ) {
		char s[64]; sprintf( s, "exit %d (sanitizer report?)", WEXITSTATUS( status ) );
		strcpy( result, "ERROR" ); verdict = s; ok = false;
	    } else {
		if ( strcmp( expect, "any" ) && strcmp( expect, result ) ) {
		    verdict += std::string( "expected " ) + expect + " ";
		    ok = false;
		}
		if ( strcmp( largest, "-" ) && atof( largest ) > c.limitMB ) {
		    char s[64]; sprintf( s, "allocation > %.0f MB ", c.limitMB );
		    verdict += s; ok = false;
		}
	    }
	    if ( ok )
		verdict = "ok";
	    else
		++failures;
	    printf( "%-29s %5u %-6s %-11s %9s %9s %8.1f  %s\n", c.name, (unsigned)c.data.size(),
		    mode ? "decode" : "load", result, largest, heap, ru.ru_maxrss / 1024.0,
		    ok ? "ok" : ( "FAIL " + verdict ).c_str() );
	    fflush( stdout );
	}
    }
    printf( failures ? "%d FAILED\n" : "all passed\n", failures );
    return failures;
}

// ------------------------------------------------------------ dump
static int dump( int argc, char **argv )
{
    for ( int i = 0; i < argc; ++i ) {
	const char *path = argv[i];
	const char *slash = strrchr( path, '/' );
	printf( "%s\n", slash ? slash + 1 : path );
	QImage im;
	const bool ok = im.load( QString::fromLocal8Bit( path ) );
	printf( "  load %s %s\n", ok ? "ok" : "failed", digest( im ).c_str() );
	Bytes data = readFile( path );
	if ( data.size() >= 4 && ( !memcmp( &data[0], "GIF8", 4 ) || !memcmp( &data[1], "PNG", 3 )
				   || !memcmp( &data[1], "MNG", 3 ) ) ) {
	    Consumer c;
	    QImage last;
	    const int r = decodeAll( data, c, &last );
	    printf( "%s  decoder %s, %d frames, last %s\n", c.log.c_str(), r < 0 ? "error" : "ok",
		    c.frames, digest( last ).c_str() );
	}
	fflush( stdout );
    }
    return 0;
}

// ------------------------------------------------------------ gen
static void writeFile( const char *dir, const char *name, const Bytes &b )
{
    std::string p = std::string( dir ) + "/" + name;
    FILE *f = fopen( p.c_str(), "wb" );
    if ( !f ) { perror( p.c_str() ); exit( 1 ); }
    fwrite( &b[0], 1, b.size(), f );
    fclose( f );
}

static int gen( const char *dir )
{
    {	// single frame
	Bytes b; gifHeader( b, 64, 48, 8 ); gifImage( b, 0, 0, 64, 48, 8, 0 ); put( b, ';' );
	writeFile( dir, "g-single.gif", b );
    }
    {	// animation: frames smaller than the screen at offsets, all disposals
	for ( int t = 0; t < 2; ++t ) {
	    Bytes b; gifHeader( b, 100, 80, 4, 3 ); gifLoop( b );
	    gifGce( b, 1, 20, t ? 2 : -1 ); gifImage( b, 0, 0, 100, 80, 4, 0 );
	    gifGce( b, 2, 30, t ? 5 : -1 ); gifImage( b, 5, 7, 20, 10, 4, 1 );
	    gifGce( b, 3, 40, t ? 1 : -1 ); gifImage( b, 60, 40, 30, 30, 4, 2 );
	    gifGce( b, 0, 50, -1 ); gifImage( b, 33, 22, 17, 9, 4, 3, true );
	    gifGce( b, 2, 5, t ? 0 : -1 ); gifImage( b, 1, 1, 98, 78, 4, 4, false, 3 );
	    gifGce( b, 1, 10, -1 ); gifImage( b, 40, 30, 5, 5, 4, 5 );
	    put( b, ';' );
	    writeFile( dir, t ? "g-anim-trans.gif" : "g-anim.gif", b );
	}
    }
    {	// interlaced, local color table, odd sizes
	Bytes b; gifHeader( b, 37, 29, 0 ); gifImage( b, 0, 0, 37, 29, 5, 7, true, 5 ); put( b, ';' );
	writeFile( dir, "g-interlaced-lct.gif", b );
	Bytes c; gifHeader( c, 5, 3, 2 ); gifImage( c, 0, 0, 5, 3, 2, 1, true ); put( c, ';' );
	writeFile( dir, "g-interlaced-tiny.gif", c );
    }
    {	// first frame smaller than the screen, at an offset, background color
	Bytes b; gifHeader( b, 120, 90, 3, 4 ); gifImage( b, 30, 20, 40, 30, 3, 2 ); put( b, ';' );
	writeFile( dir, "g-offset-first.gif", b );
	Bytes c; gifHeader( c, 120, 90, 3, 4 ); gifGce( c, 0, 10, 6 ); gifImage( c, 30, 20, 40, 30, 3, 2 ); put( c, ';' );
	writeFile( dir, "g-offset-first-trans.gif", c );
    }
    {	// frames partly outside the screen (clipped)
	Bytes b; gifHeader( b, 100, 80, 4 );
	gifImage( b, 0, 0, 100, 80, 4, 0 );
	gifGce( b, 2, 10, -1 ); gifImage( b, 90, 70, 20, 20, 4, 1 );
	gifGce( b, 3, 10, -1 ); gifImage( b, 95, 10, 30, 30, 4, 2, true );
	gifGce( b, 0, 10, -1 ); gifImage( b, 10, 75, 30, 30, 4, 3 );
	put( b, ';' );
	writeFile( dir, "g-partly-outside.gif", b );
    }
    {	// screen 0x0: the frame decides
	Bytes b; gifHeader( b, 0, 0, 2 ); gifImage( b, 3, 4, 30, 20, 2, 1 ); put( b, ';' );
	writeFile( dir, "g-screen-0.gif", b );
    }
    {	// large screens believed or not
	Bytes b; gifHeader( b, 3000, 3000, 1 ); gifImage( b, 900, 700, 1, 1, 1, 1 ); put( b, ';' );
	writeFile( dir, "g-screen-3000-frame-1.gif", b );
	Bytes c; gifHeader( c, 2100, 30, 2 ); gifImage( c, 0, 0, 2100, 30, 2, 1 ); put( c, ';' );
	writeFile( dir, "g-wide-2100.gif", c );
	// a wide animation whose later small frames the old decoder "disbelieved"
	Bytes d; gifHeader( d, 2500, 40, 2 );
	gifImage( d, 0, 0, 2500, 40, 2, 0 );
	gifGce( d, 2, 10, -1 ); gifImage( d, 2400, 5, 10, 10, 2, 1 );
	gifGce( d, 0, 10, -1 ); gifImage( d, 100, 5, 10, 10, 2, 2 );
	put( d, ';' );
	writeFile( dir, "g-wide-small-frames.gif", d );
    }
    {	// large but within the limit: 8192x8192 = 256 MiB
	Bytes b; gifHeader( b, 8192, 8192, 2 ); gifImage( b, 0, 0, 8192, 8192, 2, 0 ); put( b, ';' );
	writeFile( dir, "g-large-8192.gif", b );
    }
    {	// BMP: 1, 4, 8 bpp with palettes, RLE8 with delta
	Bytes p1; for ( int y = 0; y < 5; ++y ) { put( p1, 0xa5 ^ y ); put( p1, 0x3c ); put( p1, 0 ); put( p1, 0 ); }
	writeFile( dir, "b-1bpp.bmp", bmp( 13, 5, 1, 0, 0, p1, 2 ) );
	Bytes p4; for ( int y = 0; y < 3; ++y ) { for ( int i = 0; i < 4; ++i ) put( p4, ( i * 17 + y ) & 0xff ); }
	writeFile( dir, "b-4bpp.bmp", bmp( 7, 3, 4, 0, 16, p4, 16 ) );
	Bytes p8; for ( int y = 0; y < 4; ++y ) { for ( int i = 0; i < 8; ++i ) put( p8, ( i * 31 + y * 7 ) % 40 ); }
	writeFile( dir, "b-8bpp-40.bmp", bmp( 6, 4, 8, 0, 40, p8, 40 ) );
	Bytes r8;
	put( r8, 3 ); put( r8, 5 ); put( r8, 0 ); put( r8, 3 ); put( r8, 1 ); put( r8, 2 ); put( r8, 3 ); put( r8, 0 );
	put( r8, 0 ); put( r8, 0 );			// end of line
	put( r8, 0 ); put( r8, 2 ); put( r8, 2 ); put( r8, 1 );	// delta 2,1
	put( r8, 2 ); put( r8, 9 ); put( r8, 0 ); put( r8, 1 );	// end of bitmap
	writeFile( dir, "b-rle8.bmp", bmp( 8, 4, 8, 1, 16, r8, 16 ) );
	Bytes r4;
	put( r4, 5 ); put( r4, 0x12 ); put( r4, 0 ); put( r4, 4 ); put( r4, 0x34 ); put( r4, 0x56 );
	put( r4, 0 ); put( r4, 0 ); put( r4, 3 ); put( r4, 0x7f ); put( r4, 0 ); put( r4, 1 );
	writeFile( dir, "b-rle4.bmp", bmp( 9, 2, 4, 2, 16, r4, 16 ) );
    }
    {	// PNM
	Bytes a; puts( a, "P2\n3 2\n15\n0 3 15\n7 8 1\n" ); writeFile( dir, "p-ascii.pgm", a );
	Bytes b; puts( b, "P5\n3 2\n255\n" ); for ( int i = 0; i < 6; ++i ) put( b, i * 40 ); writeFile( dir, "p-raw.pgm", b );
	Bytes c; puts( c, "P3\n2 2\n255\n255 0 0 0 255 0 0 0 255 9 9 9\n" ); writeFile( dir, "p-ascii.ppm", c );
	Bytes d; puts( d, "P6\n2 1\n255\n" ); for ( int i = 0; i < 6; ++i ) put( d, i * 50 ); writeFile( dir, "p-raw.ppm", d );
	Bytes e; puts( e, "P1\n5 2\n1 0 1 0 1\n0 1 0 1 0\n" ); writeFile( dir, "p-ascii.pbm", e );
	Bytes f; puts( f, "P4\n9 2\n" ); put( f, 0xa5 ); put( f, 0x80 ); put( f, 0x5a ); put( f, 0 ); writeFile( dir, "p-raw.pbm", f );
    }
    {	// XPM, XBM
	Bytes a; puts( a, "/* XPM */\nstatic const char *x[] = {\n\"4 3 3 1\",\n\". c None\",\n\"# c #ff0000\",\n\"+ c blue\",\n\".#+.\",\n\"##++\",\n\"+..#\"};\n" );
	writeFile( dir, "x-small.xpm", a );
	Bytes b; puts( b, "/* XPM */\nstatic const char *x[] = {\n\"3 2 2 2\",\n\"aa c #102030\",\n\"bb c #a0b0c0\",\n\"aabbaa\",\n\"bbaabb\"};\n" );
	writeFile( dir, "x-cpp2.xpm", b );
	Bytes c; puts( c, "#define t_width 10\n#define t_height 2\nstatic char t_bits[] = {\n 0x5a, 0x03, 0xff, 0x01 };\n" );
	writeFile( dir, "x-small.xbm", c );
    }
    return 0;
}

int main( int argc, char **argv )
{
    QApplication app( argc, argv, FALSE );
    if ( getenv( "IMGB_PLUGINS" ) )
	QApplication::setLibraryPaths( QStringList( QString::fromLocal8Bit( getenv( "IMGB_PLUGINS" ) ) ) );
    const char *jpg = getenv( "IMGB_TINY_JPEG" );
    const char *pjpg = getenv( "IMGB_TINY_PJPEG" );
    const char *mjpg = getenv( "IMGB_TINY_MJPEG" );
    if ( jpg ) g_tinyJpeg = readFile( jpg );
    if ( pjpg ) g_tinyProgJpeg = readFile( pjpg );
    if ( mjpg ) g_tinyMultiScanJpeg = readFile( mjpg );
    if ( argc >= 3 && !strcmp( argv[1], "gen" ) )
	return gen( argv[2] );
    if ( argc >= 3 && !strcmp( argv[1], "write" ) ) {
	std::vector<Case> v = cases();
	for ( size_t i = 0; i < v.size(); ++i )
	    writeFile( argv[2], v[i].name, v[i].data );
	return 0;
    }
    if ( argc >= 2 && !strcmp( argv[1], "crafted" ) )
	return crafted( argc >= 3 ? argv[2] : 0, getenv( "TMPDIR" ) ? getenv( "TMPDIR" ) : "/tmp" );
    if ( argc >= 4 && !strcmp( argv[1], "one" ) ) {	// in this process (for a debugger)
	std::vector<Case> v = cases();
	for ( size_t i = 0; i < v.size(); ++i )
	    if ( !strcmp( v[i].name, argv[2] ) )
		return runCase( v[i], !strcmp( argv[3], "decode" ), "/tmp" );
	return 2;
    }
    if ( argc >= 3 && !strcmp( argv[1], "dump" ) )
	return dump( argc - 2, argv + 2 );
    fprintf( stderr, "usage: %s gen DIR | crafted [CASE] | dump FILE...\n", argv[0] );
    return 2;
}
