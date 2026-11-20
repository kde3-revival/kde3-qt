TEMPLATE = lib
TARGET	 = qsqlodbc

CONFIG  += qt plugin
DESTDIR	 = ../../../sqldrivers

HEADERS		= ../../../../src/sql/drivers/odbc/qsql_odbc.h
SOURCES		= main.cpp \
		  ../../../../src/sql/drivers/odbc/qsql_odbc.cpp

unix {
	OBJECTS_DIR	= .obj
	!contains( LIBS, .*odbc.* ) {
	    LIBS 	*= -lodbc
	}
}

REQUIRES	= sql

target.path += $$plugins.path/sqldrivers
INSTALLS += target
