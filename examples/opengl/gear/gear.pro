TEMPLATE	= app
TARGET		= gear

CONFIG		+= qt opengl warn_on release
CONFIG -= dlopen_opengl
unix:LIBS  += -lm
DEPENDPATH	= ../include

REQUIRES        = opengl

HEADERS		=
SOURCES		= gear.cpp
