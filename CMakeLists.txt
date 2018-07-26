cmake_minimum_required(VERSION 3.5.1 FATAL_ERROR)

project(MartyCam CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

#-----------------------------------------------------------------------------
# Find Qt5
#-----------------------------------------------------------------------------
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Svg OpenGL Concurrent)

#-----------------------------------------------------------------------------
# Find HPX
#-----------------------------------------------------------------------------
find_package(HPX REQUIRED)
message("HPX libs are " ${HPX_LIBRARIES})

#-----------------------------------------------------------------------------
# Find OpenCV
#-----------------------------------------------------------------------------
find_package(OpenCV REQUIRED)
message("opencv libs are " ${OpenCV_LIBS})

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
add_hpx_executable(MartyCam
                   MODULE MartyCam
                   DEPENDENCIES ${OpenCV_LIBS}
                   SOURCES ${MartyCam_SRCS}
                           ${MartyCam_RCC_SRCS}
                           ${MartyCam_QT_UIS_H}
                           ${MartyCam_MOC_SRCS}
                   COMPONENT_DEPENDENCIES iostreams
)



qt5_use_modules(MartyCam_exe Core Widgets Gui Svg OpenGL Concurrent)

target_link_libraries(MartyCam_exe
  ${OpenCV_LIBS}
  ${HPX_TLL_PUBLIC}
  ${Boost_LIBRARIES}
  Qt5::Core
  Qt5::Widgets
  Qt5::Gui
  Qt5::Svg
  Qt5::OpenGL
  Qt5::Concurrent
) 