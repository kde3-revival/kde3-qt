/****************************************************************************
**
** Implementation of QDesktopWidget class.
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
** Please review the following information to ensure GNU General
** Public Licensing requirements will be met:
** http://trolltech.com/products/qt/licenses/licensing/opensource/.
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://trolltech.com/products/qt/licenses/licensing/licensingoverview
** or contact the sales department at sales@trolltech.com.
**
** This file may be used under the terms of the Q Public License as
** defined by Trolltech ASA and appearing in the file LICENSE.QPL
** included in the packaging of this file.  Licensees holding valid Qt
** Commercial licenses may use this file in accordance with the Qt
** Commercial License Agreement provided with the Software.
**
** This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
** INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE. Trolltech reserves all rights not granted
** herein.
**
**********************************************************************/

#include "qdesktopwidget.h"
#include "qapplication.h"
#include "qobjectlist.h"
#include "qt_x11_p.h"

// defined in qwidget_x11.cpp
extern int qt_x11_create_desktop_on_screen;

// defined in qapplication_x11.cpp
extern Atom qt_net_workarea;
extern bool qt_net_supports(Atom atom);

// function to update the workarea of the screen
static bool qt_desktopwidget_workarea_dirty = TRUE;
void qt_desktopwidget_update_workarea()
{
    qt_desktopwidget_workarea_dirty = TRUE;
}


class QSingleDesktopWidget : public QWidget
{
public:
    QSingleDesktopWidget();
    ~QSingleDesktopWidget();
};

QSingleDesktopWidget::QSingleDesktopWidget()
    : QWidget( 0, "desktop", WType_Desktop )
{
}

QSingleDesktopWidget::~QSingleDesktopWidget()
{
    while ( children() )
        removeChild( children()->getFirst() );
}


class QDesktopWidgetPrivate
{
public:
    QDesktopWidgetPrivate();
    ~QDesktopWidgetPrivate();

    void init();
    void initScreens( Display *dpy );

    bool use_xinerama; // virtual desktop: Xinerama or RandR monitors
    int defaultScreen;
    int screenCount;

    // per X screen desktop widgets (not virtual desktop only); the
    // screen count can change later, so remember the allocated size
    int screensSize;
    int screensDefault;
    QWidget **screens;
    QRect *rects;
    QRect *workareas;
};

QDesktopWidgetPrivate::QDesktopWidgetPrivate()
    : use_xinerama(FALSE), defaultScreen(0), screenCount(1),
      screensSize( 0 ), screensDefault( 0 ),
      screens( 0 ), rects( 0 ), workareas( 0 )
{
}

QDesktopWidgetPrivate::~QDesktopWidgetPrivate()
{
    if ( screens ) {
	for ( int i = 0; i < screensSize; ++i ) {
	    if (i == screensDefault) continue;
	    delete screens[i];
	    screens[i] = 0;
	}

	delete [] screens;
    }

    if ( rects )     delete [] rects;
    if ( workareas ) delete [] workareas;
}

#ifndef QT_NO_XRANDR
// RandR 1.5 monitors (XRRGetMonitors) need both the client library
// headers and the server to know the request.
#  if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 5)
#    define QT_XRANDR_MONITORS
#  endif
#endif

#ifdef QT_XRANDR_MONITORS
// defined in qapplication_x11.cpp
extern bool qt_use_xrandr;
extern int qt_xrandr_major;
extern int qt_xrandr_minor;

/*
  Returns the active RandR monitors of the default screen in
  \a rects (allocated with new[], or 0 when there are none), the primary
  monitor first and the others left to right, then top to bottom.
  Monitors include the CRTC-based ones and those defined with
  "xrandr --setmonitor".
*/
static int qt_xrandr_monitor_rects( QRect **rects )
{
    *rects = 0;
    if ( !qt_use_xrandr ||
	 qt_xrandr_major < 1 || ( qt_xrandr_major == 1 && qt_xrandr_minor < 5 ) )
	return 0;

    Display *dpy = QPaintDevice::x11AppDisplay();
    int n = 0;
    XRRMonitorInfo *mon = XRRGetMonitors( dpy, QPaintDevice::x11AppRootWindow(),
					  True, &n );
    if ( !mon )
	return 0;

    int count = 0;
    QRect *r = n > 0 ? new QRect[ n ] : 0;
    int primary = -1;
    for ( int i = 0; i < n; ++i ) {
	if ( mon[i].width <= 0 || mon[i].height <= 0 )
	    continue;
	if ( mon[i].primary && primary < 0 )
	    primary = count;
	r[count++].setRect( mon[i].x, mon[i].y, mon[i].width, mon[i].height );
    }
    XRRFreeMonitors( mon );

    if ( count == 0 ) {
	delete [] r;
	return 0;
    }

    // primary first
    if ( primary > 0 ) {
	QRect p = r[primary];
	for ( int i = primary; i > 0; --i )
	    r[i] = r[i - 1];
	r[0] = p;
    }
    // the others by x, then by y (insertion sort: a handful of monitors)
    int first = primary >= 0 ? 1 : 0;
    for ( int i = first + 1; i < count; ++i ) {
	QRect k = r[i];
	int j = i - 1;
	while ( j >= first &&
		( r[j].x() > k.x() || ( r[j].x() == k.x() && r[j].y() > k.y() ) ) ) {
	    r[j + 1] = r[j];
	    --j;
	}
	r[j + 1] = k;
    }

    *rects = r;
    return count;
}
#endif // QT_XRANDR_MONITORS

