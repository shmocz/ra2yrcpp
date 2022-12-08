set(PROTO_FILES
    ra2yrproto/core.proto ra2yrproto/ra2yr.proto
    ra2yrproto/commands_builtin.proto ra2yrproto/commands_yr.proto
    ra2yrproto/game.proto)

# set protobuf file paths
set(PROTO_HDRS "")
set(PROTO_SRCS "")
foreach(X IN LISTS PROTO_FILES)
  string(REPLACE ".proto" ".pb.h" FN_H ${X})
  string(REPLACE ".proto" ".pb.cc" FN_S ${X})
  list(APPEND PROTO_HDRS ${FN_H})
  list(APPEND PROTO_SRCS ${FN_S})
  unset(FN_H)
  unset(FN_S)
endforeach()

add_custom_target(compile-protobuf ALL DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})
add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} -I${CMAKE_CURRENT_SOURCE_DIR}
    --pyi_out=${CMAKE_CURRENT_BINARY_DIR}
    --python_out=${CMAKE_CURRENT_BINARY_DIR}
    --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${PROTO_FILES}
  DEPENDS ${PROTO_FILES})

add_library(protocol_o OBJECT protocol.cpp ${PROTO_SRCS} ${PROTO_HDRS}
                              ../util_string.cpp)
add_library(protocol INTERFACE protocol.cpp)
target_link_libraries(protocol INTERFACE protocol_o)
