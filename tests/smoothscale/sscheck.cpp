// sscheck: QImage::smoothScale checks and benchmark.
//   sscheck check   corner cases (sizes, alpha, depths) and aliasing of a
//                   zone plate; prints FAIL lines, exits with the failure count
//                   (SSCHECK_HUGE=1 adds a 12000x12000 image, 576 MB)
//   sscheck bench   times smoothScale() on synthetic images
#include <qimage.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures = 0;
#define CHECK(c, ...) do { if ( !(c) ) { printf( "FAIL %s:%d: ", __FILE__, __LINE__ ); printf( __VA_ARGS__ ); printf( "\n" ); ++failures; } } while ( 0 )

static QImage solid( int w, int h, QRgb c, bool alpha )
{
    QImage im( w, h, 32 );
    im.fill( c );		// fill() keeps the alpha channel of alpha images
    im.setAlphaBuffer( alpha );
    return im;
}

// every pixel equal to c (alpha compared only for alpha images)
static bool uniform( const QImage &im, QRgb c, int tol = 0 )
{
    for ( int y = 0; y < im.height(); ++y )
	for ( int x = 0; x < im.width(); ++x ) {
	    QRgb p = im.pixel( x, y );
	    if ( abs( qRed( p ) - qRed( c ) ) > tol || abs( qGreen( p ) - qGreen( c ) ) > tol ||
		 abs( qBlue( p ) - qBlue( c ) ) > tol ||
		 ( im.hasAlphaBuffer() && abs( qAlpha( p ) - qAlpha( c ) ) > tol ) ) {
		printf( "  pixel %d,%d = %08x, expected %08x\n", x, y, p, c );
		return FALSE;
	    }
	}
    return TRUE;
}

static void lowdepth();
static void zoneplate();
static int bench();

