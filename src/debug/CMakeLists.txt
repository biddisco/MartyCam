# Currently there are no source files for this library
set(martycam_debug_SRCS demangle_helper.hpp logging.hpp print.cpp print.hpp stacktrace.hpp)

# debug library
add_library(martycam_debug ${martycam_debug_SRCS})

target_include_directories(
  martycam_debug INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_include_directories(martycam_debug PUBLIC ${MARTYCAM_INCLUDE_ROOT})
target_link_libraries(martycam_debug PUBLIC HPX::hpx)

message("HERE")
# ------------------------------------------------------------------------------
# logging support
# ------------------------------------------------------------------------------
add_config_define_namespace(NAMESPACE LOGGING DEFINE MARTYCAM_LOGGING_ENABLED VALUE "1")
add_config_define_namespace(
  NAMESPACE LOGGING UNDEFINE DEFINE SPDLOG_ACTIVE_LEVEL
  VALUE "SPDLOG_LEVEL_${MARTYCAM_LOG_LEVEL_UPPERCASE}"
)
message(
  "
add_config_define_namespace(
  NAMESPACE LOGGING UNDEFINE DEFINE SPDLOG_ACTIVE_LEVEL
  VALUE "SPDLOG_LEVEL_${MARTYCAM_LOG_LEVEL_UPPERCASE}"
)
  "
)

if(NOT "${MARTYCAM_LOG_LEVEL_UPPERCASE}" MATCHES "OFF")
  message("here AGAIN")
  target_link_libraries(martycam_debug PUBLIC spdlog::spdlog fmt::fmt)
endif()

write_config_defines_file_mc(
  NAMESPACE LOGGING FILENAME "${PROJECT_BINARY_DIR}/src/martycam/config-logging.hpp"
)

message("Still here")