void QDesktopWidgetPrivate::init()
{
    // the arrays are kept when the number of screens stays the same: the
    // references screenGeometry() and availableGeometry() return stay
    // valid across a change of the monitors
    int oldCount = rects ? screenCount : -1;
    QRect *oldRects = rects;
    QRect *oldWorkareas = workareas;
    rects = 0;
    workareas = 0;

    Display *dpy = QPaintDevice::x11AppDisplay();
    bool done = FALSE;

#ifdef QT_XRANDR_MONITORS
    // RandR monitors describe one X screen; with several X screens
    // (Zaphod mode) each one is a Qt screen as before.
    if ( ScreenCount( dpy ) == 1 ) {
	int n = qt_xrandr_monitor_rects( &rects );
	if ( n > 0 ) {
	    use_xinerama = TRUE;
	    defaultScreen = 0;
	    screenCount = n;
	    workareas = new QRect[ screenCount ];
	    done = TRUE;
	}
    }
#endif // QT_XRANDR_MONITORS
    if ( !done )
	initScreens( dpy );

    if ( oldRects && oldCount == screenCount ) {
	for ( int i = 0; i < screenCount; ++i ) {
	    oldRects[i] = rects[i];
	    oldWorkareas[i] = QRect();
	}
	delete [] rects;
	delete [] workareas;
	rects = oldRects;
	workareas = oldWorkareas;
    } else {
	delete [] oldRects;
	delete [] oldWorkareas;
    }
}

void QDesktopWidgetPrivate::initScreens( Display *dpy )
{
    // get the screen count
#ifndef QT_NO_XINERAMA
    XineramaScreenInfo *xinerama_screeninfo = 0;
    int unused;
    use_xinerama = (XineramaQueryExtension(dpy, &unused, &unused) &&
		    XineramaIsActive(dpy));

    if (use_xinerama) {
	xinerama_screeninfo = XineramaQueryScreens(dpy, &screenCount);
	if ( !xinerama_screeninfo || screenCount < 1 ) {
	    // active but no screens: treat like no Xinerama
	    if ( xinerama_screeninfo )
		XFree( xinerama_screeninfo );
	    xinerama_screeninfo = 0;
	    use_xinerama = FALSE;
	}
    }
    if (use_xinerama) {
	defaultScreen = 0;
    } else
#endif // QT_NO_XINERAMA
    {
	defaultScreen = DefaultScreen(dpy);
	screenCount = ScreenCount(dpy);
    }

    rects     = new QRect[ screenCount ];
    workareas = new QRect[ screenCount ];

    // get the geometry of each screen
    int i, x, y, w, h;
    for ( i = 0; i < screenCount; i++ ) {

#ifndef QT_NO_XINERAMA
	if (use_xinerama) {
	    x = xinerama_screeninfo[i].x_org;
	    y = xinerama_screeninfo[i].y_org;
	    w = xinerama_screeninfo[i].width;
	    h = xinerama_screeninfo[i].height;
	} else
#endif // QT_NO_XINERAMA
	    {
		x = 0;
		y = 0;
		w = WidthOfScreen(ScreenOfDisplay(dpy, i));
		h = HeightOfScreen(ScreenOfDisplay(dpy, i));
	    }

	rects[i].setRect(x, y, w, h);
	workareas[i] = QRect();
    }

#ifndef QT_NO_XINERAMA
    if (xinerama_screeninfo)
	XFree(xinerama_screeninfo);
#endif // QT_NO_XINERAMA

}

/*
  Called by QApplication for every RandR event on the root window of
  the default screen (the monitors can change while the root window
  keeps its size: an output switched on or off, moved, made primary, or
  "xrandr --setmonitor"). Re-reads the screens if \a reread is TRUE
  (FALSE when a resize event just did it) and returns the lowest Qt
  screen number whose geometry differs from \a old (\a oldCount
  entries), or -1 if nothing changed.
*/
int qt_desktopwidget_screens_changed( QDesktopWidgetPrivate *d, bool reread,
				      const QRect *old, int oldCount )
{
    if ( reread ) {
	d->init();
	qt_desktopwidget_workarea_dirty = TRUE;
    }
    int n = QMAX( oldCount, d->screenCount );
    for ( int i = 0; i < n; ++i ) {
	if ( i >= oldCount || i >= d->screenCount || old[i] != d->rects[i] )
	    return i;
    }
    return -1;
}

