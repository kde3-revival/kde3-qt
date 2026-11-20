TEMPLATE	= lib
TARGET		= grapher

CONFIG	       += qt dll release
LIBS	       += -lqnp
unix:LIBS      += -lXt

HEADERS		=
SOURCES		= grapher.cpp
