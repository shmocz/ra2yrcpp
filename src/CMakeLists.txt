include_directories(${CMAKE_CURRENT_BINARY_DIR})
set(PROTO_FILES
    protocol/core.proto protocol/ra2yr.proto protocol/commands_builtin.proto
    protocol/commands_yr.proto protocol/game.proto)
protobuf_generate_python(PROTO_PY ${PROTO_FILES})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(errors OBJECT errors.cpp)
add_library(process STATIC process.cpp exprocess.cpp $<TARGET_OBJECTS:errors>)
target_link_libraries(process PUBLIC psapi fmt::fmt)
add_library(network STATIC network.cpp $<TARGET_OBJECTS:errors>)
add_library(hooks STATIC hook.cpp x86.cpp)
add_library(command_manager STATIC command/command_manager.cpp
                                   command/command.cpp errors.cpp)
target_link_libraries(command_manager PUBLIC fmt::fmt)
target_link_libraries(network ${LIB_WSOCK})
add_library(dll_inject STATIC dll_inject.cpp)
target_link_libraries(dll_inject process)
add_library(protocol_o OBJECT protocol/protocol.cpp ${PROTO_SRCS} ${PROTO_HDRS}
                              util_string.cpp)
add_library(protocol STATIC protocol/protocol.cpp)
target_link_libraries(protocol_o ${Protobuf_LIBRARIES} ZLIB::ZLIB fmt::fmt)
target_link_libraries(protocol PUBLIC protocol_o)
target_include_directories(protocol PUBLIC ${Protobuf_INCLUDE_DIRS})
add_custom_target(protocol_py ALL DEPENDS ${PROTO_PY})
add_library(connection STATIC connection.cpp)
target_link_libraries(connection PUBLIC network)
add_library(
  ra2objects STATIC ra2/game_state.cpp ra2/state_parser.cpp ra2/utility.cpp
                    ra2/type_classes.cpp ra2/objects.cpp)
target_link_libraries(ra2objects PUBLIC process errors fmt::fmt)
add_library(
  instrumentation_service STATIC
  instrumentation_service.cpp
  instrumentation_client.cpp
  server.cpp
  context.cpp
  multi_client.cpp
  util_string.cpp
  logging.hpp)
target_link_libraries(
  instrumentation_service
  PUBLIC protocol
         connection
         process
         command_manager
         hooks
         ra2objects
         fmt::fmt)
add_library(commands STATIC commands_builtin.cpp commands_yr.cpp)
target_link_libraries(commands PUBLIC instrumentation_service)
add_library(instrumentation_client STATIC instrumentation_client.cpp
                                          multi_client.cpp)
target_link_libraries(instrumentation_client PUBLIC protocol errors connection
                                                    fmt::fmt)
add_library(is_instance STATIC is_context.cpp manager.cpp)
target_link_libraries(is_instance PUBLIC commands instrumentation_service)
add_library(yrclient MODULE yrclient_dll.cpp)
target_link_libraries(yrclient PUBLIC is_instance commands)
add_executable(recordtool recordtool.cpp)
target_link_libraries(recordtool PUBLIC protocol fmt::fmt)

add_subdirectory(ra2yrcppcli)

add_custom_target(ra2yrcpp)
add_dependencies(ra2yrcpp yrclient recordtool ra2yrcppcli)

add_library(manager STATIC manager.cpp)
target_link_libraries(manager PUBLIC instrumentation_client)