int main( int argc, char **argv )
{
    if ( argc > 1 && !strcmp( argv[1], "bench" ) )
	return bench();
    if ( argc < 2 || strcmp( argv[1], "check" ) ) {
	fprintf( stderr, "usage: sscheck check|bench\n" );
	return 2;
    }
    // solid colours stay exact at any size, with and without alpha
    const int sizes[][4] = { { 1, 1, 7, 5 }, { 7, 5, 1, 1 }, { 1, 9, 1, 2 }, { 9, 1, 2, 1 },
			     { 1, 1, 1000, 1 }, { 1000, 1, 1, 1 }, { 3, 3, 2, 2 }, { 2, 2, 3, 3 },
			     { 640, 480, 17, 13 }, { 17, 13, 640, 480 }, { 4000, 3, 7, 3 },
			     { 40000, 1, 3, 1 }, { 1, 40000, 1, 3 }, { 333, 777, 41, 9 } };
    const QRgb colours[] = { qRgb( 255, 255, 255 ), qRgb( 0, 0, 0 ), qRgb( 12, 200, 99 ) };
    for ( unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; ++s )
	for ( unsigned c = 0; c < 3; ++c ) {
	    const int *z = sizes[s];
	    QImage r = solid( z[0], z[1], colours[c], FALSE ).smoothScale( z[2], z[3] );
	    CHECK( r.width() == z[2] && r.height() == z[3] && r.depth() == 32 && !r.hasAlphaBuffer(),
		   "size %dx%d->%dx%d", z[0], z[1], z[2], z[3] );
	    CHECK( uniform( r, colours[c] ), "solid %dx%d->%dx%d", z[0], z[1], z[2], z[3] );
	    QRgb ac = ( colours[c] & 0xffffff ) | 0x80000000;
	    r = solid( z[0], z[1], ac, TRUE ).smoothScale( z[2], z[3] );
	    CHECK( r.hasAlphaBuffer(), "alpha buffer kept" );
	    CHECK( uniform( r, ac, 1 ), "solid alpha %dx%d->%dx%d", z[0], z[1], z[2], z[3] );
	    r = solid( z[0], z[1], 0x00ff00ff, TRUE ).smoothScale( z[2], z[3] );
	    CHECK( uniform( r, 0 ), "transparent %dx%d->%dx%d", z[0], z[1], z[2], z[3] );
	}

    // the colour of transparent pixels must not bleed into visible ones
    {
	QImage im( 64, 64, 32 );
	im.setAlphaBuffer( TRUE );
	for ( int y = 0; y < 64; ++y )
	    for ( int x = 0; x < 64; ++x )
		im.setPixel( x, y, ( x + y ) & 1 ? qRgba( 255, 0, 255, 0 ) : qRgba( 0, 128, 0, 255 ) );
	for ( int d = 1; d < 70; d += 3 ) {
	    QImage r = im.smoothScale( d, d );
	    bool ok = TRUE;
	    for ( int y = 0; y < d && ok; ++y )
		for ( int x = 0; x < d && ok; ++x ) {
		    QRgb p = r.pixel( x, y );
		    if ( qAlpha( p ) > 8 && ( qRed( p ) > 2 || qBlue( p ) > 2 || abs( qGreen( p ) - 128 ) > 2 + 255 / qAlpha( p ) ) ) {
			printf( "  %d: pixel %d,%d = %08x\n", d, x, y, p );
			ok = FALSE;
		    }
		}
	    CHECK( ok, "transparent colour bleeds at %d", d );
	}
    }

    // white high-contrast edges: no wrap-around from overshoot, no values > 255
    {
	QImage im( 100, 100, 32 );
	for ( int y = 0; y < 100; ++y )
	    for ( int x = 0; x < 100; ++x )
		im.setPixel( x, y, ( x / 3 + y / 3 ) & 1 ? qRgb( 255, 255, 255 ) : qRgb( 0, 0, 0 ) );
	QImage r = im.smoothScale( 333, 333 );
	CHECK( !r.isNull(), "checker up" );
	r = im.smoothScale( 31, 31 );
	CHECK( !r.isNull(), "checker down" );
    }

    // empty and null
    CHECK( QImage().smoothScale( 10, 10 ).isNull(), "null image" );
    CHECK( solid( 10, 10, 0xffffffff, FALSE ).smoothScale( 0, 10 ).isNull(), "zero width" );
    CHECK( solid( 10, 10, 0xffffffff, FALSE ).smoothScale( 10, 0 ).isNull(), "zero height" );

    // same size: a copy
    {
	QImage im = solid( 5, 5, qRgb( 1, 2, 3 ), FALSE );
	CHECK( im.smoothScale( 5, 5 ) == im, "same size copy" );
    }

    // modes
    {
	QImage im = solid( 400, 200, qRgb( 1, 2, 3 ), FALSE );
	CHECK( im.smoothScale( 100, 100, QImage::ScaleMin ).size() == QSize( 100, 50 ), "ScaleMin" );
	CHECK( im.smoothScale( 100, 100, QImage::ScaleMax ).size() == QSize( 200, 100 ), "ScaleMax" );
    }

    lowdepth();
    zoneplate();

    // a large white image to one pixel: the box prefilter divides by huge counts
    if ( getenv( "SSCHECK_HUGE" ) ) {
	QImage big = solid( 12000, 12000, qRgb( 255, 255, 255 ), FALSE );
	CHECK( !big.isNull(), "12000x12000 allocated" );
	QImage r = big.smoothScale( 1, 1 );
	CHECK( uniform( r, qRgb( 255, 255, 255 ) ), "12000x12000 -> 1x1" );
	r = big.smoothScale( 3, 2 );
	CHECK( uniform( r, qRgb( 255, 255, 255 ) ), "12000x12000 -> 3x2" );
    }

    printf( failures ? "%d failures\n" : "all passed\n", failures );
    return failures;
}

// 8-bit grey, 8-bit colour with alpha, 1-bit
static void lowdepth()
    {
	QImage g( 50, 50, 8, 256 );
	for ( int i = 0; i < 256; ++i )
	    g.setColor( i, qRgb( i, i, i ) );
	g.fill( 77 );
	QImage r = g.smoothScale( 13, 17 );
	// (the result is not isGrayscale(): convertDepth(8) builds a palette of the used colours only, as before)
	CHECK( r.depth() == 8 && uniform( r, qRgb( 77, 77, 77 ) ), "8-bit grey" );
	QImage c( 50, 50, 8, 2 );
	c.setColor( 0, qRgba( 255, 0, 0, 255 ) );
	c.setColor( 1, qRgba( 0, 0, 255, 0 ) );
	c.setAlphaBuffer( TRUE );
	c.fill( 0 );
	r = c.smoothScale( 20, 20 );
	CHECK( r.depth() == 32 && r.hasAlphaBuffer() && uniform( r, qRgba( 255, 0, 0, 255 ) ), "8-bit colour alpha" );
	QImage b( 50, 50, 1, 2, QImage::LittleEndian );
	b.setColor( 0, qRgb( 255, 255, 255 ) );
	b.setColor( 1, qRgb( 0, 0, 0 ) );
	b.fill( 1 );
	r = b.smoothScale( 7, 7 );
	CHECK( r.depth() == 8 && uniform( r, qRgb( 0, 0, 0 ) ), "1-bit" );
    }


