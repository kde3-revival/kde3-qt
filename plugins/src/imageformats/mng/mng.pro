TEMPLATE = lib
TARGET  += qmng

CONFIG  += qt plugin
DESTDIR  = ../../../imageformats

VERSION = 1.0.0
REQUIRES = !no-mng !mng

SOURCES += main.cpp

win32-borland {
	QMAKE_CFLAGS_WARN_ON	+= -w-par
	QMAKE_CXXFLAGS_WARN_ON	+= -w-par
}
win32: CONFIG-=zlib system-zlib jpeg system-jpeg

system-mng {
        win32:LIBS += libmng.lib
        unix:LIBS  += -lmng
}

!no-zlib:!zlib:unix:LIBS += -lz
!no-zlib:!zlib:mac:LIBS += -lz

system-jpeg {
        unix:LIBS  += -ljpeg
        win32:LIBS += libjpeg.lib
}


target.path += $$plugins.path/imageformats
INSTALLS += target