// the QDesktopWidget itself will be created on the default screen
// as qt_x11_create_desktop_on_screen defaults to -1
QDesktopWidget::QDesktopWidget()
    : QWidget( 0, "desktop", WType_Desktop )
{
    d = new QDesktopWidgetPrivate();

    /*
      we don't call d->init() here, since the initial resize event
      will end up calling init() a second time, which is inefficient.
      instead, for the sending of all posted event to the desktop
      widget (including the initial resize event, which calls
      d->init()).
    */
    QApplication::sendPostedEvents( this, 0 );
}

QDesktopWidget::~QDesktopWidget()
{
    delete d;
}

bool QDesktopWidget::isVirtualDesktop() const
{
    return d->use_xinerama;
}

int QDesktopWidget::primaryScreen() const
{
    return d->defaultScreen;
}

int QDesktopWidget::numScreens() const
{
    return d->screenCount;
}

QWidget *QDesktopWidget::screen( int screen )
{
    if (d->use_xinerama)
	return this;

    if ( screen < 0 || screen >= d->screenCount )
	screen = d->defaultScreen;

    if ( ! d->screens ) {
	d->screensSize = d->screenCount;
	d->screensDefault = d->defaultScreen;
	d->screens = new QWidget*[ d->screensSize ];
	memset( d->screens, 0, d->screensSize * sizeof( QWidget * ) );
	d->screens[ d->screensDefault ] = this;
    }
    if ( screen >= d->screensSize )
	screen = d->screensDefault;

    if ( ! d->screens[screen] ||               // not created yet
	 ! d->screens[screen]->isDesktop() ) { // reparented away
	qt_x11_create_desktop_on_screen = screen;
	d->screens[screen] = new QSingleDesktopWidget;
	qt_x11_create_desktop_on_screen = -1;
    }

    return d->screens[screen];
}

const QRect& QDesktopWidget::availableGeometry( int screen ) const
{
    if ( qt_desktopwidget_workarea_dirty ) {
	// the workareas are dirty, invalidate them
	for ( int i = 0; i < d->screenCount; ++i )
	    d->workareas[i] = QRect();
	qt_desktopwidget_workarea_dirty = FALSE;
    }

    if ( screen < 0 || screen >= d->screenCount )
	screen = d->defaultScreen;

    if ( d->workareas[screen].isValid() )
	return d->workareas[screen];

    if ( ! isVirtualDesktop() && qt_net_supports( qt_net_workarea ) ) {
	Atom ret;
	int format, e;
	unsigned char *data = 0;
	unsigned long nitems, after;

	e = XGetWindowProperty( QPaintDevice::x11AppDisplay(),
				QPaintDevice::x11AppRootWindow( screen ),
				qt_net_workarea, 0, 4, False, XA_CARDINAL,
				&ret, &format, &nitems, &after, &data );

	if (e == Success && ret == XA_CARDINAL &&
	    format == 32 && nitems == 4) {
	    long *workarea = (long *) data;
	    d->workareas[screen].setRect( workarea[0], workarea[1],
					  workarea[2], workarea[3] );
	} else {
	    d->workareas[screen] = screenGeometry(screen);
	}
	if ( data )
	    XFree( data );
    } else {
	d->workareas[screen] = screenGeometry(screen);
    }

    return d->workareas[screen];
}

const QRect& QDesktopWidget::screenGeometry( int screen ) const
{
    if ( screen < 0 || screen >= d->screenCount )
	screen = d->defaultScreen;

    return d->rects[ screen ];
}

int QDesktopWidget::screenNumber( QWidget *widget ) const
{
    if ( !widget )
	return d->defaultScreen;

    if (d->use_xinerama) {
	// this is how we do it for xinerama (and RandR monitors)
	QRect frame = widget->frameGeometry();
	if ( !widget->isTopLevel() )
	    frame.moveTopLeft( widget->mapToGlobal( QPoint( 0, 0 ) ) );

	int maxSize = -1;
	int maxScreen = -1;

	for ( int i = 0; i < d->screenCount; ++i ) {
	    QRect sect = d->rects[i].intersect( frame );
	    int size = sect.width() * sect.height();
	    if ( size > maxSize && sect.width() > 0 && sect.height() > 0 ) {
		maxSize = size;
		maxScreen = i;
	    }
	}
	return maxScreen;
    }

    return widget->x11Screen();
}

int QDesktopWidget::screenNumber( const QPoint &point ) const
{
    for ( int i = 0; i < d->screenCount; ++i ) {
	if ( d->rects[i].contains( point ) )
	    return i;
    }
    return -1;
}

void QDesktopWidget::resizeEvent( QResizeEvent *event )
{
    d->init();
    qt_desktopwidget_workarea_dirty = TRUE;
    QWidget::resizeEvent( event );
}
