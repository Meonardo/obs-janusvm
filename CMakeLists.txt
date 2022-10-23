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
  PRIVATE src/janus-videoroom.c
          src/janus-videoroom.h
          src/rtc_client.cpp
          src/rtc_client.h
          )

target_include_directories(
  janus-videoroom
  PRIVATE "deps/asio/asio/include" "deps/websocketpp"
  "${LIBWEBRTCDIR}/libwebrtc/include"
  )

target_link_libraries(
  janus-videoroom
  PRIVATE OBS::libobs
          OBS::frontend-api
          "${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebrtc/lib/$<CONFIG>/libwebrtc.dll.lib"
          nlohmann_json::nlohmann_json)

target_compile_features(janus-videoroom PRIVATE cxx_std_17)

set_target_properties(janus-videoroom PROPERTIES FOLDER "plugins/janus-videoroom")

add_definitions(-D_WEBSOCKETPP_CPP11_STL_)
target_compile_options(janus-videoroom PRIVATE /wd4267 /wd4996)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH_NAME "64bit")
		set(OBS_BUILDDIR_ARCH "build64")
	else()
		set(ARCH_NAME "32bit")
		set(OBS_BUILDDIR_ARCH "build32")
endif()

  # pre build events, build libwebrtc.dll & copy it to this project
  set(LIBWEBRTC_BUILD_SCRIPT_DIR "${LIBWEBRTCDIR}/libwebrtc/scripts")  
  add_custom_command(TARGET janus-videoroom PRE_BUILD
		COMMAND cmd /c \"${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebrtc/scripts/build-libwebrtc.bat\" "${LIBWEBRTC_BUILD_SCRIPT_DIR}" "$<CONFIG>" "${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebrtc"
	)

  # post build events, copy libwebrtc.dll to obs bin directory
  set(OBS_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/bin/${ARCH_NAME}")
	add_custom_command(TARGET janus-videoroom POST_BUILD
    COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebrtc/lib/$<CONFIG>/libwebrtc.dll" "${OBS_BIN_DIR}"
		)
    COMMAND if $<CONFIG:Debug>==1 (
      "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebrtc/lib/$<CONFIG>/libwebrtc.dll.pdb" "${OBS_BIN_DIR}"
		)
    COMMAND if $<CONFIG:Release>==1 (
			"${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebrtc/lib/$<CONFIG>/libwebrtc.dll" "${OBS_BIN_DIR}"
		)
	)

# Final CMake helpers
setup_plugin_target(janus-videoroom)