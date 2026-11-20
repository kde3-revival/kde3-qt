TEMPLATE	= lib
CONFIG		+= qt warn_on release
SOURCES		= qwidgetfactory.cpp \
		  ../shared/domtool.cpp \
		  ../shared/uib.cpp

HEADERS		= ../shared/domtool.h \
		  ../shared/uib.h

sql:SOURCES += 		  ../designer/database.cpp
sql:HEADERS +=		  ../designer/database2.h

DEFINES += QT_INTERNAL_XML
DEFINES += Q_TEMPLATE_EXTERN=extern
include( ../../../src/qt_professional.pri )
TARGET		= kde3-qui
INCLUDEPATH	+= ../shared
DESTDIR		= ../../../lib
VERSION		= 1.0.0
DEFINES		+= RESOURCE

unix {
	target.path=$$libs.path

	INSTALLS        += target
}

