# Qt project file
TEMPLATE	= lib
TARGET		= kde3-qt-st
VERSION		= 3.3.8
DESTDIR		= $$QMAKE_LIBDIR_QT
DLLDESTDIR	= ../bin

CONFIG		+= qt warn_on depend_includepath
CONFIG          += qmake_cache

linux-*:version_script {
   QMAKE_LFLAGS += -Wl,--version-script=libqt.map
   TARGETDEPS += libqt.map
}

KERNEL_CPP	= kernel
CANVAS_CPP      = canvas
WIDGETS_CPP	= widgets
SQL_CPP	        = sql
TABLE_CPP	= table
DIALOGS_CPP	= dialogs
ICONVIEW_CPP	= iconview
NETWORK_CPP	= network
OPENGL_CPP	= opengl
TOOLS_CPP	= tools
CODECS_CPP	= codecs
WORKSPACE_CPP	= workspace
XML_CPP	        = xml
STYLES_CPP	= styles

unix {
	CANVAS_H	= $$CANVAS_CPP
	KERNEL_H	= $$KERNEL_CPP
	WIDGETS_H	= $$WIDGETS_CPP
	SQL_H		= $$SQL_CPP
	TABLE_H		= $$TABLE_CPP
	DIALOGS_H	= $$DIALOGS_CPP
	ICONVIEW_H	= $$ICONVIEW_CPP
	NETWORK_H	= $$NETWORK_CPP
	OPENGL_H	= $$OPENGL_CPP
	TOOLS_H		= $$TOOLS_CPP
	CODECS_H	= $$CODECS_CPP
	WORKSPACE_H	= $$WORKSPACE_CPP
	XML_H		= $$XML_CPP
	STYLES_H	= $$STYLES_CPP
	CONFIG	   += x11 x11inc
}

DEPENDPATH += ;$$NETWORK_H;$$KERNEL_H;$$WIDGETS_H;$$SQL_H;$$TABLE_H;$$DIALOGS_H;
DEPENDPATH += $$ICONVIEW_H;$$OPENGL_H;$$TOOLS_H;$$CODECS_H;$$WORKSPACE_H;$$XML_H;
DEPENDPATH += $$CANVAS_H;$$STYLES_H

thread {
	TARGET = kde3-qt
	DEFINES += QT_THREAD_SUPPORT
}

!cups:DEFINES += QT_NO_CUPS


largefile {
	unix:DEFINES += _LARGEFILE_SOURCE _LARGE_FILES _FILE_OFFSET_BITS=64
}

# GLib event loop (configure -glib-eventloop): only the library itself is
# compiled with the GLib flags, the public headers do not need them
glib_eventloop {
	QMAKE_CXXFLAGS += $$QMAKE_CFLAGS_GLIB
	LIBS += $$QMAKE_LIBS_GLIB
}

# HarfBuzz (configure -harfbuzz, found with pkg-config): text shaping and
# fractional glyph positioning (kernel/qfontengine_ft.cpp); like GLib only
# the library is compiled with its flags
harfbuzz {
	QMAKE_CXXFLAGS += $$QMAKE_CFLAGS_HARFBUZZ
	LIBS += $$QMAKE_LIBS_HARFBUZZ
	DEFINES += QT_HARFBUZZ
}

#here for compatability, should go away ####
include($$KERNEL_CPP/qt_compat.pri)

#platforms
x11:include($$KERNEL_CPP/qt_x11.pri)

#modules
include($$KERNEL_CPP/qt_kernel.pri)
include($$WIDGETS_CPP/qt_widgets.pri)
include($$DIALOGS_CPP/qt_dialogs.pri)
include($$ICONVIEW_CPP/qt_iconview.pri)
include($$WORKSPACE_CPP/qt_workspace.pri)
include($$NETWORK_CPP/qt_network.pri)
include($$CANVAS_CPP/qt_canvas.pri)
include($$TABLE_CPP/qt_table.pri)
include($$XML_CPP/qt_xml.pri)
include($$OPENGL_CPP/qt_opengl.pri)
include($$SQL_CPP/qt_sql.pri)
include($$KERNEL_CPP/qt_gfx.pri)
include($$TOOLS_CPP/qt_tools.pri)
include($$CODECS_CPP/qt_codecs.pri)
include($$STYLES_CPP/qt_styles.pri)

# qconfig.cpp
exists($$QT_BUILD_TREE/src/tools/qconfig.cpp) {
    SOURCES += $$QT_BUILD_TREE/src/tools/qconfig.cpp
}

#install directives
include(qt_install.pri)
!staticlib:PRL_EXPORT_DEFINES += QT_SHARED

unix {
   CONFIG     += create_pc
   QMAKE_PKGCONFIG_LIBDIR = $$target.path
   QMAKE_PKGCONFIG_INCDIR = $$headers.path
}
