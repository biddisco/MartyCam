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
# Use stream_buffer_recorder based capture thread instead of OpenCV VideoCapture
# -----------------------------------------------------------------------------
option(MARTYCAM_USE_STREAM_CAPTURE "Use stream_buffer_recorder for frame capture" OFF)
if(MARTYCAM_USE_STREAM_CAPTURE)
  message(STATUS "Using stream_buffer_recorder based capture thread")
  add_compile_definitions(USE_STREAM_CAPTURE_THREAD)
endif()
# -----------------------------------------------------------------------------
# Include GitExternal for cloning other repos internally
# -----------------------------------------------------------------------------
include(GitExternal)
include(ExternalProject)

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

# ------------------------------------------------------------------------------
# nlohmannn_json library
# ------------------------------------------------------------------------------
find_package(nlohmann_json REQUIRED)
message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")

# ------------------------------------------------------------------------------
# Build/Download qwt
# ------------------------------------------------------------------------------
get_external_project(
  PROJECT_NAME
  "qwt"
  GIT_REPO
  "$ENV{HOME}/git-repos/qwt"
  GIT_TAG
  "candle-fixes"
)

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
# libs we need for video processing/recording
# -----------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(AVDEVICE REQUIRED libavdevice)
# Manually find the absolute path to each library using the Spack directory hints
find_library(AVFORMAT_LIB NAMES avformat HINTS ${AVFORMAT_LIBRARY_DIRS})
find_library(AVCODEC_LIB NAMES avcodec HINTS ${AVCODEC_LIBRARY_DIRS})
find_library(AVUTIL_LIB NAMES avutil HINTS ${AVUTIL_LIBRARY_DIRS})
find_library(SWSCALE_LIB NAMES swscale HINTS ${SWSCALE_LIBRARY_DIRS})
find_library(AVDEVICE_LIB NAMES avdevice HINTS ${AVDEVICE_LIBRARY_DIRS})
message(
  STATUS
    "Found FFmpeg libraries: ${AVFORMAT_LIB}, ${AVCODEC_LIB}, ${AVUTIL_LIB}, ${SWSCALE_LIB}, ${AVDEVICE_LIB}"
)

# pkg_check_modules() include dirs can be empty on some setups; fall back to pkg-config "includedir"
# for each module.
set(FFMPEG_INCLUDE_DIRS ${AVFORMAT_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS} ${AVUTIL_INCLUDE_DIRS}
                        ${SWSCALE_INCLUDE_DIRS} ${AVDEVICE_INCLUDE_DIRS}
)
if("${FFMPEG_INCLUDE_DIRS}" STREQUAL "")
  pkg_get_variable(AVFORMAT_PKG_INCLUDEDIR libavformat includedir)
  pkg_get_variable(AVCODEC_PKG_INCLUDEDIR libavcodec includedir)
  pkg_get_variable(AVUTIL_PKG_INCLUDEDIR libavutil includedir)
  pkg_get_variable(SWSCALE_PKG_INCLUDEDIR libswscale includedir)
  pkg_get_variable(AVDEVICE_PKG_INCLUDEDIR libavdevice includedir)

  list(
    APPEND
    FFMPEG_INCLUDE_DIRS
    ${AVFORMAT_PKG_INCLUDEDIR}
    ${AVCODEC_PKG_INCLUDEDIR}
    ${AVUTIL_PKG_INCLUDEDIR}
    ${SWSCALE_PKG_INCLUDEDIR}
    ${AVDEVICE_PKG_INCLUDEDIR}
  )
endif()
list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)

message(STATUS "Found FFmpeg include dirs (pkg-config): ${FFMPEG_INCLUDE_DIRS}")

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
# Setup libs
# -----------------------------------------------------------------------------
# QWT is fetched via get_external_project() earlier in this file
set(MARTYCAM_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/include
                          ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/src)

add_subdirectory(src/debug)
add_subdirectory(src/core)
add_subdirectory(src/widgets)
add_subdirectory(apps/gui)
add_subdirectory(apps/cli)

add_subdirectory(test)
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
