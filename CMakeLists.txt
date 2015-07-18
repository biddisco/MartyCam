project(MartyCam)

#-----------------------------------------------------------------------------
# CMake directives
#-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 2.8)
SET(CMAKE_MODULE_PATH ${CMAKE_CURRECT_SOURCE_DIR}/cmake)

#-----------------------------------------------------------------------------
# Include GitExternal for cloning other repos internally
#-----------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/GitExternal.cmake)

#-----------------------------------------------------------------------------
# Find OpenCV
#-----------------------------------------------------------------------------
find_package(OpenCV REQUIRED)

#-----------------------------------------------------------------------------
# Find Qt
#-----------------------------------------------------------------------------
# by default only QtCore and QtGui modules are enabled
set(QT_USE_QTCORE   TRUE)   
set(QT_USE_QTGUI    TRUE)   
set(QT_USE_QTSVG    TRUE)   
set(QT_USE_QTSCRIPT FALSE)

find_package(Qt4 4.7 REQUIRED)
include(${QT_USE_FILE})

#-----------------------------------------------------------------------------
# Find Boost
#-----------------------------------------------------------------------------
option(Boost_USE_STATIC "Use static Boost libraries" ON)
find_package(Boost 1.49 REQUIRED thread system) 
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIR})
message("Boost link dir is ${Boost_LIBRARY_DIRS}")

#-----------------------------------------------------------------------------
# Clone videoInput locally for easy webcam integration
#-----------------------------------------------------------------------------
git_external(
  ${CMAKE_CURRENT_SOURCE_DIR}/videoInput
  git@github.com:ofTheo/videoInput.git
  master
  VERBOSE
  SHALLOW
)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/streamulus/src"
)

#-----------------------------------------------------------------------------
# Clone Streamulus locally : used for experimental processing of data stream
#-----------------------------------------------------------------------------
git_external(
  ${CMAKE_CURRENT_SOURCE_DIR}/streamulus
  git@github.com:biddisco/streamulus.git
  master
  VERBOSE
  SHALLOW
)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/videoInput/videoInputSrcAndDemos/libs/videoInput"
)

#-----------------------------------------------------------------------------
# MSVC specifics
#-----------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "/Zc:wchar_t-")

#-----------------------------------------------------------------------------
# Qt resource files
#-----------------------------------------------------------------------------
SET(MartyCam_QT_RCCS 
  resources.qrc
)

#-----------------------------------------------------------------------------
# Qt GUI interface wrapping
#-----------------------------------------------------------------------------
set(MartyCam_QT_UIS
  settings.ui
  martycam.ui
  IPCameraForm.ui
)

#-----------------------------------------------------------------------------
# Qt QObjects for MOC
#-----------------------------------------------------------------------------
set(MartyCam_QT_MOC_HDRS
  martycam.h
  IPCameraForm.h
  settings.h
  renderwidget.h
  capturethread.h
  processingthread.h
)

#-----------------------------------------------------------------------------
# Invoke Qt wrapping
#-----------------------------------------------------------------------------

# generate rules that will run rcc on the resource files
QT4_ADD_RESOURCES(MartyCam_RCC_SRCS 
  ${MartyCam_QT_RCCS}
)

# generate rules that will run uic on .ui files:
QT4_WRAP_UI(MartyCam_QT_UIS_H 
  ${MartyCam_QT_UIS}
)

# generate rules that will run moc on QObject header files
QT_WRAP_CPP(MartyCam MartyCam_MOC_SRCS 
  ${MartyCam_QT_MOC_HDRS}
)

#-----------------------------------------------------------------------------
# Analogue widgets library used for graph plotting
#-----------------------------------------------------------------------------
ADD_SUBDIRECTORY(AnalogWidgets)

#-----------------------------------------------------------------------------
# Include directories
#-----------------------------------------------------------------------------
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/AnalogWidgets/analogwidgets"
  "${CMAKE_CURRENT_SOURCE_DIR}/AnalogWidgets/analogwidgets/analogwidgets"
  "${QT_INCLUDE_DIR}"
)

#-----------------------------------------------------------------------------
# Main source files 
#-----------------------------------------------------------------------------
SET(MartyCam_SRCS
  capturethread.cpp
  filter.cpp
  PSNRFilter.cpp
  MotionFilter.cpp
  DecayFilter.cpp
  GraphUpdateFilter.cpp
  martycam.cpp
  main.cpp
  processingthread.cpp
  renderwidget.cpp
  settings.cpp
  IPCameraForm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/videoInput/videoInputSrcAndDemos/libs/videoInput/videoInput.cpp
  MartyCam.rc
)

#-----------------------------------------------------------------------------
# Target executable
#-----------------------------------------------------------------------------
ADD_EXECUTABLE(MartyCam 
  ${MartyCam_SRCS} 
  ${MartyCam_RCC_SRCS}
  ${MartyCam_QT_UIS_H}
  ${MartyCam_MOC_SRCS}
)

target_link_libraries(MartyCam 
  analogue
  ${QT_LIBRARIES} 
  ${OpenCV_LIBS} 
  ${Boost_LIBRARIES}
)

#-----------------------------------------------------------------------------
# Testing
#-----------------------------------------------------------------------------
ADD_EXECUTABLE(OpenCVCameraTest 
  test/OpenCVCameraTest.cpp
)

target_link_libraries(OpenCVCameraTest
  ${OpenCV_LIBS} 
)
