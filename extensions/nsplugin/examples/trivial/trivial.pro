TEMPLATE 	= lib
TARGET      	= trivial

CONFIG	       += qt dll release
LIBS	       += -lqnp
unix:LIBS      += -lXt

HEADERS		=
SOURCES		= trivial.cpp
