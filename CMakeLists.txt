PROJECT(MartyCam)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRECT_SOURCE_DIR}/cmake)


cmake_minimum_required(VERSION 2.8)

FIND_PACKAGE(Qt4 4.4 REQUIRED)
FIND_PACKAGE(OpenCV REQUIRED)

INCLUDE(${QT_USE_FILE})
INCLUDE_DIRECTORIES(include)

ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)

SET(VIDEOLIB_LIBRARY 
  "C:\\Code\\OpenCV-2.2.0\\3rdparty\\lib\\videoInput64.lib" 
  CACHE FILEPATH "Path to videlib")

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
#  trackcontroller.cpp
)

ADD_EXECUTABLE(MartyCam WIN32 ${QtOpenCV_SRCS} ${QtOpenCV_UIS_H} ${MOC_FILES})

#SET_TARGET_PROPERTIES (MartyCam
# #   PROPERTIES
#        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
#) videoInput64

#LINK_DIRECTORIES("C:\\Code\\OpenCV-2.2.0\\3rdparty\\lib")
target_link_libraries(MartyCam 
  ${QT_LIBRARIES} 
  ${OpenCV_LIBS} 
  strmiids
  ${VIDEOLIB_LIBRARY}
)

