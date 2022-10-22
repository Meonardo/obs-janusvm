project(janus-videoroom VERSION 0.0.1)

option(ENABLE_JANUS "Enable building OBS with janus-videoroom plugin" ON)

if(NOT ENABLE_JANUS OR NOT ENABLE_UI)
  message(STATUS "OBS:  DISABLED   janus-videoroom")
  return()
endif()

# Submodule deps check
if(NOT
   (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/json/CMakeLists.txt
    AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp/CMakeLists.txt
    AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include/asio.hpp))
  obs_status(FATAL_ERROR "janus-videoroom submodule deps not available.")
endif()

# Plugin tests flag
option(PLUGIN_TESTS "Enable plugin runtime tests" OFF)

# # Find nlohmann
# set(JSON_BuildTests
#     OFF
#     CACHE INTERNAL "")
# add_subdirectory(deps/json)

# Tell websocketpp not to use system boost
add_definitions(-DASIO_STANDALONE)

# Setup target
add_library(janus-videoroom MODULE)
add_library(OBS::janus ALIAS janus-videoroom)

target_sources(
  janus-videoroom
  PRIVATE src/janus-videoroom.cpp
          src/janus-videoroom.h
          )

target_include_directories(
  janus-videoroom
  PRIVATE "deps/asio/asio/include" "deps/websocketpp")

target_link_libraries(
  janus-videoroom
  PRIVATE OBS::libobs
          OBS::frontend-api
          nlohmann_json::nlohmann_json)

target_compile_features(janus-videoroom PRIVATE cxx_std_17)

set_target_properties(janus-videoroom PROPERTIES FOLDER "plugins/janus-videoroom")

if(PLUGIN_TESTS)
  target_compile_definitions(janus-videoroom PRIVATE PLUGIN_TESTS)
endif()

# Random other things
if(WIN32)
  add_definitions(-D_WEBSOCKETPP_CPP11_STL_)
endif()

if(MSVC)
  target_compile_options(janus-videoroom PRIVATE /wd4267 /wd4996)
endif()

# Final CMake helpers
setup_plugin_target(janus-videoroom)