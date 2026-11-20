TEMPLATE = lib
TARGET  += qmng

CONFIG  += qt plugin
DESTDIR  = ../../../imageformats

VERSION = 1.0.0
REQUIRES = !no-mng !mng

SOURCES += main.cpp


system-mng {
        unix:LIBS  += -lmng
}

!no-zlib:!zlib:unix:LIBS += -lz

system-jpeg {
        unix:LIBS  += -ljpeg
}


target.path += $$plugins.path/imageformats
INSTALLS += target
