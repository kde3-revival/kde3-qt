/****************************************************************************
**
** GLib based implementation of the QEventLoop class for X11
**
** Copyright (C) 2026 The kde3-revival project.
**
** This file is part of the kernel module of the Qt GUI Toolkit.
**
** This file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free
** Software Foundation and appearing in the files LICENSE.GPL2
** and LICENSE.GPL3 included in the packaging of this file.
**
** This file may be used under the terms of the Q Public License as
** defined by Trolltech ASA and appearing in the file LICENSE.QPL
** included in the packaging of this file.
**
** This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
** INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE.
**
**********************************************************************/

/*
  The event loop of a Qt built with "configure -glib-eventloop".

  It runs on the default GMainContext, so that GLib based libraries (GIO,
  GDBus, libpulse's glib main loop, ...) work in a Qt program. The context
  is acquired by the thread that creates the event loop (the GUI thread)
  for the lifetime of the QEventLoop. Qt's event sources are GSources:

  source	priority		ready when
  ------	--------		----------
  posted	G_PRIORITY_DEFAULT	events were posted since the last send
  X11		G_PRIORITY_DEFAULT	the X fd is readable or Xlib has queued
					events (XEventsQueued(QueuedAfterFlush))
  sockets	G_PRIORITY_DEFAULT	the fd of a socket notifier is ready (one
					g_source_add_unix_fd() tag per notifier)
  timers	G_PRIORITY_DEFAULT	a timer with a non-zero interval expired
  zero timers	G_PRIORITY_DEFAULT_IDLE	there is a zero timer (QTimer::start( 0 ));
					idle priority so that they do not starve
					g_idle_add() sources, as in Qt 4/5

  GLib dispatches the ready sources of the highest priority in the order
  they were attached, so one iteration handles posted events, X events,
  socket notifiers and timers in the same order as the select() loop
  does. All Qt sources may recurse (nested event loops).

  The timer and socket notifier bookkeeping is shared with the select()
  loop (qeventloop_unix.cpp), as is the X event processing
  (qeventloop_x11_p.h). The timers use the monotonic clock in this build.

  processEvents( flags ):
  - without Exclude* flags it runs one g_main_context_iteration(), which
    also dispatches foreign GLib sources;
  - with ExcludeUserInput, ExcludeSocketNotifiers or ExcludeTimers (0x08)
    it runs one iteration of Qt's own sources only, with its own poll(),
    like the select() loop: foreign sources (GDBus messages, ...) are
    neither dispatched nor allowed to wake it up, so the clipboard and
    drag and drop waits and the like are not re-entered by them;
  - it returns TRUE if Qt delivered something (X events, socket notifier
    activations, timers with a non-zero interval), like the select() loop.

  qt_mutex is released only around poll(), via the context's poll
  function, so GLib callbacks run with it held as Qt's do. Outside of any
  QEventLoop::processEvents() call (e.g. a g_main_loop_run() before
  QApplication::exec()), the Qt sources take it themselves, foreign
  sources run without it.

  Note for code using GIO with Qt 3: <gio/gio.h> must be included before
  the Qt headers, because Qt 3 #defines "signals" (GDBusInterfaceInfo has
  a member of that name).
*/

// Use no GLib API newer than what configure requires.
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_36
#define GLIB_VERSION_MAX_ALLOWED GLIB_VERSION_2_36
#include <glib.h>

#include "qeventloop_p.h" // includes qplatformdefs.h
#include "qeventloop.h"
#include "qapplication.h"
#include "qcolor_p.h"
#include "qmetaobject.h"
#include "qt_x11_p.h"
#include "qeventloop_x11_p.h"

#if defined(QT_THREAD_SUPPORT)
#  include "qmutex.h"
#  include "qthread.h"
#endif // QT_THREAD_SUPPORT

#include <atomic>
#include <memory>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

// from qapplication.cpp
extern bool qt_is_gui_used;
extern uint qGlobalPostedEventsCount();

// from qeventloop_unix.cpp
extern timeval *qt_wait_timer();
extern void cleanupTimers();

