/****************************************************************************
**
** The FreeType font engine and HarfBuzz shaping (the text path shared by
** the X11 and the Wayland back ends).
**
** QFontEngineFT is the font engine of the Wayland back end, and on X11 the
** glyph rasterizer and HarfBuzz face of each QFontEngineXft (same pattern).
**
** The font is the one fontconfig chooses for the X11 font database's
** pattern (qfontdatabase_x11.cpp, as for Xft). This engine does what
** XftFontOpenPattern() and XftGlyphExtents() do with that pattern, without
** an X server: the same face size, load flags, hinting, LCD filter and
** render mode, the glyph metrics computed as Xft computes them (libXft
** 2.3: xftfreetype.c, XftFontInfoFill/XftFontOpenInfo; xftglyphs.c,
** XftFontLoadGlyphs), and the character map of Xft (the pattern's
** charset, then FcFreeTypeCharIndex). So the glyph ids, the integer
** advances, the metrics and the glyph images are those of QFontEngineXft
** on X11, and the shapers see the same face.
**
** The glyph images are cached here; QPainter (qpainter_wl.cpp) composites
** them through the QGfx as XRender composites Xft's glyphs, and on X11
** QFontEngineXft uploads them to an XRender glyph set. Each glyph is
** cached per horizontal subpixel phase (a quarter of a pixel) of its
** origin, for positions in 26.6 fixed point (fractional positioning).
**
** The faces are shared by the engines of the same (file, index), each
** engine with its own FT_Size. The HarfBuzz part (qt_hb_shape(), the
** engines' HarfBuzz fonts, fractional and integer modes) is described at
** the end of this file.
**
** This file is part of the kde3-revival Qt fork; GPL v2 or v3.
**
****************************************************************************/

#include "qfontengine_p.h"

#ifdef QT_XFT2

#ifdef QT_WL_BACKEND
#include "qplatform_p.h"
#endif
#include "qpainter.h"
#include "qapplication.h"
#include "qtextengine_p.h"
#include "qscriptengine_p.h"
#include <private/qunicodetables_p.h>

#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_SYNTHESIS_H
#include FT_LCD_FILTER_H
#include FT_SIZES_H
#include <hb.h>
#include <hb-ft.h>
#include <hb-ot.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************
  The faces, shared by the engines of a file
 *****************************************************************************/

static FT_Library qt_ft_library = 0;

struct QFtFace
{
    QCString file;
    int index;
    FT_Face face;
    hb_face_t *hbFace;
    int ref;
    QFtFace *next;
};

static QFtFace *qt_ft_faces = 0;

bool qt_ft_init()
{
    if ( !FcInit() )
	return FALSE;
    if ( !qt_ft_library && FT_Init_FreeType( &qt_ft_library ) )
	return FALSE;
    return TRUE;
}

static QFtFace *qt_ft_face_get( const char *file, int index )
{
    for ( QFtFace *f = qt_ft_faces; f; f = f->next ) {
	if ( f->index == index && f->file == file ) {
	    f->ref++;
	    return f;
	}
    }
    if ( !qt_ft_library && FT_Init_FreeType( &qt_ft_library ) )
	return 0;
    FT_Face face;
    if ( FT_New_Face( qt_ft_library, file, index, &face ) )
	return 0;
    QFtFace *f = new QFtFace;
    f->file = file;
    f->index = index;
    f->face = face;
    f->hbFace = 0;
    f->ref = 1;
    f->next = qt_ft_faces;
    qt_ft_faces = f;
    return f;
}

static void qt_ft_face_release( QFtFace *face )
{
    if ( --face->ref )
	return;
    QFtFace **p = &qt_ft_faces;
    while ( *p != face )
	p = &(*p)->next;
    *p = face->next;
    if ( face->hbFace )
	hb_face_destroy( face->hbFace );
    FT_Done_Face( face->face );
    delete face;
}


/*****************************************************************************
  QFontEngineFT
 *****************************************************************************/

#define FLOOR(x)    ((x) & -64)
#define CEIL(x)	    (((x)+63) & -64)
#define TRUNC(x)    ((x) >> 6)
#define ROUND(x)    (((x)+32) & -64)

// every glyph image follows its QFtGlyph
static const int glyphHeaderSize = ( sizeof( QFtGlyph ) + 15 ) & ~15;
// the cache of an engine is emptied beyond this
static const uint maxGlyphMemory = 4 * 1024 * 1024;

QFontEngineFT::QFontEngineFT( FcPattern *pattern, int cmap )
    : _pattern( pattern ), face( 0 ), ftSize( 0 ), charset( 0 ),
      _openType( 0 ), _cmap( cmap ), _scale( 1. ), numPages( 0 ), glyphMemory( 0 ),
      transformed( 0 ), _hbFont( 0 )
{
    memset( pages, 0, sizeof( pages ) );
    lbearing = SHRT_MIN;
    rbearing = SHRT_MIN;
    _ascent = _descent = _height = _maxAdvance = 0;
    cache_cost = 0;

    // XftFontInfoFill(): the file, the size and the rendering settings
    FcChar8 *filename;
    if ( FcPatternGetString( pattern, FC_FILE, 0, &filename ) != FcResultMatch )
	return;
    int id;
    if ( FcPatternGetInteger( pattern, FC_INDEX, 0, &id ) != FcResultMatch )
	id = 0;
    double dsize, aspect;
    if ( FcPatternGetDouble( pattern, FC_PIXEL_SIZE, 0, &dsize ) != FcResultMatch )
	return;
    if ( FcPatternGetDouble( pattern, FC_ASPECT, 0, &aspect ) != FcResultMatch )
	aspect = 1.0;
    ysize = (FT_F26Dot6)( dsize * 64.0 );
    xsize = (FT_F26Dot6)( dsize * aspect * 64.0 );

    FcBool b;
    antialias = FcPatternGetBool( pattern, FC_ANTIALIAS, 0, &b ) == FcResultMatch ? b : TRUE;
    if ( FcPatternGetInteger( pattern, FC_RGBA, 0, &rgba ) != FcResultMatch )
	rgba = FC_RGBA_UNKNOWN;
    if ( FcPatternGetInteger( pattern, FC_LCD_FILTER, 0, &lcdFilter ) != FcResultMatch )
	lcdFilter = FC_LCD_DEFAULT;

    FcMatrix *fm;
    matrix.xx = matrix.yy = 0x10000;
    matrix.xy = matrix.yx = 0;
    if ( FcPatternGetMatrix( pattern, FC_MATRIX, 0, &fm ) == FcResultMatch ) {
	matrix.xx = (FT_Fixed)( 0x10000L * fm->xx );
	matrix.yy = (FT_Fixed)( 0x10000L * fm->yy );
	matrix.xy = (FT_Fixed)( 0x10000L * fm->xy );
	matrix.yx = (FT_Fixed)( 0x10000L * fm->yx );
    }
    for ( int mid = 1; FcPatternGetMatrix( pattern, FC_MATRIX, mid, &fm ) == FcResultMatch; ++mid ) {
	FcMatrix fm1;
	FcMatrixInit( &fm1 );
	fm1.xx = (double)matrix.xx / 0x10000L;
	fm1.yy = (double)matrix.yy / 0x10000L;
	fm1.xy = (double)matrix.xy / 0x10000L;
	fm1.yx = (double)matrix.yx / 0x10000L;
	FcMatrixMultiply( &fm1, fm, &fm1 );
	matrix.xx = (FT_Fixed)( 0x10000L * fm1.xx );
	matrix.yy = (FT_Fixed)( 0x10000L * fm1.yy );
	matrix.xy = (FT_Fixed)( 0x10000L * fm1.xy );
	matrix.yx = (FT_Fixed)( 0x10000L * fm1.yx );
    }
    transform = matrix.xx != 0x10000 || matrix.xy != 0 || matrix.yx != 0 || matrix.yy != 0x10000;

    loadFlags = FT_LOAD_DEFAULT | FT_LOAD_COLOR;
    FcBool bitmap = FcPatternGetBool( pattern, FC_EMBEDDED_BITMAP, 0, &b ) == FcResultMatch ? b : FcFalse;
    if ( ( !bitmap && antialias ) || transform )
	loadFlags |= FT_LOAD_NO_BITMAP;
    FcBool hinting = FcPatternGetBool( pattern, FC_HINTING, 0, &b ) == FcResultMatch ? b : FcTrue;
    embolden = FcPatternGetBool( pattern, FC_EMBOLDEN, 0, &b ) == FcResultMatch ? b : FcFalse;
    int hintStyle;
    if ( FcPatternGetInteger( pattern, FC_HINT_STYLE, 0, &hintStyle ) != FcResultMatch )
	hintStyle = FC_HINT_FULL;
    if ( !hinting || hintStyle == FC_HINT_NONE )
	loadFlags |= FT_LOAD_NO_HINTING;
    if ( antialias ) {
	if ( FC_HINT_NONE < hintStyle && hintStyle < FC_HINT_FULL ) {
	    loadFlags |= FT_LOAD_TARGET_LIGHT;
	} else {
	    switch ( rgba ) {
	    case FC_RGBA_RGB:
	    case FC_RGBA_BGR:
		loadFlags |= FT_LOAD_TARGET_LCD;
		break;
	    case FC_RGBA_VRGB:
	    case FC_RGBA_VBGR:
		loadFlags |= FT_LOAD_TARGET_LCD_V;
		break;
	    }
	}
    } else {
	loadFlags |= FT_LOAD_TARGET_MONO;
    }
    if ( FcPatternGetBool( pattern, FC_VERTICAL_LAYOUT, 0, &b ) == FcResultMatch && b )
	loadFlags |= FT_LOAD_VERTICAL_LAYOUT;
    if ( FcPatternGetBool( pattern, FC_AUTOHINT, 0, &b ) == FcResultMatch && b )
	loadFlags |= FT_LOAD_FORCE_AUTOHINT;
    if ( FcPatternGetBool( pattern, FC_GLOBAL_ADVANCE, 0, &b ) == FcResultMatch && !b )
	loadFlags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;
    if ( FcPatternGetInteger( pattern, FC_SPACING, 0, &spacing ) != FcResultMatch )
	spacing = FC_PROPORTIONAL;
    FcBool minspace = FcPatternGetBool( pattern, FC_MINSPACE, 0, &b ) == FcResultMatch ? b : FcFalse;
    if ( FcPatternGetInteger( pattern, FC_CHAR_WIDTH, 0, &charWidth ) != FcResultMatch )
	charWidth = 0;
    else if ( charWidth )
	spacing = FC_MONO;

    // XftFontOpenInfo(): the face at this size
    face = qt_ft_face_get( (const char *)filename, id );
    if ( !face )
	return;
    FT_Face f = face->face;
    if ( FT_New_Size( f, &ftSize ) || FT_Activate_Size( ftSize ) ) {
	ftSize = 0;
	qt_ft_face_release( face );
	face = 0;
	return;
    }
    int err;
    if ( !( f->face_flags & FT_FACE_FLAG_SCALABLE ) ) {
	// bitmap only faces: the closest strike (height first), as Xft
	int best = 0;
#define Y_SIZE(face,i) ((face)->available_sizes[i].y_ppem)
#define X_SIZE(face,i) ((face)->available_sizes[i].x_ppem)
#define dist(a,b) (QABS((a)-(b)))
	for ( int i = 1; i < f->num_fixed_sizes; i++ ) {
	    if ( dist( ysize, Y_SIZE( f, i ) ) < dist( ysize, Y_SIZE( f, best ) ) ||
		 ( dist( ysize, Y_SIZE( f, i ) ) == dist( ysize, Y_SIZE( f, best ) ) &&
		   dist( xsize, X_SIZE( f, i ) ) < dist( xsize, X_SIZE( f, best ) ) ) )
		best = i;
	}
#undef dist
#undef X_SIZE
#undef Y_SIZE
	err = f->num_fixed_sizes < 1
	      || ( FT_Set_Char_Size( f, f->available_sizes[best].x_ppem,
				     f->available_sizes[best].y_ppem, 0, 0 ) != 0
		   && FT_Set_Char_Size( f, f->available_sizes[best].width << 6,
					f->available_sizes[best].height << 6, 0, 0 ) != 0 );
    } else {
	err = FT_Set_Char_Size( f, xsize, ysize, 0, 0 );
    }
    if ( err ) {
	FT_Done_Size( ftSize );
	ftSize = 0;
	qt_ft_face_release( face );
	face = 0;
	return;
    }

    FcCharSet *cs;
    if ( FcPatternGetCharSet( pattern, FC_CHARSET, 0, &cs ) == FcResultMatch )
	charset = FcCharSetCopy( cs );
    else
	charset = FcFreeTypeCharSet( f, 0 );

    renderAntialias = antialias && ( f->face_flags & FT_FACE_FLAG_SCALABLE );
#ifdef FT_HAS_SVG
    color = FT_HAS_COLOR( f ) && !FT_HAS_SVG( f );
#else
    color = FT_HAS_COLOR( f );
#endif
    renderMode = FT_RENDER_MODE_MONO;
    if ( color )
	renderMode = FT_RENDER_MODE_NORMAL;
    if ( renderAntialias ) {
	switch ( rgba ) {
	case FC_RGBA_RGB:
	case FC_RGBA_BGR:
	    renderMode = FT_RENDER_MODE_LCD;
	    break;
	case FC_RGBA_VRGB:
	case FC_RGBA_VBGR:
	    renderMode = FT_RENDER_MODE_LCD_V;
	    break;
	default:
	    renderMode = FT_RENDER_MODE_NORMAL;
	}
    }

    const FT_Size_Metrics &m = f->size->metrics;
    if ( transform ) {
	FT_Vector v;
	v.x = 0;
	v.y = m.descender;
	FT_Vector_Transform( &v, &matrix );
	_descent = (int)( -( v.y >> 6 ) );
	v.x = 0;
	v.y = m.ascender;
	FT_Vector_Transform( &v, &matrix );
	_ascent = (int)( v.y >> 6 );
	if ( minspace ) {
	    _height = _ascent + _descent;
	} else {
	    v.x = 0;
	    v.y = m.height;
	    FT_Vector_Transform( &v, &matrix );
	    _height = (int)( v.y >> 6 );
	}
    } else {
	_descent = -(int)( m.descender >> 6 );
	_ascent = (int)( m.ascender >> 6 );
	_height = minspace ? _ascent + _descent : (int)( m.height >> 6 );
    }
    if ( charWidth ) {
	_maxAdvance = charWidth;
    } else if ( transform ) {
	FT_Vector v;
	v.x = m.max_advance;
	v.y = 0;
	FT_Vector_Transform( &v, &matrix );
	_maxAdvance = (int)( v.x >> 6 );
    } else {
	_maxAdvance = (int)( m.max_advance >> 6 );
    }

    // as QFontEngineXft with Xft 2 (whatever cmap the database asked for):
    // Xft maps Unicode and Adobe roman, the engine an Adobe custom or MS
    // symbol map
    _cmap = -1;
    for ( int i = 0; i < f->num_charmaps; ++i ) {
	FT_CharMap cm = f->charmaps[i];
	if ( cm->encoding == ft_encoding_adobe_custom
	     || cm->encoding == ft_encoding_symbol ) {
	    _cmap = i;
	    break;
	}
    }

    cache_cost = _height * _maxAdvance * ( f->num_glyphs ? f->num_glyphs : 1024 );
    if ( !antialias )
	cache_cost /= 8;

    numPages = ( (int)f->num_glyphs + 1 + PageSize - 1 ) >> PageBits;
}

