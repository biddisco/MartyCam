# -----------------------------------------------------------------------------
# MartyCam Core Library Camera capture, image processing, filters, and utilities. No QtWidgets
# dependency — suitable for CLI applications.
# -----------------------------------------------------------------------------

if(MARTYCAM_USE_STREAM_CAPTURE)
  set(capture_source stream_capture_thread.cpp)
else()
  set(capture_source capturethread.cpp)
endif()

set(martycam_core_SRCS
    ${capture_source}
    camera_utils.cpp
    timelapse_ffmpeg_writer.cpp
    filter.cpp
    PSNRFilter.cpp
    MotionFilter.cpp
    DecayFilter.cpp
    FaceRecogFilter.cpp
    processingthread.cpp
    stream_buffer_recorder.cpp
    ${CMAKE_SOURCE_DIR}/include/martycam/core/processingthread.h
)

if(MARTYCAM_USE_STREAM_CAPTURE)
  list(APPEND martycam_core_SRCS ${CMAKE_SOURCE_DIR}/include/martycam/core/stream_capture_thread.h)
else()
  list(APPEND martycam_core_SRCS ${CMAKE_SOURCE_DIR}/include/martycam/core/capturethread.h)
endif()

add_library(martycam_core ${martycam_core_SRCS})

set_property(TARGET martycam_core PROPERTY AUTOMOC ON)

if(MARTYCAM_USE_STREAM_CAPTURE)
  target_compile_definitions(martycam_core PUBLIC USE_STREAM_CAPTURE_THREAD)
endif()

target_include_directories(
  martycam_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
         ${FFMPEG_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/streamulus/src ${HPX_INCLUDE_DIRS}
  PRIVATE ${CMAKE_SOURCE_DIR}/include/martycam/core ${CMAKE_BINARY_DIR}
)

target_link_libraries(
  martycam_core
  PUBLIC Qt::Core
         Qt::Concurrent
         ${OpenCV_LIBS}
         HPX::hpx
         ${Boost_LIBRARIES}
         nlohmann_json::nlohmann_json
         martycam_debug
  PRIVATE ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB} ${AVDEVICE_LIB}
)
