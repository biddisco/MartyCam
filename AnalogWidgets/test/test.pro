# Plik utworzony przez menad?era projektów kdevelopa
# ------------------------------------------- 
# Podkatalog wzgl?dem g?ównego katalogu projektu: ./test
# Cel to program:  



include (../Flags.pri)  

unix{
TARGETDEPS += ../analogwidgets/libanalogwidgets.a 
LIBS += ../analogwidgets/libanalogwidgets.a  
}

win32-g++:{
TARGETDEPS += ../analogwidgets/libanalogwidgets.a 
LIBS += ../analogwidgets/libanalogwidgets.a  
}


win32-msvc*:{
 TARGETDEPS += ../analogwidgets/analogwidgets.lib
 LIBS += ../analogwidgets/analogwidgets.lib
}

wince*:{
 TARGETDEPS += ../analogwidgets/analogwidgets.lib
 LIBS += ../analogwidgets/analogwidgets.lib
}



INCLUDEPATH += ../analogwidgets/analogwidgets \
               ../analogwidgets 
MOC_DIR = objects 
UI_DIR = . 
OBJECTS_DIR = ../objects 

CONFIG += release \
          warn_on 
TEMPLATE = app 
RESOURCES += test.qrc 
FORMS += testform.ui
DESTDIR=../test 
HEADERS += test.h \
           widgettester.h 

SOURCES += main.cpp \
           test.cpp \
           widgettester.cpp 

