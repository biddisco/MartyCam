# -----------------------------------------------------------------------------
# Testing
# -----------------------------------------------------------------------------
add_executable(OpenCVCameraTest OpenCVCameraTest.cpp)
add_executable(OpenCV_camera_resolutions OpenCV_camera_resolutions.cpp)
target_link_libraries(OpenCVCameraTest ${HPX_LIBRARIES} ${OpenCV_LIBS})
target_link_libraries(OpenCV_camera_resolutions ${HPX_LIBRARIES} ${OpenCV_LIBS})


add_executable(Qt_camera_resolutions Qt_camera_resolutions.cpp)
set_property(TARGET Qt_camera_resolutions PROPERTY AUTOMOC ON)
set_property(TARGET Qt_camera_resolutions PROPERTY AUTOUIC ON)
set_property(TARGET Qt_camera_resolutions PROPERTY AUTORCC ON)

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
