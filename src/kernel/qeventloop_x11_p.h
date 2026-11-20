/****************************************************************************
**
** X11 event processing shared by the QEventLoop implementations
**
** Copyright (C) 2000-2008 Trolltech ASA.  All rights reserved.
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

#ifndef QEVENTLOOP_X11_P_H
#define QEVENTLOOP_X11_P_H

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

#include "qeventloop_p.h" // includes qplatformdefs.h
#include "qeventloop.h"
#include "qapplication.h"
#include "qt_x11_p.h"

// Must be included before KeyPress and KeyRelease are #undef'ed.

/*
  Result of qt_x11_process_events():
  - QtX11EventsDone: Xlib's queue is empty
  - QtX11EventsShortcut: exitLoop(), exit() or quit() was called while
    dispatching; the event loop must return FALSE without doing anything else
  - QtX11EventsFiltered: QApplication::x11ProcessEvent() returned 1; the
    event loop must return TRUE without doing anything else
*/
enum QtX11EventsResult {
    QtX11EventsDone,
    QtX11EventsShortcut,
    QtX11EventsFiltered
};

/*
  Reads and dispatches the X events, including the ones already in Xlib's
  queue, until there are no more. With ExcludeUserInput, input events and
  most client messages are dropped. \a event is the caller's buffer for
  the event being dispatched, \a nevents is incremented for each event
  dispatched.

  This is the X11 part of the select() based QEventLoop::processEvents()
  and is shared with the other implementations, so that all of them handle
  X events in exactly the same way. It is static inline because each event
  loop calls it from exactly one place.
*/
static inline QtX11EventsResult qt_x11_process_events( QEventLoopPrivate * const &d,
						       QEventLoop::ProcessEventsFlags flags,
						       XEvent &event, int &nevents )
{
    // Two loops so that posted events accumulate
    while ( XPending( QPaintDevice::x11AppDisplay() ) ) {
	// also flushes output buffer
	while ( XPending( QPaintDevice::x11AppDisplay() ) ) {
	    if ( d->shortcut ) {
		return QtX11EventsShortcut;
	    }

	    XNextEvent( QPaintDevice::x11AppDisplay(), &event );

	    if ( flags & QEventLoop::ExcludeUserInput ) {
		switch ( event.type ) {
		case ButtonPress:
		case ButtonRelease:
		case MotionNotify:
		case KeyPress:
		case KeyRelease:
		case EnterNotify:
		case LeaveNotify:
		    continue;

		case ClientMessage:
		    {
			// from qapplication_x11.cpp
			extern Atom qt_wm_protocols;
			extern Atom qt_wm_take_focus;
			extern Atom qt_qt_scrolldone;

			// only keep the wm_take_focus and
			// qt_qt_scrolldone protocols, discard all
			// other client messages
			if ( event.xclient.format != 32 )
			    continue;

			if ( event.xclient.message_type == qt_wm_protocols ||
			     (Atom) event.xclient.data.l[0] == qt_wm_take_focus )
			    break;
			if ( event.xclient.message_type == qt_qt_scrolldone )
			    break;
		    }

		default: break;
		}
	    }

	    nevents++;
	    if ( qApp->x11ProcessEvent( &event ) == 1 )
		return QtX11EventsFiltered;
	}
    }
    return QtX11EventsDone;
}

#endif // QEVENTLOOP_X11_P_H