QFontEngineFT::~QFontEngineFT()
{
    while ( transformed ) {
	Transformed *t = transformed;
	transformed = t->next;
	delete t->engine;
	delete t;
    }
    if ( _hbFont )
	hb_font_destroy( _hbFont );
    delete _openType;
    freeGlyphs();
    for ( int p = 0; p < Phases; ++p )
	free( pages[p] );
    if ( charset )
	FcCharSetDestroy( (FcCharSet *)charset );
    if ( ftSize )
	FT_Done_Size( ftSize );
    if ( face )
	qt_ft_face_release( face );
    FcPatternDestroy( _pattern );
}

void QFontEngineFT::freeGlyphs()
{
    for ( int p = 0; p < Phases; ++p ) {
	if ( !pages[p] )
	    continue;
	for ( int i = 0; i < numPages; ++i ) {
	    if ( !pages[p][i] )
		continue;
	    for ( int g = 0; g < PageSize; ++g )
		free( pages[p][i][g] );
	    free( pages[p][i] );
	    pages[p][i] = 0;
	}
    }
    glyphMemory = 0;
}

FT_Face QFontEngineFT::lockFace() const
{
    if ( !face )
	return 0;
    FT_Face f = face->face;
    FT_Activate_Size( ftSize );
    FT_Set_Transform( f, transform ? (FT_Matrix *)&matrix : 0, 0 );
    return f;
}

const QFtGlyph *QFontEngineFT::glyph( glyph_t g, bool render, int subpixel )
{
    static const QFtGlyph empty = { 0, 0, 0, 0, 0, 0, QFtGlyph::Gray, 1, 0, 0 };
    // Xft knows the glyphs 0 .. num_glyphs (some fonts count from 1)
    if ( !face || (int)( g >> PageBits ) >= numPages )
	return &empty;
    subpixel &= Phases - 1;
    QFtGlyph ***pp = pages[subpixel];
    if ( pp ) {
	QFtGlyph **page = pp[g >> PageBits];
	if ( page && page[g & ( PageSize - 1 )] )
	    return page[g & ( PageSize - 1 )];
    }
    const QFtGlyph *r = loadGlyph( g, render, subpixel );
    return r ? r : &empty;
}

// renderGlyph(), kept in the cache
QFtGlyph *QFontEngineFT::loadGlyph( glyph_t g, bool, int subpixel )
{
    if ( glyphMemory > maxGlyphMemory )
	freeGlyphs();
    if ( !pages[subpixel] ) {
	pages[subpixel] = (QFtGlyph ***)calloc( numPages, sizeof( QFtGlyph ** ) );
	if ( !pages[subpixel] )
	    return 0;
    }
    QFtGlyph **&page = pages[subpixel][g >> PageBits];
    if ( !page ) {
	page = (QFtGlyph **)calloc( PageSize, sizeof( QFtGlyph * ) );
	if ( !page )
	    return 0;
    }
    QFtGlyph *glyph = renderGlyph( g, subpixel );
    if ( !glyph )
	return 0;
    glyphMemory += glyphHeaderSize + glyph->pitch * glyph->height;
    page[g & ( PageSize - 1 )] = glyph;
    return glyph;
}

/*
  XftFontLoadGlyphs(): the metrics come from the rendered image, so the
  image is always rendered. Not cached: the caller frees it (free()).
*/
QFtGlyph *QFontEngineFT::renderGlyph( glyph_t g, int subpixel )
{
    if ( !face )
	return 0;
    subpixel &= Phases - 1;
    FT_Face f = lockFace();
    FT_Library_SetLcdFilter( qt_ft_library, (FT_LcdFilter)lcdFilter );
    FT_Error error = FT_Load_Glyph( f, g, loadFlags );
    if ( error && ( loadFlags & FT_LOAD_NO_BITMAP ) )
	error = FT_Load_Glyph( f, g, loadFlags & ~FT_LOAD_NO_BITMAP );

    QFtGlyph metrics;
    memset( &metrics, 0, sizeof( metrics ) );
    FT_GlyphSlot slot = f->glyph;
    bool ok = !error;
    if ( ok && embolden )
	FT_GlyphSlot_Embolden( slot );
    const bool rtransform = transform && renderMode != FT_RENDER_MODE_MONO;
    if ( ok && slot->format != FT_GLYPH_FORMAT_BITMAP ) {
	if ( subpixel && slot->format == FT_GLYPH_FORMAT_OUTLINE )
	    FT_Outline_Translate( &slot->outline, subpixel * 64 / Phases, 0 );
	ok = !FT_Render_Glyph( slot, (FT_Render_Mode)renderMode );
    }
    FT_Library_SetLcdFilter( qt_ft_library, FT_LCD_FILTER_NONE );

    if ( ok ) {
	// the advance
	if ( spacing >= FC_MONO ) {
	    if ( rtransform ) {
		FT_Vector v;
		if ( loadFlags & FT_LOAD_VERTICAL_LAYOUT ) {
		    v.x = 0;
		    v.y = -f->size->metrics.max_advance;
		} else {
		    v.x = f->size->metrics.max_advance;
		    v.y = 0;
		}
		FT_Vector_Transform( &v, &matrix );
		metrics.xOff = (short)TRUNC( ROUND( v.x ) );
		metrics.yOff = (short)( -TRUNC( ROUND( v.y ) ) );
	    } else {
		short maxX = (short)_maxAdvance;
		short maxY = (short)-_maxAdvance;
		short x = (short)TRUNC( ROUND( slot->advance.x ) );
		short y = (short)( -TRUNC( ROUND( slot->advance.y ) ) );
		if ( loadFlags & FT_LOAD_VERTICAL_LAYOUT ) {
		    metrics.xOff = 0;
		    metrics.yOff = QMIN( maxY, y );
		} else {
		    metrics.xOff = QMIN( maxX, x );
		    metrics.yOff = 0;
		}
	    }
	} else {
	    metrics.xOff = (short)TRUNC( ROUND( slot->advance.x ) );
	    metrics.yOff = (short)( -TRUNC( ROUND( slot->advance.y ) ) );
	}
    }

    // the image (_compute_xrender_bitmap_size, _fill_xrender_bitmap)
    int width = 0, height = 0, pitch = 0;
    int format = QFtGlyph::Gray;
    const FT_Bitmap &bm = slot->bitmap;
    if ( ok && slot->format == FT_GLYPH_FORMAT_BITMAP ) {
	width = bm.width;
	height = bm.rows;
	const bool lcd = renderMode == FT_RENDER_MODE_LCD || renderMode == FT_RENDER_MODE_LCD_V;
	switch ( bm.pixel_mode ) {
	case FT_PIXEL_MODE_MONO:
	    if ( renderMode == FT_RENDER_MODE_MONO ) {
		format = QFtGlyph::Mono;
		pitch = ( ( width + 31 ) & ~31 ) >> 3;
	    } else if ( lcd ) {
		format = QFtGlyph::Lcd;
		pitch = width * 4;
	    } else {
		format = QFtGlyph::Gray;
		pitch = ( width + 3 ) & ~3;
	    }
	    break;
	case FT_PIXEL_MODE_GRAY:
	    if ( lcd ) {
		format = QFtGlyph::Lcd;
		pitch = width * 4;
	    } else {
		format = QFtGlyph::Gray;
		pitch = ( width + 3 ) & ~3;
	    }
	    break;
	case FT_PIXEL_MODE_BGRA:
	    format = QFtGlyph::Color;
	    pitch = width * 4;
	    break;
	case FT_PIXEL_MODE_LCD:
	    if ( renderMode != FT_RENDER_MODE_LCD ) {
		ok = FALSE;
		break;
	    }
	    format = QFtGlyph::Lcd;
	    width /= 3;
	    pitch = width * 4;
	    break;
	case FT_PIXEL_MODE_LCD_V:
	    if ( renderMode != FT_RENDER_MODE_LCD_V ) {
		ok = FALSE;
		break;
	    }
	    format = QFtGlyph::Lcd;
	    height /= 3;
	    pitch = width * 4;
	    break;
	default:
	    ok = FALSE;
	}
    } else {
	ok = FALSE;
    }
    // Xft leaves the metrics of a glyph it could not load undefined
    if ( !ok ) {
	width = height = pitch = 0;
	format = QFtGlyph::Gray;
    }

    const int size = pitch * height;
    QFtGlyph *glyph = (QFtGlyph *)malloc( glyphHeaderSize + size );
    if ( !glyph )
	return 0;
    *glyph = metrics;
    glyph->width = (unsigned short)width;
    glyph->height = (unsigned short)height;
    glyph->format = (unsigned char)format;
    glyph->rendered = 1;
    glyph->pitch = pitch;
    glyph->data = (unsigned char *)glyph + glyphHeaderSize;
    if ( ok ) {
	// a bitmap glyph of a transformed font is not scaled here (Xft
	// scales the images of color bitmap fonts)
	glyph->x = (short)( -slot->bitmap_left );
	glyph->y = (short)( slot->bitmap_top );

	unsigned char *dst = glyph->data;
	memset( dst, 0, size );
	const unsigned char *src = bm.buffer;
	int spitch = bm.pitch;
	if ( spitch < 0 )
	    src -= spitch * ( (int)bm.rows - 1 );
	const bool bgr = rgba == FC_RGBA_BGR || rgba == FC_RGBA_VBGR;
	for ( int h = 0; h < height; ++h, dst += pitch ) {
	    uint *d32 = (uint *)dst;
	    switch ( bm.pixel_mode ) {
	    case FT_PIXEL_MODE_MONO:
		if ( format == QFtGlyph::Lcd ) {
		    for ( int x = 0; x < width; x++ )
			if ( src[x >> 3] & ( 0x80 >> ( x & 7 ) ) )
			    d32[x] = 0xffffffffU;
		} else if ( format == QFtGlyph::Gray ) {
		    for ( int x = 0; x < width; x++ )
			if ( src[x >> 3] & ( 0x80 >> ( x & 7 ) ) )
			    dst[x] = 0xff;
		} else {
		    memcpy( dst, src, ( width + 7 ) >> 3 );
		}
		src += spitch;
		break;
	    case FT_PIXEL_MODE_GRAY:
		if ( format == QFtGlyph::Lcd ) {
		    for ( int x = 0; x < width; x++ ) {
			uint pix = src[x];
			pix |= pix << 8;
			pix |= pix << 16;
			d32[x] = pix;
		    }
		} else {
		    memcpy( dst, src, width );
		}
		src += spitch;
		break;
	    case FT_PIXEL_MODE_BGRA:
		memcpy( dst, src, width * 4 );
		src += spitch;
		break;
	    case FT_PIXEL_MODE_LCD: {
		const unsigned char *s = src;
		for ( int x = 0; x < width; x++, s += 3 ) {
		    const uint r = bgr ? s[2] : s[0], b = bgr ? s[0] : s[2];
		    d32[x] = ( r << 16 ) | ( (uint)s[1] << 8 ) | b | ( (uint)s[1] << 24 );
		}
		src += spitch;
		break;
	    }
	    default: {			// FT_PIXEL_MODE_LCD_V
		const unsigned char *s = src;
		for ( int x = 0; x < width; x++, s++ ) {
		    const uint r = bgr ? s[spitch * 2] : s[0], b = bgr ? s[0] : s[spitch * 2];
		    d32[x] = ( r << 16 ) | ( (uint)s[spitch] << 8 ) | b | ( (uint)s[spitch] << 24 );
		}
		src += 3 * spitch;
		break;
	    }
	    }
	}
    }
    return glyph;
}

