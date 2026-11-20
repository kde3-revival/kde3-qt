TEMPLATE = lib
TARGET	 = qsqlpsql

CONFIG	+= qt plugin
DESTDIR	 = ../../../sqldrivers

HEADERS		= ../../../../src/sql/drivers/psql/qsql_psql.h
SOURCES		= main.cpp \
		  ../../../../src/sql/drivers/psql/qsql_psql.cpp
unix {
	OBJECTS_DIR	= .obj
	!contains( LIBS, .*pq.* ) {
	    LIBS	*= -lpq
	}
}

REQUIRES	= sql

target.path += $$plugins.path/sqldrivers
INSTALLS += target