namespace {

struct SourceDeleter
{
    void operator()( GSource *source ) const
    {
	g_source_destroy( source );
	g_source_unref( source );
    }
};
typedef std::unique_ptr<GSource, SourceDeleter> SourcePtr;

// processEvents() flag of the X11 select() loop: do not activate timers
const uint ExcludeTimers = 0x08;

} // namespace

struct QEventLoopGlib
{
    QEventLoopGlib( QEventLoop *q, QEventLoopPrivate *d );
    ~QEventLoopGlib();

    void setupX11();
    void cleanupX11();
    GSource *newSource( GSourceFuncs *funcs, int priority, const char *name );

    void emitAboutToBlock();
    void emitAwake();

    QEventLoop *q;
    QEventLoopPrivate *d;

    GMainContext *context;
    bool acquired;			// context acquired by this thread
    GPollFunc oldPollFunc;

    SourcePtr postedSource;
    SourcePtr x11Source;
    SourcePtr socketSource;
    SourcePtr timerSource;
    SourcePtr zeroTimerSource;
    gpointer x11Tag;

    // incremented for each X event, socket notifier activation and timer
    // with a non-zero interval; processEvents() returns whether it changed
    unsigned int delivered;

    // posted events: wakeUp() increments postedSerial (postEvent() calls
    // it), the posted source sends them while it differs from sentSerial
    std::atomic<unsigned int> postedSerial;
    unsigned int sentSerial;

    // wake-ups: wakeUp() from the GUI thread just sets wakePending (the
    // poll function then does not block); from other threads it calls
    // g_main_context_wakeup() and, while processEvents() polls for Qt's
    // sources only, writes to d->thread_pipe
    bool wakePending;
    std::atomic<bool> crossWake;
    std::atomic<bool> privatePolling;
#if defined(QT_THREAD_SUPPORT)
    Qt::HANDLE guiThread;
    QMutex **qtMutex;			// &QApplication::qt_mutex
#endif

    // levels of qt_mutex taken by processEvents() and the Qt sources on the
    // GUI thread; the poll function releases one of them, like the select()
    // loop does
    int mutexDepth;
    bool inPoll;			// in the poll function

    int awakeSignal;			// QEventLoop::awake()
    int aboutToBlockSignal;		// QEventLoop::aboutToBlock()
    int guiThreadAwakeSignal;		// QApplication::guiThreadAwake()
};

// the event loop whose poll function is installed
static QEventLoopGlib *qt_glib_loop = 0;

namespace {

// All Qt sources: a GSource with a pointer back to the event loop.
struct QtSource
{
    GSource source;
    QEventLoopGlib *loop;
};

inline QEventLoopGlib *loopOf( GSource *source )
{
    return reinterpret_cast<QtSource *>( source )->loop;
}

/*
  Holds qt_mutex while a Qt source is dispatched outside of any
  processEvents() call, i.e. from a g_main_context_iteration() of
  someone else. Inside processEvents() it is already held.
*/
class DispatchLock
{
public:
    explicit DispatchLock( QEventLoopGlib *g )
#if defined(QT_THREAD_SUPPORT)
	: loop( g ), mutex( g->mutexDepth == 0 ? *g->qtMutex : 0 )
    {
	if ( mutex ) {
	    mutex->lock();
	    ++loop->mutexDepth;
	}
    }
    ~DispatchLock()
    {
	if ( mutex ) {
	    --loop->mutexDepth;
	    mutex->unlock();
	}
    }

private:
    QEventLoopGlib *loop;
    QMutex *mutex;
#else
    {
	Q_UNUSED( g );
    }
#endif

    DispatchLock( const DispatchLock & ) = delete;
    DispatchLock &operator=( const DispatchLock & ) = delete;
};

/*
  Releases the qt_mutex level taken by the innermost processEvents() (or
  Qt source) while blocking, like the select() loop does around select().
*/
class PollUnlock
{
public:
    explicit PollUnlock( QEventLoopGlib *g )
#if defined(QT_THREAD_SUPPORT)
	: mutex( g->mutexDepth > 0 ? *g->qtMutex : 0 )
    {
	if ( mutex )
	    mutex->unlock();
    }
    ~PollUnlock()
    {
	if ( mutex )
	    mutex->lock();
    }

private:
    QMutex *mutex;
#else
    {
	Q_UNUSED( g );
    }
#endif

