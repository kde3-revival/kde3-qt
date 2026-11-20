# Qt graphics 

#mng support
HEADERS += $$KERNEL_H/qmngio.h
SOURCES += $$KERNEL_CPP/qmngio.cpp
mng {
	unix:LIBS  += -lmng
	no-jpeg {
		message(Use of mng requires support for jpeg)
		CONFIG += jpeg
	} else:!jpeg {
		message(Use of mng requires support for jpeg)
		CONFIG += jpeg
	}
}
else:DEFINES += QT_NO_IMAGEIO_MNG

#jpeg support..
HEADERS += $$KERNEL_H/qjpegio.h 
SOURCES += $$KERNEL_CPP/qjpegio.cpp
jpeg {
	unix:LIBS  += -ljpeg
}
else:DEFINES += QT_NO_IMAGEIO_JPEG

#png support
HEADERS+=$$KERNEL_H/qpngio.h
SOURCES+=$$KERNEL_CPP/qpngio.cpp
png {
	unix:LIBS  += -lpng
}
else:DEFINES += QT_NO_IMAGEIO_PNG

#zlib support
!no-zlib {
   unix:LIBS += -lz
}

unix:xftfreetype {
	INCLUDEPATH += 3rdparty/opentype
	SOURCES += 3rdparty/opentype/ftxopentype.c
}

#use Qt gif
gif:DEFINES += QT_BUILTIN_GIF_READER=1

