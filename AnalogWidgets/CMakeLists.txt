#--------------------------------------------------
# analogue library
#--------------------------------------------------

INCLUDE_DIRECTORIES(
  "${QT_QTSVG_INCLUDE_DIR}"
  "${QT_QTSCRIPT_INCLUDE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets"
)

SET(CPP_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/bitmapbutton.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/counter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/functions.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/led.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/manometer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/potentiometer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/scriptwidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/thermometer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/wallclock.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/widgetwithbackground.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/abstractmeter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart/chartdecorator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart/scalegrid.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart/channel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart/standard/channeldecorator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart/standard/glassdecorator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart/standard/labelsdecorator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart/standard/legenddecorator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/chart/standard/scalegriddecorator.cpp"
  )

SET(QOBJECT_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/item.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/counter.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/led.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/manometer.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/potentiometer.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/scriptwidget.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/thermometer.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/wallclock.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/widgetwithbackground.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/abstractmeter.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/bitmapbutton.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/analogwidgets/chart.h"
)

QT_WRAP_CPP(
  analogue
  analogue_MOC_FILES 
  ${QOBJECT_HDRS}
)

QT4_ADD_RESOURCES(
  analogue_RSRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc"
  "${CMAKE_CURRENT_SOURCE_DIR}/analogwidgets/code/analogwidgets.qrc"
)

ADD_LIBRARY(analogue STATIC
  ${analogue_RSRCS}
  ${analogue_MOC_FILES}
  ${CPP_SRCS}
)

#--------------------------------------------------
# ScriptTool
#--------------------------------------------------

IF (QT_QTSCRIPT_LIBRARY)
  QT4_WRAP_UI( 
    scripttool_UIS_H
    scripttool/scripttool.ui
  )

  QT_WRAP_CPP(
    scripttool1 
    scripttool_MOC_FILES 
    scripttool/scripttool.h
  )

  include_directories(${CMAKE_CURRENT_BINARY_DIR})

  ADD_DEFINITIONS(-DQJS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

  ADD_EXECUTABLE(scripttool
    ${scripttool_UIS_H}
    ${scripttool_MOC_FILES}
    scripttool/main.cpp
    scripttool/scripttool.cpp
  )

  target_link_libraries(scripttool
    analogue
    ${QT_LIBRARIES} 
    ${QT_QTSVG_LIBRARY}
    ${QT_QTSCRIPT_LIBRARY}  
  )

ENDIF (QT_QTSCRIPT_LIBRARY)

#--------------------------------------------------
# Testing
#--------------------------------------------------

QT4_WRAP_UI( 
  test_UIS_H
  test/testform.ui
)

QT_WRAP_CPP(
  test
  test_MOC_FILES 
  test/test.h
  test/widgettester.h
)

QT4_ADD_RESOURCES(
  test_RSRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/test/test.qrc"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

ADD_DEFINITIONS(-DWTR_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

ADD_EXECUTABLE(test
  ${test_UIS_H}
  ${test_RSRCS}
  ${test_MOC_FILES}
  test/main.cpp
  test/test.cpp
  test/widgettester.cpp
)

target_link_libraries(test
  analogue
  ${QT_LIBRARIES} 
  ${QT_QTSVG_LIBRARY}
)