    PollUnlock( const PollUnlock & ) = delete;
    PollUnlock &operator=( const PollUnlock & ) = delete;
};


/*****************************************************************************
  Posted events
 *****************************************************************************/

gboolean postedPrepare( GSource *source, gint *timeout )
{
    QEventLoopGlib *g = loopOf( source );
    *timeout = -1;
    return g->postedSerial.load() != g->sentSerial;
}

gboolean postedCheck( GSource *source )
{
    QEventLoopGlib *g = loopOf( source );
    return g->postedSerial.load() != g->sentSerial;
}

gboolean postedDispatch( GSource *source, GSourceFunc, gpointer )
{
    QEventLoopGlib *g = loopOf( source );
    if ( g->d->shortcut )
	return G_SOURCE_CONTINUE;
    DispatchLock lock( g );
    // Events posted while sending (also from a nested event loop, which
    // sends the events this call has not reached yet) are sent next time.
    unsigned int serial = g->postedSerial.load();
    QApplication::sendPostedEvents();
    g->sentSerial = serial;
    return G_SOURCE_CONTINUE;
}

GSourceFuncs postedFuncs = { postedPrepare, postedCheck, postedDispatch, 0, 0, 0 };


/*****************************************************************************
  X11 events
 *****************************************************************************/

gboolean x11Prepare( GSource *, gint *timeout )
{
    *timeout = -1;
    // flushes the output buffer and reads what has arrived; events that
    // are already in Xlib's queue would not make the fd readable
    return XEventsQueued( QPaintDevice::x11AppDisplay(), QueuedAfterFlush ) > 0;
}

gboolean x11Check( GSource *source )
{
    QEventLoopGlib *g = loopOf( source );
    return ( g_source_query_unix_fd( source, g->x11Tag ) & G_IO_IN ) ||
	XEventsQueued( QPaintDevice::x11AppDisplay(), QueuedAlready ) > 0;
}

gboolean x11Dispatch( GSource *source, GSourceFunc, gpointer )
{
    QEventLoopGlib *g = loopOf( source );
    QEventLoopPrivate *d = g->d;
    if ( d->shortcut )
	return G_SOURCE_CONTINUE;
    DispatchLock lock( g );

    // the X11 part of the select() loop's processEvents()
    QApplication::sendPostedEvents();
    XEvent event;
    int nevents = 0;
    QtX11EventsResult result = qt_x11_process_events( d, QEventLoop::AllEvents,
						      event, nevents );
    g->delivered += nevents;
    if ( result == QtX11EventsDone && !d->shortcut )
	QApplication::sendPostedEvents();
    return G_SOURCE_CONTINUE;
}

GSourceFuncs x11Funcs = { x11Prepare, x11Check, x11Dispatch, 0, 0, 0 };


/*****************************************************************************
  Socket notifiers
 *****************************************************************************/

const GIOCondition socketConditions[3] = { G_IO_IN, G_IO_OUT, G_IO_PRI };

// GLib defines the G_IO_* conditions as the poll() flags
static_assert( G_IO_IN == POLLIN && G_IO_OUT == POLLOUT && G_IO_PRI == POLLPRI &&
	       G_IO_ERR == POLLERR && G_IO_HUP == POLLHUP && G_IO_NVAL == POLLNVAL,
	       "GIOCondition differs from the poll() flags" );

// whether poll()'s result means what select() reports for the notifier type
inline bool socketReady( int type, unsigned int revents )
{
    switch ( type ) {
    case QSocketNotifier::Read:
	return revents & ( G_IO_IN | G_IO_HUP | G_IO_ERR );
    case QSocketNotifier::Write:
	return revents & ( G_IO_OUT | G_IO_HUP | G_IO_ERR );
    default:
	return revents & G_IO_PRI;
    }
}

void disableInvalidNotifiers( const std::vector<QSocketNotifier *> &invalid )
{
    // as the select() loop does on EBADF
    static const char *t[] = { "Read", "Write", "Exception" };
    for ( QSocketNotifier *notifier : invalid ) {
	qWarning( "QSocketNotifier: invalid socket %d and type '%s', disabling...",
		  notifier->socket(), t[notifier->type()] );
	notifier->setEnabled( FALSE );
    }
}

// no prepare and check: GLib dispatches the source when one of its fds
// has events
gboolean socketDispatch( GSource *source, GSourceFunc, gpointer )
{
    QEventLoopGlib *g = loopOf( source );
    QEventLoopPrivate *d = g->d;
    if ( d->shortcut )
	return G_SOURCE_CONTINUE;
    DispatchLock lock( g );

    std::vector<QSocketNotifier *> invalid;
    for ( int type = 0; type < 3; ++type ) {
	QPtrList<QSockNot> *list = d->sn_vec[type].list;
	if ( !list )
	    continue;
	QPtrListIterator<QSockNot> it( *list );
	for ( QSockNot *sn; ( sn = it.current() ); ++it ) {
	    if ( !sn->tag )
		continue;
	    unsigned int revents = g_source_query_unix_fd( source, sn->tag );
	    if ( revents & G_IO_NVAL ) {
		invalid.push_back( sn->obj );
	    } else if ( socketReady( type, revents ) ) {
		qt_sn_set_pending( d, sn );
	    } else if ( revents & ( G_IO_HUP | G_IO_ERR ) ) {
		// An exception notifier on a hung up fd: select() would not
		// report it, but poll() always does. No out-of-band data can
		// come any more, stop polling the fd.
		g_source_remove_unix_fd( source, sn->tag );
		sn->tag = 0;
	    }
	}
    }
    disableInvalidNotifiers( invalid );
    g->delivered += g->q->activateSocketNotifiers();
    return G_SOURCE_CONTINUE;
}

GSourceFuncs socketFuncs = { 0, 0, socketDispatch, 0, 0, 0 };


/*****************************************************************************
  Timers
 *****************************************************************************/

gboolean timerPrepare( GSource *, gint *timeout )
{
    *timeout = qt_timers_wait();	// rounded up
    return *timeout == 0;
}

gboolean timerCheck( GSource * )
{
    return qt_timers_wait() == 0;
}

gboolean timerDispatch( GSource *source, GSourceFunc, gpointer )
{
    QEventLoopGlib *g = loopOf( source );
    if ( g->d->shortcut )
	return G_SOURCE_CONTINUE;
    DispatchLock lock( g );
    g->delivered += qt_activate_timers( FALSE );
    return G_SOURCE_CONTINUE;
}

GSourceFuncs timerFuncs = { timerPrepare, timerCheck, timerDispatch, 0, 0, 0 };

gboolean zeroTimerPrepare( GSource *, gint *timeout )
{
    bool zeroTimers = qt_zero_timers();
    *timeout = zeroTimers ? 0 : -1;
    return zeroTimers;
}

gboolean zeroTimerCheck( GSource * )
{
    return qt_zero_timers();
}

gboolean zeroTimerDispatch( GSource *source, GSourceFunc, gpointer )
{
    QEventLoopGlib *g = loopOf( source );
    if ( g->d->shortcut )
	return G_SOURCE_CONTINUE;
    DispatchLock lock( g );
    // zero timers do not count as delivered events, as in the select() loop
    qt_activate_timers( TRUE );
    return G_SOURCE_CONTINUE;
}

GSourceFuncs zeroTimerFuncs = { zeroTimerPrepare, zeroTimerCheck, zeroTimerDispatch, 0, 0, 0 };


/*****************************************************************************
  Blocking
 *****************************************************************************/

void drainWakePipe( QEventLoopPrivate *d )
{
    char buf[64];
    while ( ::read( d->thread_pipe[0], buf, sizeof buf ) > 0 )
	;
}

/*
  The poll function of the context: tells Qt that the event loop is about
  to block and is awake again, runs the pre/post-select handlers and
  releases qt_mutex while polling.
*/
gint qtPoll( GPollFD *fds, guint nfds, gint timeout )
{
    QEventLoopGlib *g = qt_glib_loop;
    if ( !g )
	return g_poll( fds, nfds, timeout );
    GPollFunc poll = g->oldPollFunc ? g->oldPollFunc : g_poll;

    // processEvents() called from the slots below must not run a nested
    // g_main_context_iteration(), GLib is in the middle of one
    g->inPoll = TRUE;
    if ( g->wakePending || g->postedSerial.load() != g->sentSerial )
	timeout = 0;
    if ( timeout != 0 )
	g->emitAboutToBlock();
    qt_run_preselect_handlers();
    // posted or woken up from the slots or the handlers?
    if ( g->wakePending || g->postedSerial.load() != g->sentSerial )
	timeout = 0;
    g->wakePending = FALSE;
    g->crossWake.store( FALSE );	// g_main_context_wakeup() takes over

    gint ret;
    {
	PollUnlock unlock( g );
	ret = poll( fds, nfds, timeout );
    }
    int saved_errno = errno;

    g->emitAwake();
    qt_run_postselect_handlers();
    qt_reset_color_avail();		// color approx. optimization
    g->inPoll = FALSE;

    errno = saved_errno;
    return ret;
}

} // namespace


