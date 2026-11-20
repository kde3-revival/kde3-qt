TEMPLATE = lib
TARGET  += qpng

CONFIG += qt plugin
DESTDIR = ../../../imageformats

VERSION = 1.0.0
REQUIRES = !no-png !png

SOURCES += main.cpp

system-png {
        unix:LIBS  += -lpng
}

system-zlib:unix:LIBS += -lz


target.path += $$plugins.path/imageformats
INSTALLS += target
