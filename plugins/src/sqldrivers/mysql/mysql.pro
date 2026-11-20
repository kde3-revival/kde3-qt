TEMPLATE = lib
TARGET	 = qsqlmysql

CONFIG	+= qt plugin
DESTDIR	 = ../../../sqldrivers

HEADERS		= ../../../../src/sql/drivers/mysql/qsql_mysql.h
SOURCES		= main.cpp \
		  ../../../../src/sql/drivers/mysql/qsql_mysql.cpp

unix {
	OBJECTS_DIR = .obj

	!contains( LIBS, .*mysql.* ) {
	    LIBS    *= -lmysqlclient
	}
}

REQUIRES	= sql

target.path += $$plugins.path/sqldrivers
INSTALLS += target