/*****************************************************************************
  QEventLoopGlib
 *****************************************************************************/

QEventLoopGlib::QEventLoopGlib( QEventLoop *loop, QEventLoopPrivate *priv )
    : q( loop ), d( priv ),
      context( g_main_context_ref( g_main_context_default() ) ),
      acquired( g_main_context_acquire( context ) ),
      oldPollFunc( 0 ), x11Tag( 0 ), delivered( 0 ),
      postedSerial( 1 ), sentSerial( 0 ),	// send what was posted before
      wakePending( FALSE ), crossWake( FALSE ), privatePolling( FALSE ),
#if defined(QT_THREAD_SUPPORT)
      guiThread( QThread::currentThread() ), qtMutex( 0 ),
#endif
      mutexDepth( 0 ), inPoll( FALSE ),
      awakeSignal( QEventLoop::staticMetaObject()->findSignal( "awake()", TRUE ) ),
      aboutToBlockSignal( QEventLoop::staticMetaObject()->findSignal( "aboutToBlock()", TRUE ) ),
      guiThreadAwakeSignal( QApplication::staticMetaObject()->findSignal( "guiThreadAwake()", TRUE ) )
{
    if ( !acquired ) {
	qWarning( "QEventLoop: the GLib main context is owned by another thread,"
		  " GLib sources will not be dispatched" );
    } else if ( qt_glib_loop ) {
	qWarning( "QEventLoop: there is already a GLib event loop" );
    } else {
	oldPollFunc = g_main_context_get_poll_func( context );
	g_main_context_set_poll_func( context, qtPoll );
	qt_glib_loop = this;
    }

    // attach order = dispatch order for sources of the same priority
    postedSource.reset( newSource( &postedFuncs, G_PRIORITY_DEFAULT, "Qt posted events" ) );
    setupX11();
    socketSource.reset( newSource( &socketFuncs, G_PRIORITY_DEFAULT, "Qt socket notifiers" ) );
    timerSource.reset( newSource( &timerFuncs, G_PRIORITY_DEFAULT, "Qt timers" ) );
    zeroTimerSource.reset( newSource( &zeroTimerFuncs, G_PRIORITY_DEFAULT_IDLE,
				      "Qt zero timers" ) );
}

