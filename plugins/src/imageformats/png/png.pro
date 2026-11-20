TEMPLATE = lib
TARGET  += qpng

CONFIG += qt plugin
DESTDIR = ../../../imageformats

VERSION = 1.0.0
REQUIRES = !no-png !png

SOURCES += main.cpp

system-png {
        unix:LIBS  += -lpng
        win32:LIBS += libpng.lib
}

system-zlib:unix:LIBS += -lz
system-zlib:mac:LIBS += -lz


target.path += $$plugins.path/imageformats
INSTALLS += target
