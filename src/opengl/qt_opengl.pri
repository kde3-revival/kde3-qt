# Qt opengl module

opengl {
	HEADERS += $$OPENGL_H/qgl.h \
		   $$OPENGL_H/qglcolormap.h
	SOURCES	+= $$OPENGL_CPP/qgl.cpp \
		   $$OPENGL_CPP/qglcolormap.cpp
	x11 {
		HEADERS += $$OPENGL_H/qgl_x11_p.h
		SOURCES += $$OPENGL_CPP/qgl_x11.cpp
	}

	dlopen_opengl:DEFINES+=QT_DLOPEN_OPENGL
}


