include_directories(${CMAKE_CURRENT_BINARY_DIR})
set(PROTO_FILES protocol/core.proto protocol/ra2yr.proto
                protocol/commands_builtin.proto protocol/commands_yr.proto)
protobuf_generate_python(PROTO_PY ${PROTO_FILES})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
add_library(errors OBJECT errors.cpp)
add_library(process STATIC process.cpp exprocess.cpp $<TARGET_OBJECTS:errors>)
target_link_libraries(process PUBLIC psapi)
add_library(network STATIC network.cpp $<TARGET_OBJECTS:errors>)
add_library(hooks STATIC hook.cpp x86.cpp)
add_library(command_manager STATIC command/command_manager.cpp
                                   command/command.cpp errors.cpp)
target_link_libraries(network ${LIB_WSOCK})
add_library(dll_inject STATIC dll_inject.cpp)
target_link_libraries(dll_inject process)
add_library(protocol_o OBJECT protocol/protocol.cpp ${PROTO_SRCS} ${PROTO_HDRS}
                              util_string.cpp)
add_library(protocol STATIC protocol/protocol.cpp)
target_link_libraries(protocol_o ${Protobuf_LIBRARIES})
target_link_libraries(protocol PUBLIC protocol_o)
target_include_directories(protocol PUBLIC ${Protobuf_INCLUDE_DIRS})
add_custom_target(protocol_py ALL DEPENDS ${PROTO_PY})
add_library(proto_manager SHARED proto_manager.cpp)
target_link_libraries(proto_manager PUBLIC protocol)
add_library(connection STATIC connection.cpp)
target_link_libraries(connection PUBLIC network)
add_library(
  instrumentation_service STATIC
  instrumentation_service.cpp
  instrumentation_client.cpp
  is_context.cpp
  server.cpp
  context.cpp
  commands_builtin.cpp
  commands_yr.cpp
  multi_client.cpp
  util_string.cpp)
target_link_libraries(instrumentation_service
                      PUBLIC protocol connection process command_manager hooks)
add_library(yrclient MODULE yrclient_dll.cpp)
target_link_libraries(yrclient PUBLIC instrumentation_service)
add_executable(dllitool dllitool.cpp)
target_link_libraries(dllitool PUBLIC instrumentation_service dll_inject)