QEventLoopGlib::~QEventLoopGlib()
{
    // forget the socket notifier tags, they go with the source
    for ( int type = 0; type < 3; ++type ) {
	QPtrList<QSockNot> *list = d->sn_vec[type].list;
	if ( !list )
	    continue;
	QPtrListIterator<QSockNot> it( *list );
	for ( QSockNot *sn; ( sn = it.current() ); ++it )
	    sn->tag = 0;
    }

    zeroTimerSource.reset();
    timerSource.reset();
    socketSource.reset();
    cleanupX11();
    postedSource.reset();

    if ( qt_glib_loop == this ) {
	g_main_context_set_poll_func( context, oldPollFunc );
	qt_glib_loop = 0;
    }
    if ( acquired )
	g_main_context_release( context );
    g_main_context_unref( context );
}

GSource *QEventLoopGlib::newSource( GSourceFuncs *funcs, int priority, const char *name )
{
    GSource *source = g_source_new( funcs, sizeof( QtSource ) );
    reinterpret_cast<QtSource *>( source )->loop = this;
    g_source_set_priority( source, priority );
    g_source_set_can_recurse( source, TRUE );	// nested event loops
    g_source_set_name( source, name );
    // if another thread owns the context, the sources are not attached and
    // processEvents() handles everything itself
    if ( acquired )
	g_source_attach( source, context );
    return source;
}