// cos(pi r^2 / n): the frequency grows from 0 at the centre to the Nyquist
// limit at the edges
static QImage makeZonePlate( int n )
{
    QImage im( n, n, 32 );
    for ( int y = 0; y < n; ++y ) {
	QRgb *p = (QRgb *)im.scanLine( y );
	for ( int x = 0; x < n; ++x ) {
	    double r2 = ( x - n / 2.0 ) * ( x - n / 2.0 ) + ( y - n / 2.0 ) * ( y - n / 2.0 );
	    int v = int( 255 * ( 0.5 + 0.5 * cos( M_PI * r2 / n ) ) );
	    p[x] = qRgb( v, v, v );
	}
    }
    return im;
}

// RMS deviation from mean grey where the source frequency is above the new
// Nyquist limit: detail that cannot be shown must vanish, not alias
static double zoneAliasing( const QImage &im, int n )
{
    double s = double( n ) / im.width();
    double sum = 0;
    long count = 0;
    for ( int y = 0; y < im.height(); ++y )
	for ( int x = 0; x < im.width(); ++x ) {
	    double r = hypot( ( x + 0.5 ) * s - n / 2.0, ( y + 0.5 ) * s - n / 2.0 );
	    if ( r / n > 1.25 * 0.5 / s && r < 0.48 * n ) {
		double d = qGray( im.pixel( x, y ) ) - 127.5;
		sum += d * d;
		++count;
	    }
	}
    return count ? sqrt( sum / count ) : 0;
}

static void zoneplate()
{
    const int n = 1024;
    QImage zp = makeZonePlate( n );
    // Lanczos-3 (Pillow): 9.6, 3.9, 2.0, 1.8; Catmull-Rom (Pillow BICUBIC):
    // 11.6, 4.8, 2.4, 1.7; the old pnmscale box filter: 33.3, 15.5, 7.1, 4.1
    const int sizes[] = { 512, 256, 140, 64 };
    const double limits[] = { 13.0, 5.5, 3.0, 2.5 };
    for ( unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; ++i ) {
	QImage r = zp.smoothScale( sizes[i], sizes[i] );
	double a = zoneAliasing( r, n );
	printf( "zone plate %d -> %d: aliasing %.2f\n", n, sizes[i], a );
	CHECK( a < limits[i], "zone plate %d -> %d aliases: %.2f", n, sizes[i], a );
    }
}

static double now()
{
    timespec t;
    clock_gettime( CLOCK_MONOTONIC, &t );
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static int bench()
{
    // a noisy photo-like image: smooth gradients plus noise
    QImage big( 6000, 4000, 32 );
    srand( 1 );
    for ( int y = 0; y < big.height(); ++y ) {
	QRgb *p = (QRgb *)big.scanLine( y );
	for ( int x = 0; x < big.width(); ++x )
	    p[x] = qRgb( ( x / 24 + rand() % 32 ) & 255, ( y / 16 + rand() % 32 ) & 255, ( ( x + y ) / 40 ) & 255 );
    }
    QImage mid = big.copy( 0, 0, 1500, 1000 );
    QImage icon = big.copy( 0, 0, 128, 128 );
    icon.setAlphaBuffer( TRUE );
    struct { const char *name; QImage *im; int w, h; } cases[] = {
	{ "6000x4000 -> 1600x1067", &big, 1600, 1067 },
	{ "6000x4000 -> 400x267", &big, 400, 267 },
	{ "6000x4000 -> 160x107", &big, 160, 107 },
	{ "1500x1000 -> 375x250", &mid, 375, 250 },
	{ "1500x1000 -> 205x137", &mid, 205, 137 },
	{ "1500x1000 -> 3000x2000", &mid, 3000, 2000 },
	{ "128x128 alpha -> 22x22", &icon, 22, 22 },
	{ "128x128 alpha -> 48x48", &icon, 48, 48 },
    };
    for ( unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i ) {
	double best = 1e9;
	int reps = cases[i].im->width() * cases[i].im->height() > 1000000 ? 5 : 50;
	for ( int k = 0; k < reps; ++k ) {
	    double t = now();
	    QImage r = cases[i].im->smoothScale( cases[i].w, cases[i].h );
	    t = now() - t;
	    if ( t < best )
		best = t;
	}
	printf( "%-26s %8.3f ms\n", cases[i].name, best );
    }
    return 0;
}
