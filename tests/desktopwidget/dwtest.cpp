/*
  QDesktopWidget test: prints the screens Qt sees and how they change.

    dwtest print          print the screens and exit
    dwtest watch [secs]   print them again on every resized() signal
                          (default: run until killed)
    dwtest check          drive "xrandr --setmonitor/--delmonitor/-s" on
                          $DISPLAY and check numScreens(), the order
                          (primary first, then left to right),
                          screenNumber() and the signals; exits with the
                          number of failed checks

  "check" changes the RandR configuration of $DISPLAY: run it on a
  throw-away server only (tests/desktopwidget/run.sh starts Xephyr).
*/

#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qtimer.h>
#include <qvaluelist.h>
#include <qstringlist.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump( const char *why )
{
    QDesktopWidget *d = QApplication::desktop();
    printf( "%s: screens=%d primary=%d virtual=%d desktop=%dx%d\n", why,
	    d->numScreens(), d->primaryScreen(), (int)d->isVirtualDesktop(),
	    d->width(), d->height() );
    for ( int i = 0; i < d->numScreens(); ++i ) {
	QRect r = d->screenGeometry( i );
	QRect a = d->availableGeometry( i );
	printf( "  %d: %dx%d+%d+%d  available %dx%d+%d+%d  screenNumber(center)=%d\n",
		i, r.width(), r.height(), r.x(), r.y(),
		a.width(), a.height(), a.x(), a.y(),
		d->screenNumber( r.center() ) );
    }
    fflush( stdout );
}

class Watcher : public QObject
{
    Q_OBJECT
public:
    Watcher() : resizedCount( 0 ), workAreaCount( 0 ), lastResized( -2 ) {
	connect( QApplication::desktop(), SIGNAL(resized(int)), SLOT(resized(int)) );
	connect( QApplication::desktop(), SIGNAL(workAreaResized(int)),
		 SLOT(workAreaResized(int)) );
    }
    int resizedCount;
    int workAreaCount;
    int lastResized;
    bool verbose;
public slots:
    void resized( int s ) {
	++resizedCount;
	lastResized = s;
	if ( verbose ) {
	    QString why = QString( "resized(%1)" ).arg( s );
	    dump( why.latin1() );
	}
    }
    void workAreaResized( int ) { ++workAreaCount; }
};

// --- check mode ----------------------------------------------------------

struct Step {
    const char *cmd;      // shell command (xrandr ...), 0 = none
    const char *expect;   // "WxH+X+Y WxH+X+Y ..." in Qt screen order
    bool signal;          // resized() expected
};

// Xephyr has one output ("default"); --setmonitor creates RandR 1.5
// monitors without changing the root window size
static const Step steps[] = {
    { 0, "1600x900+0+0", FALSE },
    // two monitors side by side, the right one holds the output
    { "xrandr --setmonitor L 800/200x900/200+0+0 none"
      " && xrandr --setmonitor R 800/200x900/200+800+0 default",
      "800x900+0+0 800x900+800+0", TRUE },
    // R becomes the primary monitor: it is screen 0
    // (the monitors of "xrandr --setmonitor" are primary only if named
    // "*name"; they do not follow the primary output)
    { "xrandr --delmonitor R"
      " && xrandr --setmonitor '*R' 800/200x900/200+800+0 default",
      "800x900+800+0 800x900+0+0", TRUE },
    // a third one at the bottom left, overlapping L: ordered left to
    // right, then top to bottom
    { "xrandr --setmonitor T 400/100x300/100+0+600 none",
      "800x900+800+0 800x900+0+0 400x300+0+600", TRUE },
    { "xrandr --delmonitor T", "800x900+800+0 800x900+0+0", TRUE },
    // nothing changes: no signal
    { "xrandr --output default --primary", "800x900+800+0 800x900+0+0", FALSE },
    // the root window shrinks
    { "xrandr --delmonitor L && xrandr --delmonitor R && xrandr -s 1280x1024",
      "1280x1024+0+0", TRUE },
    { "xrandr -s 1600x900", "1600x900+0+0", TRUE },
    { 0, 0, FALSE }
};

