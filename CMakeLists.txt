project(MartyCam)

#-----------------------------------------------------------------------------
# CMake directives
#-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0059 NEW)


SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake;5.9.6/gcc_64/lib/cmake/Qt5/)

#-----------------------------------------------------------------------------
# Include GitExternal for cloning other repos internally
#-----------------------------------------------------------------------------
include(GitExternal)

#-----------------------------------------------------------------------------
# Include Subproject for building other cmake projects as subdirs
#-----------------------------------------------------------------------------
include(SubProject)

#--------------------------------------------------
# Setup Output directories
#--------------------------------------------------
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables."
)
SET (CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Libraries"
)
SET (CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all static libraries."
)

set (CMAKE_CXX_STANDARD 11)
#-----------------------------------------------------------------------------
# Find OpenCV
#-----------------------------------------------------------------------------
find_package(OpenCV REQUIRED)
message("opencv libs are " ${OpenCV_LIBS})

#-----------------------------------------------------------------------------
# Find Qt
#-----------------------------------------------------------------------------
find_package(Qt5 COMPONENTS Core Widgets Gui Svg OpenGL Concurrent)

#include(${QT5_USE_FILE})

#-----------------------------------------------------------------------------
# Find Boost
#-----------------------------------------------------------------------------
#option(Boost_USE_STATIC "Use static Boost libraries" ON)
find_package(Boost 1.49 REQUIRED thread system)
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIR})
message("Boost library dir is ${Boost_LIBRARY_DIRS}")
message("Boost include dir is ${Boost_INCLUDE_DIRS}")

#-----------------------------------------------------------------------------
# MSVC specifics
#-----------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
#option(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "/Zc:wchar_t-")

#-----------------------------------------------------------------------------
# Qt resource files
#-----------------------------------------------------------------------------
set(MartyCam_QT_RCCS
  resources.qrc
)

#-----------------------------------------------------------------------------
# Qt GUI interface wrapping
#-----------------------------------------------------------------------------
set(MartyCam_QT_UIS
  settings.ui
  martycam.ui
)

#-----------------------------------------------------------------------------
# Qt QObjects for MOC
#-----------------------------------------------------------------------------
set(MartyCam_QT_MOC_HDRS
  martycam.h
  settings.h
  renderwidget.h
  capturethread.h
  processingthread.h
)

#-----------------------------------------------------------------------------
# Invoke Qt wrapping
#-----------------------------------------------------------------------------

# generate rules that will run rcc on the resource files
QT5_ADD_RESOURCES(MartyCam_RCC_SRCS
  ${MartyCam_QT_RCCS}
)

# generate rules that will run uic on .ui files:
QT5_WRAP_UI(MartyCam_QT_UIS_H
  ${MartyCam_QT_UIS}
)

# generate rules that will run moc on QObject header files
QT_WRAP_CPP(MartyCam MartyCam_MOC_SRCS
  ${MartyCam_QT_MOC_HDRS}
)

#-----------------------------------------------------------------------------
# Include directories
#-----------------------------------------------------------------------------
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
)

#-----------------------------------------------------------------------------
# Main source files
#-----------------------------------------------------------------------------
SET(MartyCam_SRCS
  capturethread.cpp
  filter.cpp
  MotionFilter.cpp
  martycam.cpp
  main.cpp
  processingthread.cpp
  renderwidget.cpp
  settings.cpp
  MartyCam.rc
)
if (WIN32)
  set(MartyCam_SRCS
    ${MartyCam_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/videoInput/videoInputSrcAndDemos/libs/videoInput/videoInput.cpp
  )
endif()

message("MartyCam_SRCS are ${MartyCam_SRCS}")
#-----------------------------------------------------------------------------
# Target executable
#-----------------------------------------------------------------------------
ADD_EXECUTABLE(MartyCam
  ${MartyCam_SRCS}
  ${MartyCam_RCC_SRCS}
  ${MartyCam_QT_UIS_H}
  ${MartyCam_MOC_SRCS}
)

qt5_use_modules(MartyCam Core Widgets Gui Svg OpenGL Concurrent)

target_link_libraries(MartyCam
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

ADD_EXECUTABLE(OpenCVCameraTest2
  test/OpenCVCameraTest2.cpp
)

target_link_libraries(OpenCVCameraTest2
  ${OpenCV_LIBS}
)