glyph_t QFontEngineFT::charIndex( uint ucs4 ) const
{
    // XftCharIndex(): the pattern's charset, then fontconfig's lookup
    if ( !charset || !FcCharSetHasChar( (FcCharSet *)charset, ucs4 ) )
	return 0;
    return FcFreeTypeCharIndex( face->face, ucs4 );
}

// getAdobeCharIndex() of qfontengine_x11.cpp
static glyph_t qt_ft_adobe_char_index( FT_Face face, int cmap, uint ucs4 )
{
    FT_Set_Charmap( face, face->charmaps[cmap] );
    return FT_Get_Char_Index( face, ucs4 );
}

QFontEngine::Error QFontEngineFT::stringToCMap( const QChar *str, int len, glyph_t *glyphs, advance_t *advances, int *nglyphs, bool mirrored ) const
{
    if ( *nglyphs < len ) {
	*nglyphs = len;
	return OutOfMemory;
    }
    if ( !face ) {
	for ( int i = 0; i < len; ++i )
	    glyphs[i] = 0;
    } else if ( _cmap != -1 ) {
	for ( int i = 0; i < len; ++i ) {
	    unsigned short uc = str[i].unicode();
	    if ( mirrored )
		uc = ::mirroredChar( str[i] ).unicode();
	    glyph_t glyph = charIndex( uc );
	    if ( !glyph )
		glyph = qt_ft_adobe_char_index( face->face, _cmap, uc );
	    glyphs[i] = glyph;
	}
    } else {
	for ( int i = 0; i < len; ++i ) {
	    unsigned short uc = mirrored ? ::mirroredChar( str[i] ).unicode() : str[i].unicode();
	    if ( uc == 0xa0 )
		uc = 0x20;
	    glyphs[i] = charIndex( uc );
	}
    }

    if ( advances ) {
	QFontEngineFT *that = (QFontEngineFT *)this;
	if ( fractional() ) {
	    // HarfBuzz's unhinted advance, rounded
	    hb_font_t *f = that->hbFont();
	    for ( int i = 0; i < len; i++ )
		advances[i] = TRUNC( ROUND( hb_font_get_glyph_h_advance( f, glyphs[i] ) ) );
	} else {
	    for ( int i = 0; i < len; i++ )
		advances[i] = that->glyph( glyphs[i], FALSE )->xOff;
	}
	if ( _scale != 1. ) {
	    for ( int i = 0; i < len; i++ )
		advances[i] = qRound( advances[i] * _scale );
	}
    }

    *nglyphs = len;
    return NoError;
}

void QFontEngineFT::recalcAdvances( int len, glyph_t *glyphs, advance_t *advances )
{
    if ( fractional() ) {
	hb_font_t *f = hbFont();
	for ( int i = 0; i < len; i++ )
	    advances[i] = TRUNC( ROUND( hb_font_get_glyph_h_advance( f, glyphs[i] ) ) );
    } else {
	for ( int i = 0; i < len; i++ )
	    advances[i] = glyph( glyphs[i], FALSE )->xOff;
    }
    if ( _scale != 1. ) {
	for ( int i = 0; i < len; i++ )
	    advances[i] = qRound( advances[i] * _scale );
    }
}

void QFontEngineFT::draw( QPainter *p, int x, int y, const QTextEngine *engine, const QScriptItem *si, int textFlags )
{
#ifdef QT_WL_BACKEND
    // only the Wayland back end creates this engine
    if ( p->testf( QPainter::WlDev ) )
	p->drawGlyphsFT_wl( this, x, y, engine, si, textFlags );
#else
    // X11: only the rasterizer and HarfBuzz face of a QFontEngineXft
    Q_UNUSED( p ); Q_UNUSED( x ); Q_UNUSED( y ); Q_UNUSED( engine );
    Q_UNUSED( si ); Q_UNUSED( textFlags );
#endif
}

glyph_metrics_t QFontEngineFT::boundingBox( const glyph_t *glyphs, const advance_t *advances, const qoffset_t *offsets, int numGlyphs )
{
    glyph_metrics_t overall;
    int ymax = 0;
    int xmax = 0;
    if ( _scale != 1 ) {
	for ( int i = 0; i < numGlyphs; i++ ) {
	    const QFtGlyph *gi = glyph( glyphs[i], FALSE );
	    int x = overall.xoff + offsets[i].x - gi->x;
	    int y = overall.yoff + offsets[i].y - gi->y;
	    overall.x = QMIN( overall.x, x );
	    overall.y = QMIN( overall.y, y );
	    xmax = QMAX( xmax, x + gi->width );
	    ymax = QMAX( ymax, y + gi->height );
	    overall.xoff += qRound( advances[i] / _scale );
	}
	overall.x = qRound( overall.x * _scale );
	overall.y = qRound( overall.y * _scale );
	overall.xoff = qRound( overall.xoff * _scale );
	overall.yoff = qRound( overall.yoff * _scale );
	xmax = qRound( xmax * _scale );
	ymax = qRound( ymax * _scale );
    } else {
	for ( int i = 0; i < numGlyphs; i++ ) {
	    const QFtGlyph *gi = glyph( glyphs[i], FALSE );
	    int x = overall.xoff + offsets[i].x - gi->x;
	    int y = overall.yoff + offsets[i].y - gi->y;
	    overall.x = QMIN( overall.x, x );
	    overall.y = QMIN( overall.y, y );
	    xmax = QMAX( xmax, x + gi->width );
	    ymax = QMAX( ymax, y + gi->height );
	    overall.xoff += advances[i];
	}
    }
    overall.height = ymax - overall.y;
    overall.width = xmax - overall.x;
    return overall;
}

glyph_metrics_t QFontEngineFT::boundingBox( glyph_t g )
{
    const QFtGlyph *gi = glyph( g, FALSE );
    glyph_metrics_t gm = glyph_metrics_t( -gi->x, -gi->y, gi->width, gi->height, gi->xOff, -gi->yOff );
    if ( _scale != 1. ) {
	gm.x = qRound( gm.x * _scale );
	gm.y = qRound( gm.y * _scale );
	gm.height = qRound( gm.height * _scale );
	gm.width = qRound( gm.width * _scale );
	gm.xoff = qRound( gm.xoff * _scale );
	gm.yoff = qRound( gm.yoff * _scale );
    }
    return gm;
}

int QFontEngineFT::ascent() const
{
    return qRound( _ascent * _scale );
}

int QFontEngineFT::descent() const
{
    return qRound( ( _descent - 1 ) * _scale );
}

int QFontEngineFT::leading() const
{
    int l = qRound( QMIN( _height - ( _ascent + _descent ),
			  ( ( _ascent + _descent ) >> 4 ) * _scale ) );
    return ( l > 0 ) ? l : 1;
}

int QFontEngineFT::lineThickness() const
{
    // the ad hoc algorithm of QFontEngineXft
    int score = fontDef.weight * fontDef.pixelSize;
    int lw = score / 700;
    if ( lw < 2 && score >= 1050 ) lw = 2;
    if ( lw == 0 ) lw = 1;
    return lw;
}

int QFontEngineFT::underlinePosition() const
{
    int pos = ( ( lineThickness() * 2 ) + 3 ) / 6;
    return pos ? pos : 1;
}

int QFontEngineFT::maxCharWidth() const
{
    return qRound( _maxAdvance * _scale );
}

// the characters QFontEngineXft::minRightBearing() measures
static const ushort qt_ft_bearing_chars[] = {
    40, 67, 70, 75, 86, 88, 89, 91, 102, 114, 124, 127, 205, 645, 884, 922, 1070, 12386
};

int QFontEngineFT::minLeftBearing() const
{
    if ( lbearing == SHRT_MIN )
	minRightBearing(); // calculates both
    return lbearing;
}

int QFontEngineFT::minRightBearing() const
{
    if ( rbearing == SHRT_MIN ) {
	const int n = sizeof( qt_ft_bearing_chars ) / sizeof( ushort );
	QFontEngineFT *that = (QFontEngineFT *)this;
	that->lbearing = that->rbearing = 0;
	QChar *ch = (QChar *)qt_ft_bearing_chars;
	glyph_t glyphs[n];
	int ng = n;
	stringToCMap( ch, n, glyphs, 0, &ng, FALSE );
	while ( --ng ) {
	    if ( glyphs[ng] ) {
		glyph_metrics_t gi = that->boundingBox( glyphs[ng] );
		if ( gi.xoff ) {
		    that->lbearing = QMIN( lbearing, gi.x );
		    that->rbearing = QMIN( rbearing, gi.xoff - gi.x - gi.width );
		}
	    }
	}
    }
    return rbearing;
}

int QFontEngineFT::cmap() const
{
    return _cmap;
}

const char *QFontEngineFT::name() const
{
    return "ft";
}

void QFontEngineFT::setScale( double scale )
{
    _scale = scale;
}

bool QFontEngineFT::canRender( const QChar *string, int len )
{
    if ( !charset )
	return FALSE;
    for ( int i = 0; i < len; i++ ) {
	const uint uc = string[i].unicode();
	if ( !FcCharSetHasChar( (FcCharSet *)charset, uc )
	     && ( _cmap == -1 || qt_ft_adobe_char_index( face->face, _cmap, uc ) == 0 ) )
	    return FALSE;
    }
    return TRUE;
}

QOpenType *QFontEngineFT::openType() const
{
    if ( _openType )
	return _openType;
    if ( !face || !FT_IS_SFNT( face->face ) )
	return 0;
    QFontEngineFT *that = (QFontEngineFT *)this;
    that->_openType = new QOpenType( that, face->face );
    return _openType;
}

QFontEngine::Type QFontEngineFT::type() const
{
    return Freetype;
}