void QEventLoopGlib::setupX11()
{
    // the display may not be open yet (event loop created before the
    // QApplication), then appStartingUp() does this
    if ( x11Source || !qt_is_gui_used || !QPaintDevice::x11AppDisplay() )
	return;
    d->xfd = XConnectionNumber( QPaintDevice::x11AppDisplay() );
    x11Source.reset( newSource( &x11Funcs, G_PRIORITY_DEFAULT, "Qt X11 events" ) );
    x11Tag = g_source_add_unix_fd( x11Source.get(), d->xfd, G_IO_IN );
    // keep the dispatch order posted events, X11, sockets, timers
    if ( socketSource ) {
	SourcePtr sockets( std::move( socketSource ) ), timers( std::move( timerSource ) );
	// re-create them after the X11 source; the tags must be re-added
	socketSource.reset( newSource( &socketFuncs, G_PRIORITY_DEFAULT, "Qt socket notifiers" ) );
	timerSource.reset( newSource( &timerFuncs, G_PRIORITY_DEFAULT, "Qt timers" ) );
	for ( int type = 0; type < 3; ++type ) {
	    QPtrList<QSockNot> *list = d->sn_vec[type].list;
	    if ( !list )
		continue;
	    QPtrListIterator<QSockNot> it( *list );
	    for ( QSockNot *sn; ( sn = it.current() ); ++it ) {
		if ( sn->tag )
		    sn->tag = g_source_add_unix_fd( socketSource.get(), sn->fd,
						    socketConditions[type] );
	    }
	}
    }
}

void QEventLoopGlib::cleanupX11()
{
    x11Source.reset();
    x11Tag = 0;
    d->xfd = -1;
}

namespace {
// QObject::receivers() is protected; a pointer to it taken through a
// derived class can be used on any QObject
struct ReceiversAccess : public QObject
{
    static bool connected( const QObject *object, int signal )
    {
	QConnectionList *( QObject::*receivers )( int ) const = &ReceiversAccess::receivers;
	return ( object->*receivers )( signal ) != 0;
    }
};
} // namespace

// The signals are emitted only if something is connected: this runs
// twice per iteration and emitting an unconnected signal is not free.
void QEventLoopGlib::emitAboutToBlock()
{
    if ( ReceiversAccess::connected( q, aboutToBlockSignal ) )
	q->qt_emit( aboutToBlockSignal, 0 );
}

void QEventLoopGlib::emitAwake()
{
    if ( ReceiversAccess::connected( q, awakeSignal ) )
	q->qt_emit( awakeSignal, 0 );
    if ( qApp && ReceiversAccess::connected( qApp, guiThreadAwakeSignal ) )
	qApp->qt_emit( guiThreadAwakeSignal, 0 );
}


/*****************************************************************************
  Hooks called by the shared socket notifier code (qeventloop_unix.cpp)
 *****************************************************************************/

void qt_glib_sn_registered( QEventLoopPrivate *d, QSockNot *sn )
{
    QEventLoopGlib *g = d->glib;
    if ( !g || !g->socketSource )
	return;
    sn->tag = g_source_add_unix_fd( g->socketSource.get(), sn->fd,
				    socketConditions[sn->obj->type()] );
}

void qt_glib_sn_unregistered( QEventLoopPrivate *d, QSockNot *sn )
{
    QEventLoopGlib *g = d->glib;
    if ( g && g->socketSource && sn->tag )
	g_source_remove_unix_fd( g->socketSource.get(), sn->tag );
    sn->tag = 0;
}


