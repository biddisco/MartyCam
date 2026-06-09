set(utility_widgets_SRCS CameraSelectorWidget.cpp CameraSelectorWidget.h)

add_library(utility_widgets ${utility_widgets_SRCS})

set_property(TARGET utility_widgets PROPERTY AUTOMOC ON)

target_include_directories(
  utility_widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                         ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(utility_widgets Qt::Core Qt::Widgets Qt::Multimedia
                      ${OpenCV_LIBS})
