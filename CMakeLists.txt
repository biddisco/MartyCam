PROJECT(MartyCam)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRECT_SOURCE_DIR}/cmake)


cmake_minimum_required(VERSION 2.8)

FIND_PACKAGE(Qt4 4.4 REQUIRED)
FIND_PACKAGE(OpenCV REQUIRED)

INCLUDE(${QT_USE_FILE})
INCLUDE_DIRECTORIES(include)


SET(QtOpenCV_UIS
	settings.ui
	martycam.ui
)

QT4_WRAP_UI(QtOpenCV_UIS_H ${QtOpenCV_UIS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

SET(QtOpenCV_OBJECTS
	martycam.h
	settings.h
	renderwidget.h
	trackcontroller.h
	capturethread.h
)

QT_WRAP_CPP(MartyCam MOC_FILES ${QtOpenCV_OBJECTS})

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src ${QT_INCLUDE_DIR} ${QT_QTNETWORK_INCLUDE_DIR} ${QT_QTXML_INCLUDE_DIR})

LINK_LIBRARIES(${SSL_LIBRARY} ${QT_LIBRARIES} ${QT_QTNETWORK_LIBRARIES}
	       ${QT_QTXML_LIBRARIES})

SET(QtOpenCV_SRCS
	capturethread.cpp
	filter.cpp
	martycam.cpp
	imagebuffer.cpp
	main.cpp
	processingthread.cpp
	renderwidget.cpp
	settings.cpp
	trackcontroller.cpp
)

ADD_EXECUTABLE(MartyCam WIN32 ${QtOpenCV_SRCS} ${QtOpenCV_UIS_H} ${MOC_FILES})

#SET_TARGET_PROPERTIES (MartyCam
# #   PROPERTIES
#        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
#) 
target_link_libraries(MartyCam ${QT_LIBRARIES} ${OpenCV_LIBS})

#install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/MartyCam DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
#install(FILES pics/smile.png DESTINATION ${DATADIR}/emoticons)
#install(FILES 
#  pics/decrypted.png
#  pics/encrypted.png
#  pics/offline.png
#  pics/online.png
#  pics/online-none.png
#  pics/online-to.png
#  pics/online-from.png
#  pics/chat.png
#  pics/xa.png
#  pics/dnd.png
#  pics/away.png
#  pics/16x16.png
#  pics/32x32.png
#  pics/64x64.png
#  pics/128x128.png
#  DESTINATION ${DATADIR}/icons)
