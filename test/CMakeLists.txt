# -----------------------------------------------------------------------------
# Testing
# -----------------------------------------------------------------------------
add_executable(OpenCVCameraTest OpenCVCameraTest.cpp)
add_executable(OpenCV_camera_resolutions OpenCV_camera_resolutions.cpp)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(Qt_camera_resolutions Qt_camera_resolutions.cpp)

target_link_libraries(OpenCVCameraTest ${HPX_LIBRARIES} ${OpenCV_LIBS})
target_link_libraries(OpenCV_camera_resolutions ${HPX_LIBRARIES} ${OpenCV_LIBS})
target_link_libraries(
  Qt_camera_resolutions
  ${OpenCV_LIBS}
  Qt::Core
  Qt::Concurrent
  Qt::Gui
  Qt::OpenGL
  Qt::Svg
  Qt::Widgets
  Qt::Multimedia)
