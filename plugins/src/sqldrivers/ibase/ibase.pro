TEMPLATE = lib
TARGET	 = qsqlibase

CONFIG	+= qt plugin
DESTDIR	 = ../../../sqldrivers

HEADERS		= ../../../../src/sql/drivers/ibase/qsql_ibase.h
SOURCES		= main.cpp \
		  ../../../../src/sql/drivers/ibase/qsql_ibase.cpp

unix {
	OBJECTS_DIR = .obj

	!contains( LIBS, .*gds.* ):!contains( LIBS, .*libfb.* ) {
	    LIBS    *= -lgds
	}
}

REQUIRES	= sql

target.path += $$plugins.path/sqldrivers
INSTALLS += target
