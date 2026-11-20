#always install the library
target.path=$$libs.path
INSTALLS += target

#headers
headers.files  = ../include/*.h
headers.files += $$QT_BUILD_TREE/include/qconfig.h \
                 $$QT_BUILD_TREE/include/qmodules.h 
isEmpty(headers_p.path):headers_p.path=$$headers.path/private
headers_p.files = ../include/private/*.h
INSTALLS += headers headers_p

#docs
htmldocs.files = ../doc/html/*
htmldocs.path = $$docs.path/html
INSTALLS += htmldocs

#translations
translations.files = ../translations/*.qm
INSTALLS += translations
