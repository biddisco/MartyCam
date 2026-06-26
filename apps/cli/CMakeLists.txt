# -----------------------------------------------------------------------------
# MartyCam CLI Application
# Command-line tool that instantiates a capture thread and prints FPS.
# With --record it uses the stream_buffer_recorder to capture and save video.
# -----------------------------------------------------------------------------

add_executable(martycam_cli main.cpp)

target_include_directories(
  martycam_cli
  PRIVATE ${CMAKE_SOURCE_DIR}/include
          ${CMAKE_BINARY_DIR}/src
          ${CMAKE_BINARY_DIR}
          ${HPX_INCLUDE_DIRS}
)

target_link_libraries(
  martycam_cli
  PRIVATE martycam_core
          martycam_widgets
          ${AVFORMAT_LIB}
          ${AVCODEC_LIB}
          ${AVUTIL_LIB}
          ${SWSCALE_LIB}
          ${AVDEVICE_LIB}
          Fontconfig::Fontconfig
)
