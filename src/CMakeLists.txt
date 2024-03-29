include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(protocol)

set(RA2YRCPP_LINKAGE "PRIVATE")
set(RA2YRCPP_LIBRARY_NAME "ra2yrcpp")
if(MSVC)
  set(RA2YRCPP_LINKAGE "PUBLIC")
  set(RA2YRCPP_LIBRARY_NAME "libra2yrcpp")
endif()

add_library(
  ra2yrcpp_core STATIC
  $<TARGET_OBJECTS:protocol_o>
  asio_utils.cpp
  auto_thread.cpp
  client_connection.cpp
  command/command_manager.cpp
  command/is_command.cpp
  commands_builtin.cpp
  errors.cpp
  hook.cpp
  instrumentation_client.cpp
  instrumentation_service.cpp
  multi_client.cpp
  process.cpp
  utility/sync.cpp
  websocket_connection.cpp
  websocket_server.cpp
  x86.cpp)

if(NOT PROTO_LIB)
  find_library(PROTO_LIB protobuf libprotobuf REQUIRED)
endif()
target_link_libraries(ra2yrcpp_core PUBLIC fmt::fmt "${PROTO_LIB}" protocol)
if(WIN32)
  add_library(windows_utils STATIC win32/win_message.cpp
                                   win32/windows_utils.cpp)
  target_link_libraries(ra2yrcpp_core PUBLIC windows_utils ${LIB_WSOCK32}
                                             ${LIB_WS2_32})
endif()

target_compile_options(ra2yrcpp_core PUBLIC ${RA2YRCPP_EXTRA_FLAGS})
target_link_options(ra2yrcpp_core PUBLIC ${RA2YRCPP_EXTRA_FLAGS})

if(RA2YRCPP_BUILD_MAIN_DLL)
  add_library(
    yrclient STATIC
    commands_game.cpp
    commands_yr.cpp
    context.cpp
    dll_inject.cpp
    hooks_yr.cpp
    is_context.cpp
    ra2/abi.cpp
    ra2/common.cpp
    ra2/event_list.cpp
    ra2/state_context.cpp
    ra2/state_parser.cpp
    ra2/yrpp_export.cpp
    ra2yrcppcli/ra2yrcppcli.cpp)

  target_link_libraries(
    yrclient
    ${RA2YRCPP_LINKAGE}
    "${PROTO_LIB}"
    ra2yrcpp_core
    protocol
    ZLIB::ZLIB
    fmt::fmt
    YRpp)

  target_link_libraries(yrclient PUBLIC windows_utils)

  if(WIN32)
    target_link_libraries(yrclient PUBLIC ${LIB_WSOCK32} ${LIB_WS2_32})
  endif()

  add_library(ra2yrcpp_dll SHARED yrclient_dll.cpp)
  target_sources(ra2yrcpp_dll PRIVATE version.rc)

  if(MINGW)
    target_link_libraries(
      ra2yrcpp_dll
      PUBLIC yrclient -static-libgcc -static-libstdc++
             -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive)
  else()
    target_link_libraries(ra2yrcpp_dll PUBLIC yrclient)
  endif()

  set_target_properties(ra2yrcpp_dll PROPERTIES OUTPUT_NAME
                                                ${RA2YRCPP_LIBRARY_NAME})

  add_custom_target(ra2yrcpp)
  add_dependencies(ra2yrcpp yrclient)
  if(RA2YRCPP_BUILD_CLI_TOOL)
    add_library(ra2yrcppcli OBJECT ./ra2yrcppcli/ra2yrcppcli.cpp)
    target_link_libraries(ra2yrcppcli PUBLIC yrclient)

    add_subdirectory(ra2yrcppcli)
    add_dependencies(ra2yrcpp ra2yrcppcli)
  endif()

  if(MINGW)
    list(TRANSFORM MINGW_LIBS PREPEND "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
                                      OUTPUT_VARIABLE MINGW_LIBS_TEST)

    add_custom_target(copy-mingw-libraries ALL DEPENDS ${MINGW_LIBS_TEST})
    add_custom_command(
      OUTPUT ${MINGW_LIBS_TEST}
      COMMAND ${CMAKE_COMMAND} -E copy ${MINGW_LIBS_FULL}
              ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
      DEPENDS ${MINGW_LIBS_FULL})
  endif()

  install(TARGETS ra2yrcpp_dll RUNTIME)
  if(MINGW)
    install(FILES ${MINGW_LIBS_TEST} TYPE BIN)
  endif()
endif()
