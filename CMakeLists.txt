cmake_minimum_required(VERSION 3.10...3.30)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

project(martycam CXX)

# --------------------------------------------------
# CXX standard
# --------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------
# Setup Output directories
# --------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all Executables."
)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all Libraries"
)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    CACHE PATH "Single Directory for all static libraries."
)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ------------------------------------------------------------------------------
# Logging / Log level support for messages >= level
# ------------------------------------------------------------------------------
set(MARTYCAM_LOG_LEVEL "off" CACHE STRING "Enable logging for messages >=level")
set_property(
  CACHE MARTYCAM_LOG_LEVEL
  PROPERTY STRINGS
           off
           trace
           debug
           info
           warn
           error
           critical
)

# -----------------------------------------------------------------------------
# Include GitExternal for cloning other repos internally
# -----------------------------------------------------------------------------
include(GitExternal)

# -----------------------------------------------------------------------------
# Include Subproject for building other cmake projects as subdirs
# -----------------------------------------------------------------------------
include(SubProject)
include(ConfigNamespace)

# -----------------------------------------------------------------------------
# Find Fontconfig
# -----------------------------------------------------------------------------
find_package(Fontconfig REQUIRED)

# -----------------------------------------------------------------------------
# Find OpenCV
# -----------------------------------------------------------------------------
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui objdetect)
message(STATUS "Found OpenCV: ${OpenCV_LIBS}")

# ------------------------------------------------------------------------------
# Find Qt6
# ------------------------------------------------------------------------------
# this line to force correct var so that Qt-AdvancedDocking uses Qt6 by default
find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)
#
set(qtcomponents
    Core
    Widgets
    Gui
    OpenGL
    OpenGLWidgets
    Concurrent
    Svg
    Multimedia
)

find_package(Qt6 REQUIRED COMPONENTS ${qtcomponents})

# ------------------------------------------------------------------------------
# fmt formatting library
# ------------------------------------------------------------------------------
find_package(fmt REQUIRED)
message(STATUS "Found fmt: ${fmt_VERSION}")
# ------------------------------------------------------------------------------
# spdlog logging library
# ------------------------------------------------------------------------------
string(TOUPPER ${MARTYCAM_LOG_LEVEL} MARTYCAM_LOG_LEVEL_UPPERCASE)
if(NOT "${MARTYCAM_LOG_LEVEL_UPPERCASE}" MATCHES "OFF")
  find_package(spdlog REQUIRED)
  message(STATUS "Found spdlog: ${spdlog_VERSION}")
  set(MARTYCAM_LOG_LIBS martycam_debug spdlog::spdlog fmt::fmt)
endif()

# -----------------------------------------------------------------------------
# Find HPX
# -----------------------------------------------------------------------------
find_package(HPX REQUIRED)
message(STATUS "Found HPX: ${HPX_VERSION}")

# -----------------------------------------------------------------------------
# Find Boost
# -----------------------------------------------------------------------------
# option(Boost_USE_STATIC "Use static Boost libraries" ON)
find_package(Boost 1.65 REQUIRED thread system)
message(STATUS "Found Boost: ${Boost_VERSION}")

# -----------------------------------------------------------------------------
# Clone Streamulus locally : used for experimental processing of data stream
# -----------------------------------------------------------------------------
git_external(
  ${CMAKE_CURRENT_SOURCE_DIR}/streamulus git@github.com:biddisco/streamulus.git master VERBOSE
  SHALLOW
)

# -----------------------------------------------------------------------------
# Set DATA_PATH directory of the repository
# -----------------------------------------------------------------------------
set(DATA_PATH "${PROJECT_SOURCE_DIR}/data")

configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")