/*****************************************************************************
  QEventLoop implementation
 *****************************************************************************/

void QEventLoop::init()
{
    // the wake-up pipe is only used while polling for Qt's sources only
    if ( ::pipe( d->thread_pipe ) == -1 ) {
	// as in the select() loop: qFatal() returns if the application
	// installed a message handler that does not exit; go on without the
	// pipe (wakeUp() and processQtEvents() skip a negative descriptor)
	qFatal( "QEventLoop: cannot create the thread pipe: %s", strerror( errno ) );
	d->thread_pipe[0] = d->thread_pipe[1] = -1;
    } else {
	for ( int i = 0; i < 2; ++i ) {
	    fcntl( d->thread_pipe[i], F_SETFD, FD_CLOEXEC );
	    fcntl( d->thread_pipe[i], F_SETFL, O_NONBLOCK );
	}
    }

    d->sn_highest = -1;
    d->xfd = -1;
    d->glib = new QEventLoopGlib( this, d );
#if defined(QT_THREAD_SUPPORT)
    d->glib->qtMutex = &QApplication::qt_mutex;
#endif
}

void QEventLoop::cleanup()
{
    delete d->glib;
    d->glib = 0;
    if ( d->thread_pipe[0] != -1 ) {
	close( d->thread_pipe[0] );
	close( d->thread_pipe[1] );
    }
    cleanupTimers();
}

void QEventLoop::appStartingUp()
{
    d->glib->setupX11();
}

void QEventLoop::appClosingDown()
{
    d->glib->cleanupX11();
}

void QEventLoop::wakeUp()
{
    QEventLoopGlib *g = d->glib;
    if ( !g )
	return;
    g->postedSerial.fetch_add( 1 );
#if defined(QT_THREAD_SUPPORT)
    if ( QThread::currentThread() == g->guiThread ) {
	// the GUI thread is not blocked; make sure that it does not block
	// before it has seen the posted events (see qtPoll())
	g->wakePending = TRUE;
	return;
    }
#endif
    g->crossWake.store( TRUE );
    g_main_context_wakeup( g->context );
    if ( g->privatePolling.load() && d->thread_pipe[1] != -1 ) {
	char c = 0;
	if ( ::write( d->thread_pipe[1], &c, 1 ) < 0 && errno != EAGAIN )
	    qWarning( "QEventLoop::wakeUp: %s", strerror( errno ) );
    }
}

bool QEventLoop::hasPendingEvents() const
{
    return qGlobalPostedEventsCount() ||
	( d->glib->x11Source && XPending( QPaintDevice::x11AppDisplay() ) );
}

