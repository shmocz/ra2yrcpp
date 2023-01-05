include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(protocol)

add_library(errors OBJECT errors.cpp)
add_library(process STATIC process.cpp exprocess.cpp $<TARGET_OBJECTS:errors>)
target_link_libraries(process PUBLIC psapi fmt::fmt)
add_library(network STATIC network.cpp $<TARGET_OBJECTS:errors>)

target_link_libraries(network ${LIB_WSOCK})
add_library(dll_inject OBJECT dll_inject.cpp)
target_link_libraries(dll_inject process)

add_library(connection OBJECT connection.cpp)
target_link_libraries(connection PUBLIC network)
add_library(
  ra2objects OBJECT ra2/game_state.cpp ra2/state_parser.cpp ra2/utility.cpp
                    ra2/type_classes.cpp ra2/objects.cpp ra2/abi.cpp)
target_link_libraries(ra2objects PUBLIC process errors fmt::fmt)
add_library(instrumentation_service OBJECT instrumentation_service.cpp
                                           instrumentation_client.cpp)
target_link_libraries(instrumentation_service PUBLIC protocol)

add_library(commands OBJECT commands_builtin.cpp commands_yr.cpp)
target_link_libraries(commands PUBLIC instrumentation_service protocol)
add_library(instrumentation_client STATIC instrumentation_client.cpp
                                          multi_client.cpp)
target_link_libraries(instrumentation_client PUBLIC protocol errors connection
                                                    fmt::fmt)
add_library(is_instance INTERFACE is_context.cpp manager.cpp commands_yr.cpp
            commands_builtin.cpp)
target_link_libraries(is_instance INTERFACE instrumentation_service)

add_library(ra2yrcppcli OBJECT ./ra2yrcppcli/ra2yrcppcli.cpp)
target_link_libraries(
  ra2yrcppcli
  PUBLIC $<TARGET_OBJECTS:protocol_o>
         fmt::fmt
         $<TARGET_OBJECTS:errors>
         connection
         process
         dll_inject
         network
         is_instance)

add_library(
  yrclient SHARED
  yrclient_dll.cpp
  is_context.cpp
  multi_client.cpp
  context.cpp
  process.cpp
  exprocess.cpp
  dll_inject.cpp
  $<TARGET_OBJECTS:errors>
  $<TARGET_OBJECTS:protocol_o>
  $<TARGET_OBJECTS:commands>
  $<TARGET_OBJECTS:instrumentation_service>
  $<TARGET_OBJECTS:ra2objects>
  network.cpp
  x86.cpp
  server.cpp
  hook.cpp
  command/command_manager.cpp
  command/command.cpp
  connection.cpp
  manager.cpp
  ra2yrcppcli/ra2yrcppcli.cpp)
target_link_libraries(yrclient PRIVATE ${Protobuf_LIBRARIES} ZLIB::ZLIB
                                       fmt::fmt ${LIB_WSOCK} psapi)

add_executable(recordtool recordtool.cpp)
target_link_libraries(recordtool PUBLIC yrclient)

add_subdirectory(ra2yrcppcli)

add_custom_target(ra2yrcpp)
add_dependencies(ra2yrcpp yrclient recordtool ra2yrcppcli-bin)

add_library(manager STATIC manager.cpp)
target_link_libraries(manager PUBLIC instrumentation_client)

list(TRANSFORM MINGW_LIBS PREPEND "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
                                  OUTPUT_VARIABLE MINGW_LIBS_TEST)

add_custom_target(copy-mingw-libraries ALL DEPENDS ${MINGW_LIBS_TEST})
add_custom_command(
  OUTPUT ${MINGW_LIBS_TEST}
  COMMAND ${CMAKE_COMMAND} -E copy ${MINGW_LIBS_FULL}
          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  DEPENDS ${MINGW_LIBS_FULL})

install(TARGETS yrclient RUNTIME)
install(FILES ${MINGW_LIBS_TEST} TYPE BIN)
