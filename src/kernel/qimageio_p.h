/****************************************************************************
**
** Limits shared by the image readers
**
** Copyright (C) 1992-2008 Trolltech ASA.  All rights reserved.
**
** This file is part of the kernel module of the Qt GUI Toolkit.
**
** This file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free
** Software Foundation and appearing in the files LICENSE.GPL2
** and LICENSE.GPL3 included in the packaging of this file.
** Alternatively you may (at your option) use any later version
** of the GNU General Public License if such license has been
** publicly approved by Trolltech ASA (or its successors, if any)
** and the KDE Free Qt Foundation.
**
** This file may be used under the terms of the Q Public License as
** defined by Trolltech ASA and appearing in the file LICENSE.QPL
** included in the packaging of this file.
**
** This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
** INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE. Trolltech reserves all rights not granted
** herein.
**
**********************************************************************/

#ifndef QIMAGEIO_P_H
#define QIMAGEIO_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  This header file may
// change from version to version without notice, or even be
// removed.
//
// We mean it.
//
//

#ifndef QT_H
#include "qimage.h"
#endif // QT_H
#include <string.h>

// The image readers size the QImage they create from header fields
// alone, before any pixel data has arrived: a few bytes of a file can ask
// for gigabytes. A reader refuses an image whose bitmap would be larger
// than these limits (bytes as QImage::create() allocates them, at the
// depth the reader creates).
//
// GIF: 256 MiB (2^28 bytes, 8192 x 8192 at 32 bpp), the default
// allocation limit of Qt 6 (QImageReader::allocationLimit()). A GIF needs
// no pixel data at all to make its decoder create the whole bitmap (the
// logical screen, or a frame's offset plus size), and GIFs larger than 64
// megapixels do not occur in practice.
//
// The other formats (BMP, PNM, XPM, PNG, JPEG, MNG): 1 GiB (2^30 bytes,
// 16384 x 16384 at 32 bpp), room for photographs and scans of up to 268
// megapixels; QImage::create() itself stops below 2 GiB.
static const Q_UINT64 qt_gif_max_image_bytes = Q_UINT64_C(1) << 28;
static const Q_UINT64 qt_max_image_bytes = Q_UINT64_C(1) << 30;

// TRUE if a w x h image of the given depth (1, 8, 16 or 32) is not empty
// and its bitmap (QImage::create()'s padded scan lines) fits in limit
// bytes.
static inline bool qt_image_size_ok( Q_INT64 w, Q_INT64 h, int depth,
				     Q_UINT64 limit = qt_max_image_bytes )
{
    if ( w <= 0 || h <= 0 || depth <= 0 || w > Q_INT64_C(0x7fffffff) )
	return FALSE;
    const Q_UINT64 bpl = ( ( Q_UINT64(w) * depth + 31 ) / 32 ) * 4;
    return Q_UINT64(h) <= limit / bpl;
}

// Pixels of an 8 or 1 bpp image whose value is not in its color table
// (a BMP, PGM or PNG palette with fewer entries than the pixels use)
// would make every reader of the image (pixel(), convertDepth(),
// QPixmap::convertFromImage(), ...) read beyond the color table: such
// pixels are set to 0. Images whose pixels are all in their color table
// are left as they are.
static inline void qt_clamp_color_indexes( QImage &image )
{
    const int ncols = image.numColors();
    if ( image.isNull() || ncols <= 0 )
	return;
    if ( image.depth() == 8 && ncols < 256 ) {
	for ( int y = 0; y < image.height(); ++y ) {
	    uchar *p = image.scanLine( y );
	    for ( int x = 0; x < image.width(); ++x )
		if ( p[x] >= ncols )
		    p[x] = 0;
	}
    } else if ( image.depth() == 1 && ncols < 2 ) {
	// all pixels must be 0; the padding bits of the scan lines too
	memset( image.bits(), 0, image.numBytes() );
    }
}

#endif // QIMAGEIO_P_H
