# -----------------------------------------------------------------------------
# MartyCam GUI Application
# Main window, settings panel, and application entry point.
# -----------------------------------------------------------------------------

set(martycam_app_SRCS
    main.cpp
    martycam.cpp
    martycam.h
    settings.cpp
    settings.h
    martycam.ui
    settings.ui
    settings_xxx.ui
    martycam.qrc
    resources.qrc
)

if(WIN32)
  list(
    APPEND
    martycam_app_SRCS
    ${CMAKE_SOURCE_DIR}/videoInput/videoInputSrcAndDemos/libs/videoInput/videoInput.cpp
  )
endif()

add_executable(martycam ${martycam_app_SRCS})

set_property(TARGET martycam PROPERTY AUTOMOC ON)
set_property(TARGET martycam PROPERTY AUTOUIC ON)
set_property(TARGET martycam PROPERTY AUTORCC ON)

target_include_directories(
  martycam
  PRIVATE ${CMAKE_SOURCE_DIR}/include
          ${CMAKE_SOURCE_DIR}/apps/gui
          ${CMAKE_BINARY_DIR}/apps/gui
          ${CMAKE_BINARY_DIR}/src
          ${CMAKE_SOURCE_DIR}/streamulus/src
          ${qwt_SOURCE_DIR}/src
          ${HPX_INCLUDE_DIRS}
)

target_link_libraries(
  martycam
  PRIVATE martycam_core
          martycam_widgets
          Qt::Core
          Qt::Concurrent
          Qt::Gui
          Qt::OpenGL
          Qt::Svg
          Qt::Widgets
          Qt::OpenGLWidgets
          Qt::Multimedia
          ${MARTYCAM_LOG_LIBS}
          Fontconfig::Fontconfig
)
