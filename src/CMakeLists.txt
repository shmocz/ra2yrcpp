include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(protocol)

set(RA2YRCPP_LINKAGE "PRIVATE")
set(RA2YRCPP_LIBRARY_NAME "ra2yrcpp")
if(MSVC)
  set(RA2YRCPP_LINKAGE "PUBLIC")
  set(RA2YRCPP_LIBRARY_NAME "libra2yrcpp")
endif()

add_library(
  yrclient STATIC
  $<TARGET_OBJECTS:protocol_o>
  auto_thread.cpp
  command/command.cpp
  command/command_manager.cpp
  commands_builtin.cpp
  commands_yr.cpp
  connection.cpp
  context.cpp
  dll_inject.cpp
  errors.cpp
  exprocess.cpp
  hook.cpp
  instrumentation_client.cpp
  instrumentation_service.cpp
  is_context.cpp
  multi_client.cpp
  network.cpp
  process.cpp
  ra2/abi.cpp
  ra2/state_parser.cpp
  ra2yrcppcli/ra2yrcppcli.cpp
  server.cpp
  websocket_server.cpp
  x86.cpp)

target_link_libraries(
  yrclient
  ${RA2YRCPP_LINKAGE}
  ${LIB_WSOCK}
  libprotobuf
  protocol
  ZLIB::ZLIB
  fmt::fmt
  psapi
  YRpp
  wsock32)

add_dependencies(yrclient protoc)
add_library(ra2yrcpp_dll SHARED yrclient_dll.cpp)
target_link_libraries(ra2yrcpp_dll PUBLIC yrclient)
set_target_properties(ra2yrcpp_dll PROPERTIES OUTPUT_NAME
                                              ${RA2YRCPP_LIBRARY_NAME})

add_executable(recordtool recordtool.cpp)
target_link_libraries(recordtool PUBLIC yrclient)

add_library(ra2yrcppcli OBJECT ./ra2yrcppcli/ra2yrcppcli.cpp)
target_link_libraries(ra2yrcppcli PUBLIC yrclient)

add_subdirectory(ra2yrcppcli)

add_custom_target(ra2yrcpp)
add_dependencies(ra2yrcpp yrclient recordtool ra2yrcppcli-bin)

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
