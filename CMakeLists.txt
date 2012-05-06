PROJECT(MartyCam)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRECT_SOURCE_DIR}/cmake)

cmake_minimum_required(VERSION 2.8)

FIND_PACKAGE(Qt4 4.4 REQUIRED)
FIND_PACKAGE(OpenCV REQUIRED)

INCLUDE(${QT_USE_FILE})
INCLUDE_DIRECTORIES(include)

ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)

LINK_DIRECTORIES(
  C:/bin/ffmpeg/lib
  C:/Code/OpenCV-2.2.0/3rdparty/lib
)
  
SET(VIDEOLIB_LIBRARY 
  "C:\\Code\\OpenCV-2.2.0\\3rdparty\\lib\\videoInput.lib" 
  CACHE FILEPATH "Path to videolib")

SET(QtOpenCV_UIS
  settings.ui
  martycam.ui
)

QT4_WRAP_UI(QtOpenCV_UIS_H 
  ${QtOpenCV_UIS}
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

ADD_SUBDIRECTORY(AnalogWidgets)
INCLUDE_DIRECTORIES(
  "${QT_QTSVG_INCLUDE_DIR}"
  "${QT_QTSCRIPT_INCLUDE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/AnalogWidgets/analogwidgets"
  "${CMAKE_CURRENT_SOURCE_DIR}/AnalogWidgets/analogwidgets/analogwidgets"
)


SET(QtOpenCV_QOBJECTS
  martycam.h
  settings.h
  renderwidget.h
  capturethread.h
)

QT_WRAP_CPP(MartyCam MartyCam_MOC_FILES ${QtOpenCV_QOBJECTS})

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR} 
  ${CMAKE_SOURCE_DIR}/src ${QT_INCLUDE_DIR} 
  ${QT_QTNETWORK_INCLUDE_DIR} 
  ${QT_QTXML_INCLUDE_DIR}
)

LINK_LIBRARIES(
  ${SSL_LIBRARY} 
  ${QT_LIBRARIES} 
  ${QT_QTNETWORK_LIBRARIES}
  ${QT_QTXML_LIBRARIES}
)

SET(QtOpenCV_SRCS
  capturethread.cpp
  filter.cpp
  martycam.cpp
  imagebuffer.cpp
  main.cpp
  processingthread.cpp
  renderwidget.cpp
  settings.cpp
)

ADD_EXECUTABLE(MartyCam WIN32 
  ${QtOpenCV_SRCS} 
  ${QtOpenCV_UIS_H} 
  ${MartyCam_MOC_FILES}
)

target_link_libraries(MartyCam 
  analogue
  ${QT_LIBRARIES} 
  ${OpenCV_LIBS} 
  strmiids
  ${VIDEOLIB_LIBRARY}
)

#----------------------------------------------------
# Test
#----------------------------------------------------

ADD_EXECUTABLE(OpenCVCameraTest test/OpenCVCameraTest.cpp)

target_link_libraries(OpenCVCameraTest
  ${OpenCV_LIBS} 
)

