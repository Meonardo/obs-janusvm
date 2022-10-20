project(janus-videoroom VERSION 0.0.1)
set(OBS_WEBSOCKET_RPC_VERSION 1)

option(ENABLE_JANUS "Enable building OBS with janus-videoroom plugin" ON)

if(NOT ENABLE_JANUS OR NOT ENABLE_UI)
  message(STATUS "DISABLED janus-videoroom")
  return()
endif()

# Submodule deps check
if(NOT
   (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/json/CMakeLists.txt
    AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp/CMakeLists.txt
    AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include/asio.hpp))
  obs_status(FATAL_ERROR "janus-videoroom submodule deps not available.")
endif()

# # Plugin tests flag
# option(PLUGIN_TESTS "Enable plugin runtime tests" OFF)

# # Qt build stuff
# set(CMAKE_PREFIX_PATH "${QTDIR}")
# set(CMAKE_INCLUDE_CURRENT_DIR ON)
# set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTOUIC ON)
# set(CMAKE_AUTORCC ON) # For resources.qrc

# # Find Qt
# find_qt(COMPONENTS Core Widgets Svg Network)

# Find nlohmann
set(JSON_BuildTests
    OFF
    CACHE INTERNAL "")
add_subdirectory(deps/json)

# Tell websocketpp not to use system boost
add_definitions(-DASIO_STANDALONE)

# Configure files
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-macros.h.in
#                ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-macros.generated.h)

# Setup target
add_library(janus-videoroom MODULE)
add_library(OBS::janus ALIAS janus-videoroom)

target_sources(
  janus-videoroom
  PRIVATE src/obs-websocket.cpp
          src/obs-websocket.h
          src/Config.cpp
          src/Config.h
          lib/obs-websocket-api.h
          src/forms/SettingsDialog.cpp
          src/forms/SettingsDialog.h
          src/forms/ConnectInfo.cpp
          src/forms/ConnectInfo.h
          src/forms/resources.qrc
          src/WebSocketApi.cpp
          src/WebSocketApi.h
          src/websocketserver/WebSocketServer.cpp
          src/websocketserver/WebSocketServer_Protocol.cpp
          src/websocketserver/WebSocketServer.h
          src/websocketserver/rpc/WebSocketSession.cpp
          src/websocketserver/rpc/WebSocketSession.h
          src/websocketserver/types/WebSocketCloseCode.h
          src/websocketserver/types/WebSocketOpCode.h
          src/eventhandler/EventHandler.cpp
          src/eventhandler/EventHandler_General.cpp
          src/eventhandler/EventHandler_Config.cpp
          src/eventhandler/EventHandler_Scenes.cpp
          src/eventhandler/EventHandler_Inputs.cpp
          src/eventhandler/EventHandler_Transitions.cpp
          src/eventhandler/EventHandler_Filters.cpp
          src/eventhandler/EventHandler_Outputs.cpp
          src/eventhandler/EventHandler_SceneItems.cpp
          src/eventhandler/EventHandler_MediaInputs.cpp
          src/eventhandler/EventHandler_Ui.cpp
          src/eventhandler/EventHandler.h
          src/eventhandler/types/EventSubscription.h
          src/requesthandler/RequestHandler.cpp
          src/requesthandler/RequestHandler_General.cpp
          src/requesthandler/RequestHandler_Config.cpp
          src/requesthandler/RequestHandler_Sources.cpp
          src/requesthandler/RequestHandler_Scenes.cpp
          src/requesthandler/RequestHandler_Inputs.cpp
          src/requesthandler/RequestHandler_Transitions.cpp
          src/requesthandler/RequestHandler_Filters.cpp
          src/requesthandler/RequestHandler_SceneItems.cpp
          src/requesthandler/RequestHandler_Outputs.cpp
          src/requesthandler/RequestHandler_Stream.cpp
          src/requesthandler/RequestHandler_Record.cpp
          src/requesthandler/RequestHandler_MediaInputs.cpp
          src/requesthandler/RequestHandler_Ui.cpp
          src/requesthandler/RequestHandler.h
          src/requesthandler/RequestBatchHandler.cpp
          src/requesthandler/RequestBatchHandler.h
          src/requesthandler/rpc/Request.cpp
          src/requesthandler/rpc/Request.h
          src/requesthandler/rpc/RequestBatchRequest.cpp
          src/requesthandler/rpc/RequestBatchRequest.h
          src/requesthandler/rpc/RequestResult.cpp
          src/requesthandler/rpc/RequestResult.h
          src/requesthandler/types/RequestStatus.h
          src/requesthandler/types/RequestBatchExecutionType.h
          src/utils/Crypto.cpp
          src/utils/Crypto.h
          src/utils/Json.cpp
          src/utils/Json.h
          src/utils/Obs.cpp
          src/utils/Obs_StringHelper.cpp
          src/utils/Obs_NumberHelper.cpp
          src/utils/Obs_ArrayHelper.cpp
          src/utils/Obs_ObjectHelper.cpp
          src/utils/Obs_SearchHelper.cpp
          src/utils/Obs_ActionHelper.cpp
          src/utils/Obs.h
          src/utils/Obs_VolumeMeter.cpp
          src/utils/Obs_VolumeMeter.h
          src/utils/Obs_VolumeMeter_Helpers.h
          src/utils/Platform.cpp
          src/utils/Platform.h
          src/utils/Compat.cpp
          src/utils/Compat.h
          src/utils/Utils.h
          deps/qr/cpp/QrCode.cpp
          deps/qr/cpp/QrCode.hpp)

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
  target_compile_options(janus-videoroom PRIVATE /wd4267 /wd4996)
endif()

# Final CMake helpers
setup_plugin_target(janus-videoroom)
setup_target_resources(janus-videoroom "obs-plugins/janus-videoroom")