class Checker : public QObject
{
    Q_OBJECT
public:
    Checker() : step( 0 ), failed( 0 ) { watcher.verbose = FALSE; }
    Watcher watcher;
    int step;
    int failed;

    bool check( const Step &s ) {
	QDesktopWidget *d = QApplication::desktop();
	QStringList want = QStringList::split( ' ', s.expect );
	QStringList got;
	for ( int i = 0; i < d->numScreens(); ++i ) {
	    QRect r = d->screenGeometry( i );
	    got << QString( "%1x%2+%3+%4" ).arg( r.width() ).arg( r.height() )
		.arg( r.x() ).arg( r.y() );
	}
	bool ok = ( want == got );
	if ( !ok )
	    printf( "FAIL: screens %s, expected %s\n", got.join( " " ).latin1(),
		    want.join( " " ).latin1() );
	if ( d->primaryScreen() != 0 ) {
	    printf( "FAIL: primaryScreen() = %d\n", d->primaryScreen() );
	    ok = FALSE;
	}
	for ( int i = 0; i < d->numScreens(); ++i ) {
	    QRect r = d->screenGeometry( i );
	    // the center of a screen may be covered by a screen listed
	    // before it (overlapping monitors); check the bottom right
	    int n = d->screenNumber( r.bottomRight() );
	    if ( n != i && !d->screenGeometry( n ).contains( r.bottomRight() ) ) {
		printf( "FAIL: screenNumber(bottomRight of %d) = %d\n", i, n );
		ok = FALSE;
	    }
	}
	if ( s.signal != ( watcher.resizedCount > 0 ) ) {
	    printf( "FAIL: resized() emitted %d times, expected %s\n",
		    watcher.resizedCount, s.signal ? "some" : "none" );
	    ok = FALSE;
	}
	if ( watcher.resizedCount != watcher.workAreaCount ) {
	    printf( "FAIL: resized() %d times, workAreaResized() %d times\n",
		    watcher.resizedCount, watcher.workAreaCount );
	    ok = FALSE;
	}
	return ok;
    }

public slots:
    void run() {
	const Step &s = steps[step];
	if ( !s.expect ) {
	    printf( "=== %d check(s) failed\n", failed );
	    qApp->exit( failed );
	    return;
	}
	watcher.resizedCount = watcher.workAreaCount = 0;
	if ( s.cmd ) {
	    printf( "--- %s\n", s.cmd );
	    fflush( stdout );
	    if ( system( s.cmd ) != 0 )
		printf( "FAIL: command failed\n" );
	}
	// let the RandR events arrive
	QTimer::singleShot( 700, this, SLOT(verify()) );
    }
    void verify() {
	const Step &s = steps[step];
	if ( !check( s ) ) {
	    ++failed;
	    dump( "state" );
	} else {
	    printf( "ok   %s (resized %d)\n", s.expect, watcher.resizedCount );
	}
	++step;
	run();
    }
};

int main( int argc, char **argv )
{
    QApplication app( argc, argv );
    const char *mode = argc > 1 ? argv[1] : "print";

    if ( !strcmp( mode, "print" ) ) {
	dump( "screens" );
	return 0;
    }
    if ( !strcmp( mode, "watch" ) ) {
	Watcher w;
	w.verbose = TRUE;
	dump( "screens" );
	if ( argc > 2 )
	    QTimer::singleShot( atoi( argv[2] ) * 1000, &app, SLOT(quit()) );
	return app.exec();
    }
    if ( !strcmp( mode, "check" ) ) {
	Checker c;
	QTimer::singleShot( 0, &c, SLOT(run()) );
	return app.exec();
    }
    fprintf( stderr, "usage: %s [print | watch [secs] | check]\n", argv[0] );
    return 2;
}

#include "dwtest.moc"