QFontEngineFT *QFontEngineFT::transformedEngine( double xx, double xy, double yx, double yy )
{
    // as the transformed fonts of QFontEngineXft::draw(): at most 10
    // besides the last one used, the last one used first
    Transformed *t = transformed;
    Transformed *prev = 0;
    int i = 0;
    while ( t ) {
	if ( t->xx == xx && t->xy == xy && t->yx == yx && t->yy == yy )
	    break;
	Transformed *tmp = t;
	t = t->next;
	if ( i > 10 ) {
	    delete tmp->engine;
	    delete tmp;
	    prev->next = t;
	} else {
	    prev = tmp;
	}
	++i;
    }
    if ( t ) {
	if ( prev ) {
	    prev->next = t->next;
	    t->next = transformed;
	    transformed = t;
	}
	return t->engine;
    }

    FcMatrix m2;
    m2.xx = xx;
    m2.xy = xy;
    m2.yx = yx;
    m2.yy = yy;
    FcMatrix *mat = 0;
    if ( FcPatternGetMatrix( _pattern, FC_MATRIX, 0, &mat ) == FcResultMatch && mat )
	FcMatrixMultiply( &m2, &m2, mat );
    FcPattern *pattern = FcPatternDuplicate( _pattern );
    FcPatternDel( pattern, FC_MATRIX );
    FcPatternAddMatrix( pattern, FC_MATRIX, &m2 );
    QFontEngineFT *fe = new QFontEngineFT( pattern, _cmap );
    if ( !fe->isValid() ) {
	delete fe;
	return 0;
    }
    fe->fontDef = fontDef;
    t = new Transformed;
    t->xx = xx;
    t->xy = xy;
    t->yx = yx;
    t->yy = yy;
    t->engine = fe;
    t->next = transformed;
    transformed = t;
    return fe;
}



/*****************************************************************************
  HarfBuzz (stage 4 of the text plan, "QHbFont": the shared text path of
  both platforms)

  Every item is shaped by HarfBuzz (qt_hb_shape(), called by
  QTextEngine::shape()) with the HarfBuzz font of its engine: on X11 the
  QFontEngineXft, whose FreeType engine (same pattern) gives the face, on
  Wayland the QFontEngineFT itself. The face of the HarfBuzz font is the
  FreeType engine's own FT_Face (hb_ft_face_create_referenced), not Xft's:
  Xft shares one FT_Face between all sizes of a file and sets its size
  whenever it locks it, so an FT_Size of our own is the only stable one.

  Two modes per font:
  - fractional (QFontEngineFT::fractional()): HarfBuzz's own unhinted
    advances and offsets in 26.6 (the face's metrics at the pixel size);
    QTextEngine keeps them (qfixedpos_t) and the integer advances are
    their rounded running sums; the engines draw each glyph at its exact
    position rounded to a quarter pixel, with the glyph image of that
    subpixel phase. kern, liga, clig, calt are on (HarfBuzz's defaults).
  - integer (fixed pitch, bitmap, transformed, emboldened or color fonts,
    and QT3_SUBPIXEL_POSITIONING=0): the advances are Xft's hinted integer
    ones, positions are rounded per glyph. Fixed pitch fonts (FC_SPACING
    dual, mono or charcell) get no kerning and no ligatures (liga, clig,
    calt, dlig off): Konsole and editors keep the character grid.

  Hinting: with fractional positioning the horizontal hinting of the glyph
  images should be light (fontconfig hintstyle "hintslight", the default
  on most distributions); with "hintfull" the x-hinted glyph shapes are
  still drawn at fractional positions.

  QT3_HARFBUZZ=0 switches HarfBuzz off (Qt's own shapers and Xft's
  advances, as before), QT3_SUBPIXEL_POSITIONING=0 keeps HarfBuzz with
  integer positions for all fonts.
 *****************************************************************************/

int qt_hb_mode()
{
    static int mode = -1;
    if ( mode < 0 ) {
	const char *e = getenv( "QT3_HARFBUZZ" );
	if ( e && *e == '0' ) {
	    mode = 0;
	} else {
	    e = getenv( "QT3_SUBPIXEL_POSITIONING" );
	    mode = ( e && *e == '0' ) ? 1 : 2;
	}
    }
    return mode;
}

bool QFontEngineFT::fractional() const
{
    return face && qt_hb_mode() == 2 && renderAntialias && !transform && !color
	&& !embolden && spacing < FC_DUAL && !( loadFlags & FT_LOAD_VERTICAL_LAYOUT );
}

bool QFontEngineFT::fixedPitch() const
{
    return spacing >= FC_DUAL;
}

int QFontEngineFT::imageFormat() const
{
    if ( color )
	return QFtGlyph::Color;
    switch ( renderMode ) {
    case FT_RENDER_MODE_MONO:
	return QFtGlyph::Mono;
    case FT_RENDER_MODE_LCD:
    case FT_RENDER_MODE_LCD_V:
	return QFtGlyph::Lcd;
    default:
	return QFtGlyph::Gray;
    }
}

struct QHbFontData
{
    QFontEngine *owner;		// the character map (and integer advances)
    QFontEngineFT *ft;		// the face
    uint id;			// unique: the key of the word cache
    int spaceFree;		// -1: not known yet; 1: no lookup of the
				// font involves the space glyph
    // the shape plans of the words (Latin, Greek, Cyrillic), made once
    hb_shape_plan_t *plans[3];
    struct QHbFast *fast;	// the Latin fast path (qt_hb_fast_segment())
};

static uint qt_hb_font_ids = 0;
static hb_user_data_key_t qt_hb_data_key;

static void qt_hb_free_fast( QHbFontData *d );

// the data of the last font asked for (hb_font_get_user_data() locks)
static hb_font_t *qt_hb_last_font = 0;
static QHbFontData *qt_hb_last_data = 0;

static QFontEngine *qt_hb_last_engine = 0;	// qt_hb_engine_id()
static uint qt_hb_last_engine_id = 0;

static void qt_hb_font_data_destroy( void *data )
{
    if ( data == qt_hb_last_data ) {
	qt_hb_last_font = 0;
	qt_hb_last_data = 0;
    }
    if ( ( (QHbFontData *)data )->id == qt_hb_last_engine_id ) {
	qt_hb_last_engine = 0;
	qt_hb_last_engine_id = 0;
    }
    QHbFontData *d = (QHbFontData *)data;
    for ( int i = 0; i < 3; ++i )
	if ( d->plans[i] )
	    hb_shape_plan_destroy( d->plans[i] );
    qt_hb_free_fast( d );
    delete d;
}

static inline QHbFontData *qt_hb_font_data( hb_font_t *font )
{
    if ( font != qt_hb_last_font ) {
	qt_hb_last_data = (QHbFontData *)hb_font_get_user_data( font, &qt_hb_data_key );
	qt_hb_last_font = font;
    }
    return qt_hb_last_data;
}

static hb_bool_t qt_hb_nominal_glyph( hb_font_t *, void *font_data, hb_codepoint_t unicode,
				      hb_codepoint_t *glyph, void * )
{
    const QHbFontData *d = (const QHbFontData *)font_data;
    glyph_t g = 0;
    if ( unicode < 0x10000 ) {
	// the engine's character map, as Qt's own shapers
	QChar c( (ushort)unicode );
	int n = 1;
	d->owner->stringToCMap( &c, 1, &g, 0, &n, FALSE );
    } else {
	// Qt's engines map UTF-16 code units; Xft's map for the others
	g = d->ft->charIndex( unicode );
    }
    *glyph = g;
    return g != 0;
}

static hb_position_t qt_hb_integer_h_advance( hb_font_t *, void *font_data, hb_codepoint_t glyph, void * )
{
    // integer mode: the hinted integer advance of Xft, in 26.6
    const QHbFontData *d = (const QHbFontData *)font_data;
    if ( d->owner->type() == QFontEngine::Xft ) {
	glyph_t g = (glyph_t)glyph;
	advance_t a = 0;
	((QFontEngineXft *)d->owner)->recalcAdvances( 1, &g, &a );
	return a * 64;
    }
    return d->ft->glyph( glyph, FALSE )->xOff * 64;
}

static hb_bool_t qt_hb_integer_extents( hb_font_t *, void *font_data, hb_codepoint_t glyph,
					hb_glyph_extents_t *extents, void * )
{
    // integer mode: the ink box of the hinted glyph (Xft's)
    const QHbFontData *d = (const QHbFontData *)font_data;
    glyph_metrics_t gm = d->owner->boundingBox( (glyph_t)glyph );
    extents->x_bearing = gm.x * 64;
    extents->y_bearing = -gm.y * 64;
    extents->width = gm.width * 64;
    extents->height = -gm.height * 64;
    return TRUE;
}

static hb_font_funcs_t *qt_hb_funcs( bool fractional )
{
    static hb_font_funcs_t *funcs[2] = { 0, 0 };
    hb_font_funcs_t *&f = funcs[fractional ? 1 : 0];
    if ( !f ) {
	f = hb_font_funcs_create();
	hb_font_funcs_set_nominal_glyph_func( f, qt_hb_nominal_glyph, 0, 0 );
	if ( !fractional ) {
	    hb_font_funcs_set_glyph_h_advance_func( f, qt_hb_integer_h_advance, 0, 0 );
	    hb_font_funcs_set_glyph_extents_func( f, qt_hb_integer_extents, 0, 0 );
	}
	hb_font_funcs_make_immutable( f );
    }
    return f;
}

hb_font_t *QFontEngineFT::createHbFont( QFontEngine *owner, bool fractional )
{
    if ( !face )
	return 0;
    if ( !face->hbFace )
	face->hbFace = hb_ft_face_create_referenced( face->face );
    // the parent: HarfBuzz's own OpenType functions at the pixel size in
    // 26.6 (advances, extents, GPOS, anchors); on top of it the owner's
    // character map and, in integer mode, its advances and extents
    hb_font_t *parent = hb_font_create( face->hbFace );
    hb_font_set_scale( parent, (int)xsize, (int)ysize );
    hb_font_set_ppem( parent, (uint)( xsize >> 6 ), (uint)( ysize >> 6 ) );
    // a variable font: the named instance fontconfig chose (FC_INDEX >> 16,
    // which FreeType and Xft open)
    const int instance = (int)( face->face->face_index >> 16 );
    if ( instance > 0 )
	hb_font_set_var_named_instance( parent, (uint)( instance - 1 ) );
    hb_font_t *font = hb_font_create_sub_font( parent );
    hb_font_destroy( parent );
    QHbFontData *d = new QHbFontData;
    d->owner = owner ? owner : this;
    d->ft = this;
    d->id = ++qt_hb_font_ids;
    d->spaceFree = -1;
    d->plans[0] = d->plans[1] = d->plans[2] = 0;
    d->fast = 0;
    hb_font_set_funcs( font, qt_hb_funcs( fractional ), d, qt_hb_font_data_destroy );
    hb_font_set_user_data( font, &qt_hb_data_key, d, 0, FALSE );
    return font;
}

hb_font_t *QFontEngineFT::hbFont()
{
    if ( !_hbFont && face )
	_hbFont = createHbFont( this, fractional() );
    return _hbFont;
}

// the features of fixed pitch fonts: no kerning, no ligatures
static const hb_feature_t *qt_hb_fixed_pitch_features( unsigned int *count )
{
    static hb_feature_t features[5];
    static bool init = FALSE;
    if ( !init ) {
	static const char * const names[5] = { "-kern", "-liga", "-clig", "-calt", "-dlig" };
	for ( int i = 0; i < 5; ++i )
	    hb_feature_from_string( names[i], -1, &features[i] );
	init = TRUE;
    }
    *count = 5;
    return features;
}

static inline bool qt_hb_invisible( ushort uc )
{
    // what HarfBuzz draws as an invisible zero width glyph (default
    // ignorables), as Qt's qIsZeroWidthChar() and the soft hyphen
    return qIsZeroWidthChar( uc ) || uc == 0xad || uc == 0xfeff || ( uc >= 0x2060 && uc <= 0x2064 );
}

/*
  The word cache. Shaping is the expensive part of laying out text (a few
  microseconds per string, mostly the kerning lookups), and the same words
  come again and again (a repaint draws the same strings, KHTML measures
  every word). As browsers do, an item of a simple script (Latin, Greek,
  Cyrillic, symbols) in left-to-right order is shaped word by word, the
  spaces apart, if no OpenType lookup of the font involves the space glyph
  (then shaping cannot cross a space); each word's glyphs are cached per
  HarfBuzz font. Direct mapped, replaced on collision.
*/
struct QHbGlyph
{
    uint glyph;
    int cluster;		// the character, from the start of the text
    int advance;		// 26.6, as HarfBuzz gives them
    int x;
    int y;			// HarfBuzz's y (up)
};

