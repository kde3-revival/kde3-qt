/****************************************************************************
**
** Qt3 with the platform chosen at run time: the FreeType font engine of
** the Wayland back end (in the X11 build: the glyph rasterizer and the
** HarfBuzz face of QFontEngineXft; draw() is the Wayland back end's).
**
** The font is the one fontconfig chooses for the X11 font database's
** pattern (qfontdatabase_x11.cpp, as for Xft). This engine does what
** XftFontOpenPattern() and XftGlyphExtents() do with that pattern, without
** an X server: the same face size, load flags, hinting, LCD filter and
** render mode, the glyph metrics computed as Xft computes them (libXft
** 2.3: xftfreetype.c, XftFontInfoFill/XftFontOpenInfo; xftglyphs.c,
** XftFontLoadGlyphs), and the character map of Xft (the pattern's
** charset, then FcFreeTypeCharIndex). So the glyph ids, the advances, the
** metrics and the glyph images are those of QFontEngineXft on X11, and the
** shapers (QOpenType) see the same face.
**
** The glyph images are cached here; QPainter (qpainter_wl.cpp) composites
** them through the QGfx as XRender composites Xft's glyphs. Each glyph is
** cached per horizontal subpixel phase (a quarter of a pixel) of its
** origin, for positions in 26.6 fixed point (fractional positioning);
** Qt's advances are integers today, so only phase 0 is used.
**
** The faces are shared by the engines of the same (file, index), each
** engine with its own FT_Size. hbFont() gives the HarfBuzz font of an
** engine with the engine's character map and integer advances (stage 4 of
** the text plan, "QHbFont", identity mode).
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
#include "qtextengine_p.h"
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

/*
  XftFontLoadGlyphs(): the metrics come from the rendered image, so the
  image is always rendered (and kept).
*/
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
    glyphMemory += glyphHeaderSize + size;
    page[g & ( PageSize - 1 )] = glyph;
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
	for ( int i = 0; i < len; i++ )
	    advances[i] = that->glyph( glyphs[i], FALSE )->xOff;
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
    // as QFontEngineXft::recalcAdvances(), which scales the advances
    // again for every glyph
    for ( int i = 0; i < len; i++ ) {
	advances[i] = glyph( glyphs[i], FALSE )->xOff;
	if ( _scale != 1. ) {
	    for ( int i = 0; i < len; i++ )
		advances[i] = qRound( advances[i] * _scale );
	}
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
  HarfBuzz (stage 4 of the text plan: the engine's HarfBuzz font)
 *****************************************************************************/

static hb_bool_t qt_ft_hb_nominal_glyph( hb_font_t *, void *font_data, hb_codepoint_t unicode,
					 hb_codepoint_t *glyph, void * )
{
    QFontEngineFT *fe = (QFontEngineFT *)font_data;
    QChar c[2];
    int n = 1;
    if ( unicode >= 0x10000 ) {
	// one glyph per UTF-16 code unit in Qt 3: no glyph for a non-BMP
	// character here (as QFontEngineXft)
	*glyph = 0;
	return FALSE;
    }
    c[0] = QChar( (ushort)unicode );
    glyph_t g = 0;
    fe->stringToCMap( c, 1, &g, 0, &n, FALSE );
    *glyph = g;
    return g != 0;
}

static hb_position_t qt_ft_hb_h_advance( hb_font_t *, void *font_data, hb_codepoint_t glyph, void * )
{
    // the hinted integer advance of Xft, in 26.6 (identity mode)
    QFontEngineFT *fe = (QFontEngineFT *)font_data;
    return fe->glyph( glyph, FALSE )->xOff * 64;
}

static hb_bool_t qt_ft_hb_extents( hb_font_t *, void *font_data, hb_codepoint_t glyph,
				   hb_glyph_extents_t *extents, void * )
{
    QFontEngineFT *fe = (QFontEngineFT *)font_data;
    const QFtGlyph *g = fe->glyph( glyph, FALSE );
    extents->x_bearing = -g->x * 64;
    extents->y_bearing = g->y * 64;
    extents->width = g->width * 64;
    extents->height = -g->height * 64;
    return TRUE;
}

static hb_font_funcs_t *qt_ft_hb_funcs()
{
    static hb_font_funcs_t *funcs = 0;
    if ( !funcs ) {
	funcs = hb_font_funcs_create();
	hb_font_funcs_set_nominal_glyph_func( funcs, qt_ft_hb_nominal_glyph, 0, 0 );
	hb_font_funcs_set_glyph_h_advance_func( funcs, qt_ft_hb_h_advance, 0, 0 );
	hb_font_funcs_set_glyph_extents_func( funcs, qt_ft_hb_extents, 0, 0 );
	hb_font_funcs_make_immutable( funcs );
    }
    return funcs;
}

hb_font_t *QFontEngineFT::hbFont()
{
    if ( _hbFont || !face )
	return _hbFont;
    if ( !face->hbFace )
	face->hbFace = hb_ft_face_create_referenced( face->face );
    // the parent: HarfBuzz's own OpenType functions at the pixel size in
    // 26.6 (GPOS, anchors, variations); this engine's character map,
    // advances and extents on top of it
    hb_font_t *parent = hb_font_create( face->hbFace );
    hb_font_set_scale( parent, (int)xsize, (int)ysize );
    hb_font_set_ppem( parent, (uint)( xsize >> 6 ), (uint)( ysize >> 6 ) );
    _hbFont = hb_font_create_sub_font( parent );
    hb_font_destroy( parent );
    hb_font_set_funcs( _hbFont, qt_ft_hb_funcs(), this, 0 );
    return _hbFont;
}

#endif // QT_XFT2