/*
  One iteration of Qt's sources only, for processEvents() with Exclude*
  flags: this is the select() loop's processEvents() with poll().
*/
static bool processQtEvents( QEventLoop *q, QEventLoopGlib *g,
			     QEventLoop::ProcessEventsFlags flags )
{
    QEventLoopPrivate *d = g->d;
    int nevents = 0;

    // handle gui and posted events
    if ( g->x11Source ) {
	QApplication::sendPostedEvents();

	XEvent event;
	QtX11EventsResult result = qt_x11_process_events( d, flags, event, nevents );
	g->delivered += nevents;
	switch ( result ) {
	case QtX11EventsShortcut:
	    return FALSE;
	case QtX11EventsFiltered:
	    return TRUE;
	case QtX11EventsDone:
	    break;
	}
    }

    if ( d->shortcut )
	return FALSE;

    g->sentSerial = g->postedSerial.load();
    QApplication::sendPostedEvents();

    const uint exclude_all = QEventLoop::ExcludeSocketNotifiers | ExcludeTimers;
    if ( nevents > 0 && ( flags & exclude_all ) == exclude_all &&
	 ( flags & QEventLoop::WaitForMore ) )
	return TRUE;

    // don't block if exitLoop() or exit()/quit() has been called.
    bool canWait = d->exitloop || d->quitnow ? FALSE : ( flags & QEventLoop::WaitForMore );

    int timeout = canWait ? -1 : 0;
    if ( canWait && !( flags & ExcludeTimers ) ) {
	if ( timeval *tm = qt_wait_timer() ) // rounded up
	    timeout = tm->tv_sec * 1000 + ( tm->tv_usec + 999 ) / 1000;
    }

    std::vector<pollfd> fds;
    std::vector<QSockNot *> notifiers;
    if ( g->x11Source )
	fds.push_back( { d->xfd, POLLIN, 0 } );
    if ( d->thread_pipe[0] != -1 )
	fds.push_back( { d->thread_pipe[0], POLLIN, 0 } );
    const size_t firstNotifier = fds.size();
    if ( !( flags & QEventLoop::ExcludeSocketNotifiers ) ) {
	static const short events[3] = { POLLIN, POLLOUT, POLLPRI };
	for ( int type = 0; type < 3; ++type ) {
	    QPtrList<QSockNot> *list = d->sn_vec[type].list;
	    if ( !list )
		continue;
	    QPtrListIterator<QSockNot> it( *list );
	    for ( QSockNot *sn; ( sn = it.current() ); ++it ) {
		if ( g->acquired && !sn->tag )	// parked, see socketDispatch()
		    continue;
		fds.push_back( { sn->fd, events[type], 0 } );
		notifiers.push_back( sn );
	    }
	}
    }

    if ( canWait )
	g->emitAboutToBlock();
    qt_run_preselect_handlers();

    g->privatePolling.store( TRUE );
    if ( g->crossWake.exchange( FALSE ) || g->wakePending )
	timeout = 0;
    g->wakePending = FALSE;
    int nsel;
    {
	PollUnlock unlock( g );
	do {
	    nsel = ::poll( fds.data(), fds.size(), timeout );
	} while ( nsel == -1 && ( errno == EINTR || errno == EAGAIN ) );
    }
    g->privatePolling.store( FALSE );
    if ( nsel == -1 )
	perror( "poll" );

    // we are awake, broadcast it
    g->emitAwake();

    if ( d->thread_pipe[0] != -1 )
	drainWakePipe( d );

    qt_run_postselect_handlers();

    // activate socket notifiers
    if ( nsel > 0 && !notifiers.empty() ) {
	std::vector<QSocketNotifier *> invalid;
	for ( size_t i = 0; i < notifiers.size(); ++i ) {
	    short revents = fds[firstNotifier + i].revents;
	    QSockNot *sn = notifiers[i];
	    if ( revents & POLLNVAL )
		invalid.push_back( sn->obj );
	    else if ( socketReady( sn->obj->type(), (unsigned short)revents ) )
		qt_sn_set_pending( d, sn );
	}
	disableInvalidNotifiers( invalid );
	int n = q->activateSocketNotifiers();
	nevents += n;
	g->delivered += n;
    }

    // activate timers
    if ( !( flags & ExcludeTimers ) ) {
	int n = q->activateTimers();
	nevents += n;
	g->delivered += n;
    }

    // color approx. optimization - only on X11
    qt_reset_color_avail();

    // return true if we handled events, false otherwise
    return nevents > 0;
}

bool QEventLoop::processEvents( ProcessEventsFlags flags )
{
    QEventLoopGlib *g = d->glib;

#if defined(QT_THREAD_SUPPORT)
    QMutexLocker locker( QApplication::qt_mutex );
    struct Depth {
	int &depth;
	explicit Depth( int &n ) : depth( n ) { ++depth; }
	~Depth() { --depth; }
    } depth( g->mutexDepth );
#endif

    const uint exclude = ExcludeUserInput | ExcludeSocketNotifiers | ExcludeTimers;
    if ( ( flags & exclude ) || g->inPoll || !g->acquired )
	return processQtEvents( this, g, flags );

    unsigned int delivered = g->delivered;
    // don't block if exitLoop() or exit()/quit() has been called.
    bool canWait = d->exitloop || d->quitnow ? FALSE : ( flags & WaitForMore );
    g_main_context_iteration( g->context, canWait );
    return g->delivered != delivered;
}