struct QHbWord
{
    uint font;			// QHbFontData::id, 0: empty
    uint hash;
    uint stamp;			// last use (the older of a set is replaced)
    int script;			// the item's script (the shape plan)
    int length;			// characters
    int count;			// glyphs
    ushort *text;		// the characters, then the glyphs (QHbGlyph)
};

// 4096 sets of two words
enum { QHbWordCacheSize = 8192, QHbMaxWordLength = 48 };
static QHbWord *qt_hb_words = 0;
static uint qt_hb_word_stamp = 0;

static void qt_hb_free_words()
{
    if ( !qt_hb_words )
	return;
    for ( int i = 0; i < QHbWordCacheSize; ++i )
	free( qt_hb_words[i].text );
    free( qt_hb_words );
    qt_hb_words = 0;
}

static inline QHbGlyph *qt_hb_word_glyphs( const QHbWord &w )
{
    return (QHbGlyph *)( (char *)w.text + ( ( w.length * sizeof( ushort ) + 7 ) & ~7 ) );
}

// how the lookups of the simple scripts (Latin, Cyrillic, Greek, the
// default script) involve the space glyph: 1 not at all (a word can be
// shaped alone), 2 only in GPOS (kerning with a space: a word is shaped
// with its spaces and the next character), 0 in GSUB (no word shaping)
static int qt_hb_space_mode( hb_font_t *font, QHbFontData *d )
{
    if ( d->spaceFree >= 0 )
	return d->spaceFree;
    d->spaceFree = 1;
    hb_codepoint_t space = 0;
    if ( !hb_font_get_nominal_glyph( font, 0x20, &space ) )
	return d->spaceFree;
    hb_face_t *face = hb_font_get_face( font );
    static const hb_tag_t scripts[] = {
	HB_TAG( 'l', 'a', 't', 'n' ), HB_TAG( 'c', 'y', 'r', 'l' ), HB_TAG( 'g', 'r', 'e', 'k' ),
	HB_TAG( 'D', 'F', 'L', 'T' ), HB_TAG_NONE
    };
    hb_set_t *lookups = hb_set_create();
    hb_set_t *before = hb_set_create();
    hb_set_t *input = hb_set_create();
    hb_set_t *after = hb_set_create();
    const hb_tag_t tables[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
    for ( int t = 0; t < 2 && d->spaceFree; ++t ) {
	hb_set_clear( lookups );
	hb_ot_layout_collect_lookups( face, tables[t], scripts, 0, 0, lookups );
	hb_codepoint_t l = HB_SET_VALUE_INVALID;
	while ( hb_set_next( lookups, &l ) ) {
	    hb_set_clear( before );
	    hb_set_clear( input );
	    hb_set_clear( after );
	    hb_ot_layout_lookup_collect_glyphs( face, tables[t], l, before, input, after, 0 );
	    if ( hb_set_has( before, space ) || hb_set_has( input, space ) || hb_set_has( after, space ) ) {
		d->spaceFree = t == 0 ? 0 : 2;
		if ( t == 0 )
		    break;
	    }
	}
    }
    hb_set_destroy( lookups );
    hb_set_destroy( before );
    hb_set_destroy( input );
    hb_set_destroy( after );
    // HarfBuzz applies an old 'kern' table if GPOS has no kerning: it may
    // have pairs with the space (not looked at: as if it had)
    if ( d->spaceFree == 1 && !hb_ot_layout_has_positioning( face ) ) {
	hb_blob_t *kern = hb_face_reference_table( face, HB_TAG( 'k', 'e', 'r', 'n' ) );
	if ( hb_blob_get_length( kern ) )
	    d->spaceFree = 2;
	hb_blob_destroy( kern );
    }
    return d->spaceFree;
}

// the scratch buffers: the glyphs of an item, the HarfBuzz buffer (like
// the word cache and qt_hb_last_font: text is laid out in one thread, the
// GUI thread, as for QFontCache)
static QHbGlyph *qt_hb_glyphs = 0;
static int qt_hb_glyphs_alloc = 0;
static hb_buffer_t *qt_hb_buffer = 0;

static QHbGlyph *qt_hb_seg;
static int qt_hb_seg_alloc;

static void qt_hb_free_items();

static void qt_hb_cleanup()
{
    qt_hb_free_items();
    free( qt_hb_seg );
    qt_hb_seg = 0;
    qt_hb_seg_alloc = 0;
    qt_hb_free_words();
    free( qt_hb_glyphs );
    qt_hb_glyphs = 0;
    qt_hb_glyphs_alloc = 0;
    if ( qt_hb_buffer )
	hb_buffer_destroy( qt_hb_buffer );
    qt_hb_buffer = 0;
}

static bool qt_hb_reserve( int n )
{
    if ( n <= qt_hb_glyphs_alloc )
	return TRUE;
    int a = QMAX( n, 2 * qt_hb_glyphs_alloc );
    QHbGlyph *g = (QHbGlyph *)realloc( qt_hb_glyphs, a * sizeof( QHbGlyph ) );
    if ( !g )
	return FALSE;
    qt_hb_glyphs = g;
    qt_hb_glyphs_alloc = a;
    return TRUE;
}

// shapes text[from, from + len) (the rest of text is context) and appends
// its glyphs, clusters relative to clusterBase, to qt_hb_glyphs from *n on
static bool qt_hb_run( hb_font_t *font, const QChar *text, int textLength, int from, int len,
		       int flags, bool rtl, bool directionFromScript, const hb_feature_t *features,
		       unsigned int nfeatures, int clusterBase, int *n, bool *rtlOut,
		       hb_script_t script = HB_SCRIPT_INVALID, hb_shape_plan_t **plan = 0 )
{
    hb_buffer_t *buffer = qt_hb_buffer;
    hb_buffer_clear_contents( buffer );
    hb_buffer_add_utf16( buffer, (const uint16_t *)text, textLength, from, len );
    if ( !directionFromScript )
	hb_buffer_set_direction( buffer, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR );
    if ( script != HB_SCRIPT_INVALID )
	hb_buffer_set_script( buffer, script );
    hb_buffer_guess_segment_properties( buffer );
    if ( directionFromScript )
	rtl = hb_buffer_get_direction( buffer ) == HB_DIRECTION_RTL;
    hb_buffer_set_flags( buffer, (hb_buffer_flags_t)flags );
    if ( plan ) {
	// the plan of these properties and features, made once
	if ( !*plan ) {
	    hb_segment_properties_t props;
	    hb_buffer_get_segment_properties( buffer, &props );
	    *plan = hb_shape_plan_create_cached( hb_font_get_face( font ), &props, features, nfeatures, 0 );
	}
	hb_shape_plan_execute( *plan, font, buffer, features, nfeatures );
    } else {
	hb_shape( font, buffer, features, nfeatures );
    }
    // Qt keeps the glyphs of an item in logical order
    if ( rtl )
	hb_buffer_reverse( buffer );
    const int count = (int)hb_buffer_get_length( buffer );
    if ( !qt_hb_reserve( *n + count ) )
	return FALSE;
    const hb_glyph_info_t *info = hb_buffer_get_glyph_infos( buffer, 0 );
    const hb_glyph_position_t *pos = hb_buffer_get_glyph_positions( buffer, 0 );
    QHbGlyph *g = qt_hb_glyphs + *n;
    for ( int i = 0; i < count; ++i ) {
	g[i].glyph = info[i].codepoint;
	g[i].cluster = (int)info[i].cluster - from + clusterBase;
	g[i].advance = pos[i].x_advance;
	g[i].x = pos[i].x_offset;
	g[i].y = pos[i].y_offset;
    }
    *n += count;
    if ( rtlOut )
	*rtlOut = rtl;
    return TRUE;
}

static inline bool qt_hb_word_script( int script )
{
    return script <= QFont::CombiningMarks
	|| ( script >= QFont::CurrencySymbols && script <= QFont::Braille );
}

/*
  Snapping at spaces. The integer advances are the rounded running sums of
  the exact ones, but at a space (before it and before the first character
  after it) the exact pen position is moved to the integer one: then the
  integer width of a word, or of a space, is the same alone as in a line
  (its exact width rounded), the widths of the words and spaces of a line
  add up to the line's width, and the glyphs of a word are drawn at the
  word's integer position plus their exact offsets from it. KHTML and the
  rich text engine lay out words and spaces by their widths; without this
  a line drawn as one string would be up to half a pixel per word longer
  or shorter than the sum of its words (text running into the next word).
  The inter-word space varies by less than a pixel; the spacing inside a
  word is exact.
*/
struct QHbPen
{
    QHbPen() : pen( 0 ), ipen( 0 ), space( -1 ) {}
    int pen;			// exact, 26.6
    int ipen;			// integer, pixels
    int space;			// the last glyph was a space; -1: none yet
};

static inline bool qt_hb_is_space( const QChar &c )
{
    return c.unicode() == 0x20;
}

// the integer advance of the next glyph (in visual order) of exact advance
// adv; prevAdv: the exact advance of the glyph before it, corrected when
// the pen is snapped (or 0)
static inline int qt_hb_advance( QHbPen &p, int adv, bool space, int *prevAdv )
{
    if ( p.space >= 0 && ( space || p.space ) ) {
	const int delta = ( p.ipen << 6 ) - p.pen;
	if ( prevAdv )
	    *prevAdv += delta;
	p.pen = p.ipen << 6;
    }
    p.space = space;
    p.pen += adv;
    const int r = TRUNC( ROUND( p.pen ) );
    const int a = r - p.ipen;
    p.ipen = r;
    return a;
}

/*
  The Latin fast path. Most text is Latin (ASCII, Latin-1, Latin Extended-A,
  the Romanian comma-below letters), and in most fonts its shaping is
  nothing but the character map, the advances and pair kerning: shaping
  such a string with HarfBuzz gives each character its glyph and advance
  plus the kerning of the pair it starts. Per font, once: the font is
  eligible if it is not shaped with a contextual lookup on these glyphs
  (calt and other chaining lookups whose context and input glyphs include
  them; ligatures and kerning are found per pair below). Per character, on
  first use: its glyph and advance shaped alone (a character HarfBuzz
  substitutes or decomposes is not fast). Per pair of characters, on first
  use: the two shaped together: the same glyphs and only the first one's
  advance changed (kerning), else the pair is not fast (a ligature, an
  adjustment of the second glyph or an offset). A string of fast
  characters and pairs gets its glyphs and advances from these tables,
  exactly what HarfBuzz gives it (wayland/test/rthb compares); anything
  else goes to the word cache and HarfBuzz.
*/
enum { QHbFastChars = 0x160 + 4, QHbFastPairs = 4096 };

struct QHbFast
{
    struct Char {
	uint glyph;
	int advance;		// 26.6
	int state;		// 0: not known yet, 1: fast, -1: not fast
    } chars[QHbFastChars];
    struct Pair {
	uint key;		// (c1 << 16 | c2) + 1, 0: empty
	int kern;		// added to the first one's advance, 26.6
	int state;
    } pairs[QHbFastPairs];
};

static void qt_hb_free_fast( QHbFontData *d )
{
    if ( d->fast && d->fast != (QHbFast *)1 )
	free( d->fast );
    d->fast = 0;
}

// the index of a fast character, -1 for another one
static inline int qt_hb_fast_index( ushort u )
{
    if ( u >= 0x20 && u < 0x180 )
	return u - 0x20;
    if ( u >= 0x218 && u <= 0x21b )
	return 0x160 + u - 0x218;
    return -1;
}

static const ushort qt_hb_fast_extra[4] = { 0x218, 0x219, 0x21a, 0x21b };

static inline ushort qt_hb_fast_char( int index )
{
    return index < 0x160 ? (ushort)( index + 0x20 ) : qt_hb_fast_extra[index - 0x160];
}

// shapes the characters (Latin, left to right, the font's features); the
// result stays in qt_hb_buffer (qt_hb_glyphs is not touched: it may hold
// the glyphs of the words before); the number of glyphs, -1 on failure
static int qt_hb_fast_run( hb_font_t *font, QHbFontData *fd, const ushort *text, int len,
			   const hb_feature_t *features, unsigned int nfeatures,
			   const hb_glyph_info_t **info, const hb_glyph_position_t **pos )
{
    hb_buffer_t *buffer = qt_hb_buffer;
    hb_buffer_clear_contents( buffer );
    hb_buffer_add_utf16( buffer, (const uint16_t *)text, len, 0, len );
    hb_buffer_set_direction( buffer, HB_DIRECTION_LTR );
    hb_buffer_set_script( buffer, HB_SCRIPT_LATIN );
    hb_buffer_guess_segment_properties( buffer );
    if ( !fd->plans[0] ) {
	hb_segment_properties_t props;
	hb_buffer_get_segment_properties( buffer, &props );
	fd->plans[0] = hb_shape_plan_create_cached( hb_font_get_face( font ), &props, features, nfeatures, 0 );
    }
    if ( !hb_shape_plan_execute( fd->plans[0], font, buffer, features, nfeatures ) )
	return -1;
    *info = hb_buffer_get_glyph_infos( buffer, 0 );
    *pos = hb_buffer_get_glyph_positions( buffer, 0 );
    return (int)hb_buffer_get_length( buffer );
}

// the font's fast tables, 0 if the font is not eligible
static QHbFast *qt_hb_fast( hb_font_t *font, QHbFontData *fd )
{
    if ( fd->fast )
	return fd->fast == (QHbFast *)1 ? 0 : fd->fast;
    fd->fast = (QHbFast *)1;		// not eligible, unless found below
    // the glyphs of the fast characters
    hb_set_t *glyphs = hb_set_create();
    for ( int i = 0; i < QHbFastChars; ++i ) {
	hb_codepoint_t g;
	if ( hb_font_get_nominal_glyph( font, qt_hb_fast_char( i ), &g ) )
	    hb_set_add( glyphs, g );
    }
    // contextual lookups of the Latin script with the default and the
    // current language (as HarfBuzz shapes) that these glyphs alone can
    // trigger: for GSUB the closure of the glyphs under them (a rule
    // applies in it only if all its glyphs, context included, are there:
    // the closure grows), for GPOS context and input glyphs among them
    hb_face_t *face = hb_font_get_face( font );
    static const hb_tag_t scripts[] = {
	HB_TAG( 'l', 'a', 't', 'n' ), HB_TAG( 'D', 'F', 'L', 'T' ), HB_TAG_NONE
    };
    hb_tag_t languages[8];
    unsigned int nlang = 6;
    hb_tag_t scriptTags[2];
    unsigned int nscript = 2;
    hb_ot_tags_from_script_and_language( HB_SCRIPT_LATIN, hb_language_get_default(),
					 &nscript, scriptTags, &nlang, languages + 1 );
    languages[0] = HB_OT_TAG_DEFAULT_LANGUAGE;
    languages[nlang + 1] = HB_TAG_NONE;
    hb_set_t *lookups = hb_set_create();
    hb_set_t *contextual = hb_set_create();
    hb_set_t *before = hb_set_create();
    hb_set_t *input = hb_set_create();
    hb_set_t *after = hb_set_create();
    bool ok = TRUE;
    const hb_tag_t tables[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
    for ( int t = 0; t < 2 && ok; ++t ) {
	hb_set_clear( lookups );
	hb_ot_layout_collect_lookups( face, tables[t], scripts, languages, 0, lookups );
	hb_codepoint_t l = HB_SET_VALUE_INVALID;
	while ( ok && hb_set_next( lookups, &l ) ) {
	    hb_set_clear( before );
	    hb_set_clear( input );
	    hb_set_clear( after );
	    hb_ot_layout_lookup_collect_glyphs( face, tables[t], l, before, input, after, 0 );
	    if ( hb_set_is_empty( before ) && hb_set_is_empty( after ) )
		continue;
	    if ( t == 0 ) {
		hb_set_add( contextual, l );
		continue;
	    }
	    hb_set_union( before, after );
	    hb_set_intersect( before, glyphs );
	    hb_set_intersect( input, glyphs );
	    if ( !hb_set_is_empty( before ) && !hb_set_is_empty( input ) )
		ok = FALSE;
	}
    }
    if ( ok && !hb_set_is_empty( contextual ) ) {
	hb_set_t *closure = hb_set_copy( glyphs );
	hb_ot_layout_lookups_substitute_closure( face, contextual, closure );
	ok = hb_set_get_population( closure ) == hb_set_get_population( glyphs );
	hb_set_destroy( closure );
    }
    hb_set_destroy( lookups );
    hb_set_destroy( contextual );
    hb_set_destroy( before );
    hb_set_destroy( input );
    hb_set_destroy( after );
    hb_set_destroy( glyphs );
    if ( !ok )
	return 0;
    QHbFast *f = (QHbFast *)calloc( 1, sizeof( QHbFast ) );
    if ( !f )
	return 0;
    fd->fast = f;
    return f;
}

static inline const QHbFast::Char *qt_hb_fast_char_info( hb_font_t *font, QHbFontData *fd, QHbFast *f,
							   int index, const hb_feature_t *features,
							   unsigned int nfeatures )
{
    QHbFast::Char &c = f->chars[index];
    if ( !c.state ) {
	const ushort u = qt_hb_fast_char( index );
	hb_codepoint_t nominal = 0;
	const hb_glyph_info_t *info;
	const hb_glyph_position_t *pos;
	const int n = qt_hb_fast_run( font, fd, &u, 1, features, nfeatures, &info, &pos );
	c.state = -1;
	if ( n == 1 && hb_font_get_nominal_glyph( font, u, &nominal )
	     && info[0].codepoint == nominal && !pos[0].x_offset && !pos[0].y_offset ) {
	    c.glyph = info[0].codepoint;
	    c.advance = pos[0].x_advance;
	    c.state = 1;
	}
    }
    return &c;
}

static int qt_hb_fast_find_kern( hb_font_t *font, QHbFontData *fd, QHbFast *f, int i1, int i2,
				 const hb_feature_t *features, unsigned int nfeatures );

// the kerning of a pair of fast characters, or INT_MIN if not fast
static inline int qt_hb_fast_kern( hb_font_t *font, QHbFontData *fd, QHbFast *f, int i1, int i2,
				   const hb_feature_t *features, unsigned int nfeatures )
{
    const uint key = ( (uint)i1 << 16 | (uint)i2 ) + 1;
    const QHbFast::Pair &p = f->pairs[( key * 0x9e3779b1U ) >> 20];
    if ( p.key == key )
	return p.state > 0 ? p.kern : INT_MIN;
    return qt_hb_fast_find_kern( font, fd, f, i1, i2, features, nfeatures );
}

static int qt_hb_fast_find_kern( hb_font_t *font, QHbFontData *fd, QHbFast *f, int i1, int i2,
				 const hb_feature_t *features, unsigned int nfeatures )
{
    const uint key = ( (uint)i1 << 16 | (uint)i2 ) + 1;
    QHbFast::Pair &p = f->pairs[( key * 0x9e3779b1U ) >> 20];
    {
	const QHbFast::Char *c1 = &f->chars[i1];
	const QHbFast::Char *c2 = &f->chars[i2];
	const ushort text[2] = { qt_hb_fast_char( i1 ), qt_hb_fast_char( i2 ) };
	const hb_glyph_info_t *info;
	const hb_glyph_position_t *pos;
	const int n = qt_hb_fast_run( font, fd, text, 2, features, nfeatures, &info, &pos );
	p.key = key;
	p.state = -1;
	if ( n == 2 && info[0].codepoint == c1->glyph && info[1].codepoint == c2->glyph
	     && pos[1].x_advance == c2->advance && info[0].cluster == 0 && info[1].cluster == 1
	     && !pos[0].x_offset && !pos[0].y_offset && !pos[1].x_offset && !pos[1].y_offset ) {
	    p.kern = pos[0].x_advance - c1->advance;
	    p.state = 1;
	}
    }
    return p.state > 0 ? p.kern : INT_MIN;
}

// qt_hb_seg: the glyphs of a fast segment for the word path

/*
  The glyphs of uc[c, e) from the fast tables (into qt_hb_seg, clusters
  from c), the last one kerned with uc[e] if e < len; FALSE if a character
  or a pair is not fast.
*/
static bool qt_hb_fast_segment( hb_font_t *font, QHbFontData *fd, QHbFast *f,
				const hb_feature_t *features, unsigned int nfeatures,
				const QChar *uc, int c, int e, int len, int *out )
{
    const int last = e < len ? e : e - 1;
    for ( int i = c; i <= last; ++i ) {
	const int idx = qt_hb_fast_index( uc[i].unicode() );
	if ( idx < 0 )
	    return FALSE;
	if ( f->chars[idx].state <= 0
	     && qt_hb_fast_char_info( font, fd, f, idx, features, nfeatures )->state < 0 )
	    return FALSE;
    }
    // into qt_hb_glyphs at *out (clusters from the text's start) or into
    // qt_hb_seg (clusters from c); pairs are found in qt_hb_buffer only
    QHbGlyph *dst;
    int base;
    if ( out ) {
	if ( !qt_hb_reserve( *out + e - c ) )
	    return FALSE;
	dst = qt_hb_glyphs + *out;
	base = c;
    } else {
	if ( e - c > qt_hb_seg_alloc ) {
	    QHbGlyph *g = (QHbGlyph *)realloc( qt_hb_seg, ( e - c ) * sizeof( QHbGlyph ) );
	    if ( !g )
		return FALSE;
	    qt_hb_seg = g;
	    qt_hb_seg_alloc = e - c;
	}
	dst = qt_hb_seg;
	base = 0;
    }
    int i1 = qt_hb_fast_index( uc[c].unicode() );
    for ( int i = c; i < e; ++i ) {
	const QHbFast::Char &ch = f->chars[i1];
	int adv = ch.advance;
	int i2 = -1;
	if ( i + 1 < len ) {
	    i2 = qt_hb_fast_index( uc[i + 1].unicode() );
	    const int k = qt_hb_fast_kern( font, fd, f, i1, i2, features, nfeatures );
	    if ( k == INT_MIN )
		return FALSE;
	    adv += k;
	}
	QHbGlyph &g = dst[i - c];
	g.glyph = ch.glyph;
	g.cluster = i - c + base;
	g.advance = adv;
	g.x = 0;
	g.y = 0;
	i1 = i2;
    }
    return TRUE;
}

/*
  The glyphs of uc[0, len) (left to right, a simple script) word by word,
  from the word cache, into qt_hb_glyphs; FALSE if the font or the text
  does not allow it (*n = -1 on an allocation failure). A word is the
  characters up to and with the spaces after them; with kerning against
  the space (space mode 2) it is shaped with the character after it, whose
  glyph is dropped. With pen, only the advances of the glyphs of the
  clusters before widthLen are added to *pen (26.6, rounded per glyph
  unless fractional), nothing is copied.
*/
static bool qt_hb_words_shape( hb_font_t *font, QHbFontData *fd, const hb_feature_t *features,
			       unsigned int nfeatures, int qscript, const QChar *uc, int len, int *n,
			       int widthLen = -1, bool fractional = TRUE, QHbPen *pen = 0 )
{
    const int mode = qt_hb_space_mode( font, fd );
    if ( !mode )
	return FALSE;
    // the item's script for every word (digits and punctuation alone
    // would be "common"), with its shape plan
    hb_script_t script = HB_SCRIPT_INVALID;
    hb_shape_plan_t **plan = 0;
    switch ( qscript ) {
    case QFont::Latin:
	script = HB_SCRIPT_LATIN;
	plan = &fd->plans[0];
	break;
    case QFont::Greek:
	script = HB_SCRIPT_GREEK;
	plan = &fd->plans[1];
	break;
    case QFont::Cyrillic:
	script = HB_SCRIPT_CYRILLIC;
	plan = &fd->plans[2];
	break;
    default:
	break;
    }
    if ( !qt_hb_words ) {
	qt_hb_words = (QHbWord *)calloc( QHbWordCacheSize, sizeof( QHbWord ) );
	if ( !qt_hb_words ) {
	    *n = -1;
	    return FALSE;
	}
    }
    QHbFast *fast = qscript == QFont::Latin ? qt_hb_fast( font, fd ) : 0;
    *n = 0;
    int c = 0;
    while ( c < len && ( !pen || c < widthLen ) ) {
	// a word and the spaces after it
	int e = c;
	while ( e < len && uc[e].unicode() != 0x20 )
	    ++e;
	while ( e < len && uc[e].unicode() == 0x20 )
	    ++e;
	if ( e < len && uc[e].unicode() >= 0x300 ) {
	    // no combining mark after a space (it would attach to the space)
	    const int cat = ::category( uc[e] );
	    if ( cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining
		 || cat == QChar::Mark_Enclosing )
		return FALSE;
	}
	const int wl = e - c;
	// the text shaped: with the next character in mode 2
	const int tl = ( mode == 2 && e < len ) ? wl + 1 : wl;
	const QHbGlyph *wg;
	int count;
	if ( fast && qt_hb_fast_segment( font, fd, fast, features, nfeatures, uc, c, e, len,
					 pen ? 0 : n ) ) {
	    // the Latin fast path (glyphs already at *n, clusters from c)
	    if ( !pen ) {
		*n += wl;
		c = e;
		continue;
	    }
	    wg = qt_hb_seg;
	    count = wl;
	} else if ( tl > QHbMaxWordLength ) {
	    // too long to keep
	    const int n0 = *n;
	    if ( !qt_hb_run( font, uc + c, tl, 0, tl, HB_BUFFER_FLAG_DEFAULT, FALSE, FALSE,
			     features, nfeatures, 0, n, 0, script, plan ) ) {
		*n = -1;
		return FALSE;
	    }
	    count = *n - n0;
	    while ( count > 0 && qt_hb_glyphs[n0 + count - 1].cluster >= wl )
		--count;
	    *n = n0;
	    wg = qt_hb_glyphs + n0;
	} else {
	    uint h = ( fd->id * 0x9e3779b1U ) ^ (uint)qscript;
	    for ( int k = c; k < c + tl; ++k )
		h = ( h ^ uc[k].unicode() ) * 0x01000193U;
	    QHbWord *set = qt_hb_words + ( ( ( h ^ ( h >> 16 ) ) << 1 ) & ( QHbWordCacheSize - 1 ) );
	    QHbWord *wp = 0;
	    for ( int k = 0; k < 2; ++k ) {
		if ( set[k].font == fd->id && set[k].hash == h && set[k].length == tl
		     && set[k].script == qscript
		     && !memcmp( set[k].text, uc + c, tl * sizeof( ushort ) ) ) {
		    wp = &set[k];
		    break;
		}
	    }
	    if ( !wp ) {
		// shape it alone and keep it, instead of the older one
		wp = set[0].stamp <= set[1].stamp ? &set[0] : &set[1];
		QHbWord &w = *wp;
		const int n0 = *n;
		if ( !qt_hb_run( font, uc + c, tl, 0, tl, HB_BUFFER_FLAG_DEFAULT, FALSE, FALSE,
				 features, nfeatures, 0, n, 0, script, plan ) ) {
		    *n = -1;
		    return FALSE;
		}
		int cnt = *n - n0;
		while ( cnt > 0 && qt_hb_glyphs[n0 + cnt - 1].cluster >= wl )
		    --cnt;
		*n = n0;
		free( w.text );
		w.font = 0;
		const int textSize = ( tl * sizeof( ushort ) + 7 ) & ~7;
		w.text = (ushort *)malloc( textSize + cnt * sizeof( QHbGlyph ) );
		if ( !w.text ) {
		    *n = -1;
		    return FALSE;
		}
		memcpy( w.text, uc + c, tl * sizeof( ushort ) );
		w.length = tl;
		w.count = cnt;
		memcpy( qt_hb_word_glyphs( w ), qt_hb_glyphs + n0, cnt * sizeof( QHbGlyph ) );
		w.font = fd->id;
		w.script = qscript;
		w.hash = h;
	    }
	    wp->stamp = ++qt_hb_word_stamp;
	    wg = qt_hb_word_glyphs( *wp );
	    count = wp->count;
	}
	if ( pen ) {
	    for ( int k = 0; k < count && wg[k].cluster + c < widthLen; ++k )
		qt_hb_advance( *pen, fractional ? wg[k].advance : ROUND( wg[k].advance ),
			       qt_hb_is_space( uc[c + wg[k].cluster] ), 0 );
	} else {
	    if ( wg != qt_hb_glyphs + *n ) {
		if ( !qt_hb_reserve( *n + count ) ) {
		    *n = -1;
		    return FALSE;
		}
		memmove( qt_hb_glyphs + *n, wg, count * sizeof( QHbGlyph ) );
	    }
	    QHbGlyph *g = qt_hb_glyphs + *n;
	    for ( int k = 0; k < count; ++k )
		g[k].cluster += c;
	    *n += count;
	}
	c = e;
    }
    return TRUE;
}

// the HarfBuzz font of an engine: 0 if it has none (Qt's shapers then)
static hb_font_t *qt_hb_engine_font( QFontEngine *fe, bool *fractional, bool *fixedPitch )
{
    if ( fe->type() == QFontEngine::Xft ) {
	QFontEngineXft *xe = (QFontEngineXft *)fe;
	if ( !xe->freetype() || xe->scale() != 1. )
	    return 0;
	*fractional = xe->fractional();
	*fixedPitch = xe->freetype()->fixedPitch();
	return xe->hbFont();
    }
    if ( fe->type() == QFontEngine::Freetype ) {
	QFontEngineFT *ft = (QFontEngineFT *)fe;
	if ( ft->scale() != 1. )
	    return 0;
	*fractional = ft->fractional();
	*fixedPitch = ft->fixedPitch();
	return ft->hbFont();
    }
    return 0;
}

static bool qt_hb_init_buffer()
{
    if ( !qt_hb_buffer ) {
	qt_hb_buffer = hb_buffer_create();
	if ( !hb_buffer_allocation_successful( qt_hb_buffer ) ) {
	    hb_buffer_destroy( qt_hb_buffer );
	    qt_hb_buffer = 0;
	    return FALSE;
	}
	qAddPostRoutine( qt_hb_cleanup );
    }
    return TRUE;
}

/*
  QFontMetrics::width() of uc[0, length) (all characters of the Latin fast
  path: one Latin item): the integer pen position after the first len
  characters, in one pass over the fast tables; -1 if a character or a
  pair is not fast (or the engine has no fast tables). The same as
  qt_hb_shape() gives (the fast segments of the word path).
*/
static int qt_hb_latin_pen( QFontEngine *fe, const QChar *uc, int length, int len, int *advances );

int qt_hb_latin_width( QFontEngine *fe, const QChar *uc, int length, int len )
{
    return qt_hb_latin_pen( fe, uc, length, len, 0 );
}

static int qt_hb_latin_pen( QFontEngine *fe, const QChar *uc, int length, int len, int *advances )
{
    bool fractional = FALSE, fixedPitch = FALSE;
    hb_font_t *font = qt_hb_engine_font( fe, &fractional, &fixedPitch );
    if ( !font || !qt_hb_init_buffer() )
	return -1;
    QHbFontData *fd = qt_hb_font_data( font );
    QHbFast *f = fd ? qt_hb_fast( font, fd ) : 0;
    if ( !f || !length )
	return -1;
    unsigned int nfeatures = 0;
    const hb_feature_t *features = fixedPitch ? qt_hb_fixed_pitch_features( &nfeatures ) : 0;
    QHbPen pen;
    const int end = QMIN( len, length );
    int i1 = qt_hb_fast_index( uc[0].unicode() );
    if ( i1 < 0 || ( f->chars[i1].state <= 0
		     && qt_hb_fast_char_info( font, fd, f, i1, features, nfeatures )->state < 0 ) )
	return -1;
    for ( int i = 0; i < end; ++i ) {
	int adv = f->chars[i1].advance;
	int i2 = -1;
	if ( i + 1 < length ) {
	    i2 = qt_hb_fast_index( uc[i + 1].unicode() );
	    if ( i2 < 0 || ( f->chars[i2].state <= 0
			     && qt_hb_fast_char_info( font, fd, f, i2, features, nfeatures )->state < 0 ) )
		return -1;
	    const int k = qt_hb_fast_kern( font, fd, f, i1, i2, features, nfeatures );
	    if ( k == INT_MIN )
		return -1;
	    adv += k;
	}
	const int a = qt_hb_advance( pen, fractional ? adv : ROUND( adv ), uc[i].unicode() == 0x20, 0 );
	if ( advances )
	    advances[i] = a;
	i1 = i2;
    }
    return pen.ipen;
}

// the unique id of an engine's HarfBuzz font (never reused), 0 if none
uint qt_hb_engine_id( QFontEngine *fe )
{
    // the last one asked for (reset when its HarfBuzz font goes, i.e.
    // before the engine can go and its address be reused)
    if ( fe == qt_hb_last_engine && fe->scale() == 1. )
	return qt_hb_last_engine_id;
    bool fractional = FALSE, fixedPitch = FALSE;
    hb_font_t *font = qt_hb_engine_font( fe, &fractional, &fixedPitch );
    QHbFontData *fd = font ? qt_hb_font_data( font ) : 0;
    if ( !fd )
	return 0;
    qt_hb_last_engine = fe;
    qt_hb_last_engine_id = fd->id;
    return fd->id;
}

/*
  QFontMetrics::width() of text that is one item of a simple script
  (QTextEngine would make one item of it): the width of its first len
  characters without a QTextEngine, from the word cache; -1 if the engine
  or the text does not allow it.
*/
int qt_hb_text_width( QFontEngine *fe, int script, const QChar *uc, int length, int len )
{
    if ( !qt_hb_word_script( script ) )
	return -1;
    if ( qt_hb_mode() == 0 )
	return -1;
    bool fractional = FALSE, fixedPitch = FALSE;
    hb_font_t *font = qt_hb_engine_font( fe, &fractional, &fixedPitch );
    if ( !font || !qt_hb_init_buffer() )
	return -1;
    QHbFontData *fd = qt_hb_font_data( font );
    if ( !fd )
	return -1;
    unsigned int nfeatures = 0;
    const hb_feature_t *features = fixedPitch ? qt_hb_fixed_pitch_features( &nfeatures ) : 0;
    // the glyphs of the clusters that start before len (QTextEngine::width()),
    // the integer advances as qt_hb_shape() computes them
    int n = 0;
    QHbPen pen;
    if ( !qt_hb_words_shape( font, fd, features, nfeatures, script, uc, length, &n, len, fractional, &pen ) )
	return -1;
    return pen.ipen;
}

/*
  The item cache: the shaped glyphs of an item (glyphs, advances, offsets,
  exact positions, attributes, log clusters), for an item that comes again
  (a repaint draws the same strings, QTextEdit draws the runs it measured).
  The key: the HarfBuzz font, the script, the direction flags, the text
  with up to 5 characters of context on both sides (what HarfBuzz looks
  at). An item is kept the second time it is seen (the first time only its
  hash is noted: text drawn once costs no copy). 1024 sets of two, items
  up to 256 characters, at most 4 MB.
*/
struct QHbItem
{
    uint font;			// QHbFontData::id, 0: empty
    uint hash;
    uint stamp;
    int keyLength;		// characters of the key
    int flags;			// direction flags, context before, script, length
    int count;			// glyphs; -1: only seen once (no data)
    int fixed;			// the item has exact positions
    uint bytes;
    char *data;			// key characters, then the glyph arrays
};

enum { QHbItemSets = 1024, QHbMaxItemLength = 256, QHbItemContext = 5 };
static const uint qt_hb_max_item_memory = 4 * 1024 * 1024;
static QHbItem *qt_hb_items = 0;
static uint qt_hb_item_memory = 0;
static uint qt_hb_item_stamp = 0;

static void qt_hb_free_items()
{
    if ( !qt_hb_items )
	return;
    for ( int i = 0; i < 2 * QHbItemSets; ++i )
	free( qt_hb_items[i].data );
    free( qt_hb_items );
    qt_hb_items = 0;
    qt_hb_item_memory = 0;
}

static inline uint qt_hb_item_data_size( int keyLength, int n, int len )
{
    return ( ( keyLength * 2 + 7 ) & ~7 ) + n * ( sizeof( glyph_t ) + sizeof( advance_t )
		+ sizeof( qoffset_t ) + sizeof( GlyphAttributes ) + sizeof( qfixedpos_t ) )
	+ len * sizeof( unsigned short );
}

static int qt_hb_shape_item( QShaperItem *item, hb_font_t *font, QHbFontData *fd,
			     bool fractional, bool fixedPitch );

int qt_hb_shape( QShaperItem *item )
{
    if ( qt_hb_mode() == 0 )
	return -1;
    bool fractional = FALSE;
    bool fixedPitch = FALSE;
    hb_font_t *font = qt_hb_engine_font( item->font, &fractional, &fixedPitch );
    // one buffer, reused (text layout happens in the GUI thread)
    if ( !font || !qt_hb_init_buffer() )
	return -1;
    QHbFontData *fd = qt_hb_font_data( font );
    if ( !fd )
	return -1;
    const int len = item->length;
    if ( len > QHbMaxItemLength )
	return qt_hb_shape_item( item, font, fd, fractional, fixedPitch );

    // the key
    const QString &str = *item->string;
    const int from = QMAX( 0, item->from - QHbItemContext );
    const int to = QMIN( (int)str.length(), item->from + len + QHbItemContext );
    const ushort *key = (const ushort *)str.unicode() + from;
    const int keyLength = to - from;
    const int flags = ( item->flags & 3 ) | ( ( item->from - from ) << 2 ) | ( item->script << 5 )
		      | ( len << 12 );
    uint h = ( fd->id * 0x9e3779b1U ) ^ (uint)flags * 0x85ebca6bU;
    for ( int k = 0; k < keyLength; ++k )
	h = ( h ^ key[k] ) * 0x01000193U;
    h ^= (uint)keyLength;

    if ( !qt_hb_items ) {
	qt_hb_items = (QHbItem *)calloc( 2 * QHbItemSets, sizeof( QHbItem ) );
	if ( !qt_hb_items )
	    return qt_hb_shape_item( item, font, fd, fractional, fixedPitch );
    }
    QHbItem *set = qt_hb_items + 2 * ( ( h ^ ( h >> 15 ) ) & ( QHbItemSets - 1 ) );
    QHbItem *e = 0;
    for ( int k = 0; k < 2; ++k ) {
	if ( set[k].font == fd->id && set[k].hash == h && set[k].keyLength == keyLength
	     && set[k].flags == flags ) {
	    e = &set[k];
	    break;
	}
    }
    if ( e && e->count >= 0 && !memcmp( e->data, key, keyLength * 2 ) ) {
	// a hit
	const int n = e->count;
	if ( n > item->num_glyphs ) {
	    item->num_glyphs = n;
	    return 0;
	}
	e->stamp = ++qt_hb_item_stamp;
	const char *d = e->data + ( ( keyLength * 2 + 7 ) & ~7 );
	memcpy( item->fixed, d, n * sizeof( qfixedpos_t ) ); d += n * sizeof( qfixedpos_t );
	memcpy( item->advances, d, n * sizeof( advance_t ) ); d += n * sizeof( advance_t );
	memcpy( item->offsets, d, n * sizeof( qoffset_t ) ); d += n * sizeof( qoffset_t );
	memcpy( item->glyphs, d, n * sizeof( glyph_t ) ); d += n * sizeof( glyph_t );
	memcpy( item->attributes, d, n * sizeof( GlyphAttributes ) ); d += n * sizeof( GlyphAttributes );
	memcpy( item->log_clusters, d, len * sizeof( unsigned short ) );
	item->num_glyphs = n;
	item->has_positioning = TRUE;
	item->has_fixed = e->fixed;
	return 1;
    }

    const int r = qt_hb_shape_item( item, font, fd, fractional, fixedPitch );
    if ( r <= 0 )
	return r;
    if ( !e ) {
	// first seen: note it, keep it the next time
	e = set[0].stamp <= set[1].stamp ? &set[0] : &set[1];
	qt_hb_item_memory -= QMIN( qt_hb_item_memory, e->bytes );
	free( e->data );
	e->data = 0;
	e->bytes = 0;
	e->font = fd->id;
	e->hash = h;
	e->keyLength = keyLength;
	e->flags = flags;
	e->count = -1;
	e->stamp = ++qt_hb_item_stamp;
	return r;
    }
    // seen before: keep it
    const int n = item->num_glyphs;
    const uint bytes = qt_hb_item_data_size( keyLength, n, len );
    qt_hb_item_memory -= QMIN( qt_hb_item_memory, e->bytes );
    free( e->data );
    e->data = 0;
    e->bytes = 0;
    e->count = -1;
    if ( qt_hb_item_memory + bytes > qt_hb_max_item_memory )
	return r;
    char *data = (char *)malloc( bytes );
    if ( !data )
	return r;
    memcpy( data, key, keyLength * 2 );
    char *d = data + ( ( keyLength * 2 + 7 ) & ~7 );
    memcpy( d, item->fixed, n * sizeof( qfixedpos_t ) ); d += n * sizeof( qfixedpos_t );
    memcpy( d, item->advances, n * sizeof( advance_t ) ); d += n * sizeof( advance_t );
    memcpy( d, item->offsets, n * sizeof( qoffset_t ) ); d += n * sizeof( qoffset_t );
    memcpy( d, item->glyphs, n * sizeof( glyph_t ) ); d += n * sizeof( glyph_t );
    memcpy( d, item->attributes, n * sizeof( GlyphAttributes ) ); d += n * sizeof( GlyphAttributes );
    memcpy( d, item->log_clusters, len * sizeof( unsigned short ) );
    e->data = data;
    e->bytes = bytes;
    e->count = n;
    e->fixed = item->has_fixed;
    e->stamp = ++qt_hb_item_stamp;
    qt_hb_item_memory += bytes;
    return r;
}

static int qt_hb_shape_item( QShaperItem *item, hb_font_t *font, QHbFontData *fd,
			     bool fractional, bool fixedPitch )
{
    const QString &str = *item->string;
    const int strLength = str.length();
    const QChar *uc = str.unicode() + item->from;
    const int len = item->length;
    const bool directionFromScript = item->flags & QTextEngine::DirectionFromScript;
    bool rtl = item->flags & QTextEngine::RightToLeft;

    unsigned int nfeatures = 0;
    const hb_feature_t *features = fixedPitch ? qt_hb_fixed_pitch_features( &nfeatures ) : 0;

    int n = 0;
    const bool words = !rtl && qt_hb_word_script( item->script )
		       && qt_hb_words_shape( font, fd, features, nfeatures, item->script, uc, len, &n );
    if ( n < 0 )
	return -1;
    bool simple = words && n == len && n <= item->num_glyphs;
    for ( int i = 0; simple && i < n; ++i )
	simple = qt_hb_glyphs[i].cluster == i && !qt_hb_glyphs[i].x && !qt_hb_glyphs[i].y
		 && qt_hb_glyphs[i].advance;
    if ( simple ) {
	// a glyph per character, no offsets (the Latin fast path, most
	// text): the post-processing below in short
	const QHbGlyph *hg = qt_hb_glyphs;
	qfixedpos_t *fixed = item->fixed;
	QHbPen pen;
	GlyphAttributes character;
	memset( &character, 0, sizeof( character ) );
	character.clusterStart = TRUE;
	character.justification = GlyphAttributes::Character;
	GlyphAttributes space = character;
	space.justification = GlyphAttributes::Space;
	for ( int i = 0; i < n; ++i ) {
	    const ushort u = uc[i].unicode();
	    const int adv = fractional ? hg[i].advance : ROUND( hg[i].advance );
	    item->glyphs[i] = (glyph_t)hg[i].glyph;
	    item->log_clusters[i] = (unsigned short)i;
	    fixed[i].advance = adv;
	    fixed[i].x = 0;
	    fixed[i].y = 0;
	    item->offsets[i].x = 0;
	    item->offsets[i].y = 0;
	    item->attributes[i] = u == 0x20 ? space : character;
	    item->advances[i] = qt_hb_advance( pen, adv, u == 0x20, i ? &fixed[i - 1].advance : 0 );
	}
	item->num_glyphs = n;
	item->has_positioning = TRUE;
	item->has_fixed = fractional;
	return 1;
    }
    if ( !words ) {
	n = 0;
	// the whole string as context (Arabic joining, ...)
	int flags = HB_BUFFER_FLAG_DEFAULT;
	if ( item->from == 0 )
	    flags |= HB_BUFFER_FLAG_BOT;
	if ( item->from + len == strLength )
	    flags |= HB_BUFFER_FLAG_EOT;
	if ( !qt_hb_run( font, str.unicode(), strLength, item->from, len, flags, rtl,
			 directionFromScript, features, nfeatures, 0, &n, &rtl ) )
	    return -1;
    }

    if ( n > item->num_glyphs ) {
	item->num_glyphs = n;
	return 0;
    }
    const QHbGlyph *hg = qt_hb_glyphs;

    // the log clusters: the first glyph of the cluster of each character
    // (the clusters are monotonic in logical order), in one pass
    unsigned short *logClusters = item->log_clusters;
    {
	int c = 0;
	int first = 0;
	for ( int g = 0; g < n; ++g ) {
	    const int cl = hg[g].cluster;
	    if ( g == 0 || cl != hg[g - 1].cluster ) {
		// characters up to this cluster belong to the one before
		for ( ; c < cl && c < len; ++c )
		    logClusters[c] = (unsigned short)first;
		first = g;
	    }
	}
	for ( ; c < len; ++c )
	    logClusters[c] = (unsigned short)first;
    }

    qfixedpos_t *fixed = item->fixed;
    QHbPen pen;
    int clusterGlyph = 0;
    for ( int g = 0; g < n; ++g ) {
	item->glyphs[g] = (glyph_t)hg[g].glyph;
	const bool start = g == 0 || hg[g].cluster != hg[g - 1].cluster;
	if ( start )
	    clusterGlyph = g;
	int c = hg[g].cluster;
	if ( c < 0 || c >= len )
	    c = 0;
	const ushort u = uc[c].unicode();

	int adv = hg[g].advance;
	int xo = hg[g].x;
	int yo = -hg[g].y;		// Qt's y goes down
	if ( !fractional ) {
	    adv = ROUND( adv );
	    xo = ROUND( xo );
	    yo = ROUND( yo );
	}
	fixed[g].advance = adv;
	fixed[g].x = xo;
	fixed[g].y = yo;
	item->offsets[g].x = (short)TRUNC( ROUND( xo ) );
	item->offsets[g].y = (short)TRUNC( ROUND( yo ) );

	GlyphAttributes a;
	a.clusterStart = start;
	a.mark = !start && adv == 0;
	a.zeroWidth = start && adv == 0 && u >= 0xad && qt_hb_invisible( u );
	a.reserved = 0;
	a.combiningClass = 0;
	if ( a.mark ) {
	    // a glyph inside a cluster: roughly its character
	    int mc = c + g - clusterGlyph;
	    if ( mc >= len )
		mc = len - 1;
	    a.combiningClass = ::combiningClass( uc[mc] );
	    a.justification = GlyphAttributes::NoJustification;
	} else if ( u < 0x80 ? u == 0x20 : ::category( uc[c] ) == QChar::Separator_Space ) {
	    a.justification = GlyphAttributes::Space;
	} else {
	    a.justification = GlyphAttributes::Character;
	}
	item->attributes[g] = a;
	// the integer advances: the rounded running sums of the exact ones,
	// in visual order (left to right from the item's integer origin), so
	// that the integer positions are the exact ones rounded; snapped at
	// spaces (qt_hb_advance())
	if ( !rtl )
	    item->advances[g] = qt_hb_advance( pen, adv, u == 0x20, g ? &fixed[g - 1].advance : 0 );
    }
    if ( rtl ) {
	for ( int k = 0; k < n; ++k ) {
	    const int g = n - 1 - k;
	    int c = hg[g].cluster;
	    if ( c < 0 || c >= len )
		c = 0;
	    item->advances[g] = qt_hb_advance( pen, fixed[g].advance, qt_hb_is_space( uc[c] ),
					       k ? &fixed[g + 1].advance : 0 );
	}
    }

    item->num_glyphs = n;
    item->has_positioning = TRUE;
    item->has_fixed = fractional;
    return 1;
}

#endif // QT_XFT2
