TEMPLATE 	= app
TARGET		= designer

CONFIG 		-= moc
DESTDIR		= $$QT_BUILD_TREE/bin

SOURCES		+= main.cpp
INCLUDEPATH	+= ../designer
LIBS	+= -ldesignercore -lkde3-qui -lqassistantclient -L$$QT_BUILD_TREE/lib


target.path=$$bins.path
INSTALLS        += target
