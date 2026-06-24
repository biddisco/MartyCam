# -----------------------------------------------------------------------------
# MartyCam Widgets Library
# Reusable Qt widgets including camera selector, render widget, plots, etc.
# Assimilates the old utility_widgets library.
# -----------------------------------------------------------------------------

set(martycam_widgets_SRCS
    CameraSelectorWidget.cpp
    GraphUpdateFilter.cpp
    IPCameraForm.cpp
    martyplot.cpp
    rendercanvas.cpp
    renderwidget.cpp
    ${CMAKE_SOURCE_DIR}/include/martycam/widgets/CameraSelectorWidget.h
    ${CMAKE_SOURCE_DIR}/include/martycam/widgets/IPCameraForm.h
    ${CMAKE_SOURCE_DIR}/include/martycam/widgets/IPCameraForm.ui
    ${CMAKE_SOURCE_DIR}/include/martycam/widgets/martyplot.h
    ${CMAKE_SOURCE_DIR}/include/martycam/widgets/renderwidget.h
    ${CMAKE_SOURCE_DIR}/include/martycam/widgets/rendercanvas.h
)

add_library(martycam_widgets ${martycam_widgets_SRCS})

set_property(TARGET martycam_widgets PROPERTY AUTOMOC ON)
set_property(TARGET martycam_widgets PROPERTY AUTOUIC ON)
set_property(TARGET martycam_widgets PROPERTY AUTORCC ON)

target_include_directories(
  martycam_widgets
  PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/martycam/widgets>
         $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/martycam_widgets_autogen/include>
  PRIVATE ${CMAKE_SOURCE_DIR}/include/martycam/core
          ${qwt_SOURCE_DIR}/src
)

target_link_libraries(
  martycam_widgets
  PUBLIC martycam_core
         Qt::Widgets
         Qt::OpenGL
         Qt::OpenGLWidgets
         Qt::Svg
         qwt
  PRIVATE Qt::Multimedia
)
