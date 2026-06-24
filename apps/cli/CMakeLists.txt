# -----------------------------------------------------------------------------
# MartyCam CLI Application (stub)
# Future command-line-only application linking to martycam_core.
# -----------------------------------------------------------------------------

add_executable(martycam_cli main.cpp)

target_link_libraries(martycam_cli PRIVATE martycam_core)
