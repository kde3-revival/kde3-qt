TEMPLATE = lib
TARGET  += qjpeg

CONFIG  += qt plugin
DESTDIR  = ../../../imageformats

VERSION = 1.0.0
REQUIRES = !no-jpeg !jpeg

SOURCES += main.cpp
system-jpeg {
        unix:LIBS += -ljpeg
}


target.path += $$plugins.path/imageformats
INSTALLS += target
