#-----------------------------------------------------------------------------
# Testing
#-----------------------------------------------------------------------------
ADD_EXECUTABLE(OpenCVCameraTest
  OpenCVCameraTest.cpp
)

target_link_libraries(OpenCVCameraTest
  ${HPX_LIBRARIES}
  ${OpenCV_LIBS}
)

