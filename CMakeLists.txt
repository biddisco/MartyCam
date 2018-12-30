cmake_minimum_required (VERSION 3.5.1)

project(MartyCam CXX)

#--------------------------------------------------
# CXX standard
#--------------------------------------------------
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

#-----------------------------------------------------------------------------
# Include GitExternal for cloning other repos internally
#-----------------------------------------------------------------------------
include(GitExternal)

#-----------------------------------------------------------------------------
# Include Subproject for building other cmake projects as subdirs
#-----------------------------------------------------------------------------
include(SubProject)

#-----------------------------------------------------------------------------
# Find OpenCV
#-----------------------------------------------------------------------------
find_package(OpenCV REQUIRED)
message("opencv libs are " ${OpenCV_LIBS})

#-----------------------------------------------------------------------------
# Find Qt
#-----------------------------------------------------------------------------
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Svg OpenGL Concurrent)

#-----------------------------------------------------------------------------
# Find HPX
#-----------------------------------------------------------------------------
find_package(HPX REQUIRED)
message("HPX libs are " ${HPX_LIBRARIES})

add_subdirectory(test)

#-----------------------------------------------------------------------------
# Find Boost
#-----------------------------------------------------------------------------
#option(Boost_USE_STATIC "Use static Boost libraries" ON)
find_package(Boost 1.65 REQUIRED thread system)
message("Boost link dir is ${Boost_LIBRARY_DIRS}")

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
  "${CMAKE_CURRENT_SOURCE_DIR}/streamulus/src"
)

#-----------------------------------------------------------------------------
# Set DATA_PATH directory of the repository
#-----------------------------------------------------------------------------
set(DATA_PATH "${PROJECT_SOURCE_DIR}/data")

configure_file(
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
)

#-----------------------------------------------------------------------------
# MSVC specifics
#-----------------------------------------------------------------------------
if(WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  #option(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "/Zc:wchar_t-")
endif()

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
# Analogue widgets library used for graph plotting
#-----------------------------------------------------------------------------
ADD_SUBDIRECTORY(AnalogWidgets)

#-----------------------------------------------------------------------------
# Main source files
#-----------------------------------------------------------------------------
set(MartyCam_SRCS
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
  MartyCam.rc
  FaceRecogFilter.cpp
)
if (WIN32)
  set(MartyCam_SRCS
    ${MartyCam_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/videoInput/videoInputSrcAndDemos/libs/videoInput/videoInput.cpp
  )
endif()

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

#-----------------------------------------------------------------------------
# Include directories
#-----------------------------------------------------------------------------
target_include_directories(MartyCam 
  PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/AnalogWidgets/analogwidgets"
    "${CMAKE_CURRENT_SOURCE_DIR}/AnalogWidgets/analogwidgets/analogwidgets"
    ${HPX_INCLUDE_DIRS}
)


target_link_libraries(MartyCam
  analogue
  iostreams_component
  ${HPX_LIBRARIES}
  ${OpenCV_LIBS}
  ${Boost_LIBRARIES}
)
#hpx_setup_target(MartyCam COMPONENT_DEPENDENCIES iostreams)

add_subdirectory(webcam_test)