# -----------------------------------------------------------------------------
# MSVC specifics
# -----------------------------------------------------------------------------
if(WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  # option(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "/Zc:wchar_t-")
endif()

# -----------------------------------------------------------------------------
# Qt resource files
# -----------------------------------------------------------------------------
# set(martycam_QT_RCCS resources.qrc )
#
# -----------------------------------------------------------------------------
# Qt GUI interface wrapping
# -----------------------------------------------------------------------------
# set(martycam_QT_UIS settings.ui martycam.ui IPCameraForm.ui )
#
# -----------------------------------------------------------------------------
# Qt QObjects for MOC
# -----------------------------------------------------------------------------
# set(martycam_QT_MOC_HDRS martycam.h IPCameraForm.h settings.h renderwidget.h processingthread.h )
#
# -----------------------------------------------------------------------------
# Invoke Qt wrapping
# -----------------------------------------------------------------------------
#
# generate rules that will run rcc on the resource files QT6_ADD_RESOURCES(martycam_RCC_SRCS
# ${martycam_QT_RCCS} )
#
# generate rules that will run uic on .ui files: QT6_WRAP_UI(martycam_QT_UIS_H ${martycam_QT_UIS} )
#
# generate rules that will run moc on QObject header files QT_WRAP_CPP(martycam martycam_MOC_SRCS
# ${martycam_QT_MOC_HDRS} )

# -----------------------------------------------------------------------------
# Analogue widgets library used for graph plotting
# -----------------------------------------------------------------------------
add_subdirectory(AnalogWidgets)

# -----------------------------------------------------------------------------
# Setup libs
# -----------------------------------------------------------------------------
set(MARTYCAM_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/src)

add_subdirectory(src/debug)

add_subdirectory(utility_widgets)
add_subdirectory(test)

# -----------------------------------------------------------------------------
# Main source files
# -----------------------------------------------------------------------------
set(martycam_SRCS
    capturethread.cpp
    filter.cpp
    PSNRFilter.cpp
    MotionFilter.cpp
    DecayFilter.cpp
    GraphUpdateFilter.cpp
    martycam.cpp
    main.cpp
    processingthread.cpp
    rendercanvas.h
    renderwidget.cpp
    settings.cpp
    IPCameraForm.cpp
    martycam.rc
    FaceRecogFilter.cpp
)
if(WIN32)
  set(martycam_SRCS
      ${martycam_SRCS}
      ${CMAKE_CURRENT_SOURCE_DIR}/videoInput/videoInputSrcAndDemos/libs/videoInput/videoInput.cpp
  )
endif()

# -----------------------------------------------------------------------------
# Target executable
# -----------------------------------------------------------------------------
add_executable(
  martycam ${martycam_SRCS} ${martycam_RCC_SRCS} ${martycam_QT_UIS_H} ${martycam_MOC_SRCS}
)
target_include_directories(martycam PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/streamulus/src")

# qt5_use_modules(martycam Core Widgets Gui Svg OpenGL Concurrent) Qt properties
set_property(TARGET martycam PROPERTY AUTOMOC ON)
set_property(TARGET martycam PROPERTY AUTOUIC ON)
set_property(TARGET martycam PROPERTY AUTORCC ON)

target_link_libraries(
  martycam
  Qt::Core
  Qt::Concurrent
  Qt::Gui
  Qt::OpenGL
  Qt::Svg
  Qt::Widgets
  Qt::OpenGLWidgets
)

# -----------------------------------------------------------------------------
# Include directories
# -----------------------------------------------------------------------------
target_include_directories(
  martycam
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
          "${MARTYCAM_INCLUDE_ROOT}"
          "${CMAKE_CURRENT_BINARY_DIR}"
          "${CMAKE_CURRENT_BINARY_DIR}/src"
          "${CMAKE_CURRENT_SOURCE_DIR}/AnalogWidgets/analogwidgets"
          "${CMAKE_CURRENT_SOURCE_DIR}/AnalogWidgets/analogwidgets/analogwidgets"
          ${HPX_INCLUDE_DIRS}
)

target_link_libraries(
  martycam
  analogue
  utility_widgets
  ${MARTYCAM_LOG_LIBS}
  ${OpenCV_LIBS}
  ${Boost_LIBRARIES}
  HPX::hpx
  Fontconfig::Fontconfig
)

add_subdirectory(webcam_test)

# ------------------------------------------------------------------------------
# generate config settings headers
# ------------------------------------------------------------------------------
add_config_define_namespace(
  NAMESPACE ALL DEFINE MARTYCAM_SOURCE_DIR VALUE "\"${MARTYCAM_SOURCE_DIR}\""
)
add_config_define_namespace(
  NAMESPACE ALL DEFINE MARTYCAM_BINARY_DIR VALUE "\"${MARTYCAM_BINARY_DIR}\""
)
add_config_define_namespace(NAMESPACE ALL DEFINE QT_NO_PRIVATE_MODULE_WARNING "1")

write_config_defines_file_mc(
  NAMESPACE ALL FILENAME "${PROJECT_BINARY_DIR}/src/martycam/config-defines.hpp"
)
