set(PROTO_FILES
    ra2yrproto/core.proto ra2yrproto/ra2yr.proto
    ra2yrproto/commands_builtin.proto ra2yrproto/commands_game.proto
    ra2yrproto/commands_yr.proto ra2yrproto/game.proto)

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

set(BDIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SDIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT protobuf_SOURCE_DIR)
  set(protobuf_SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/protobuf")
endif()
set(PROTOBUF_SOURCES "${protobuf_SOURCE_DIR}/src")

add_custom_target(compile-protobuf ALL DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})
add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
  COMMAND
    ${PROTOC_PATH} -I=. -I="${SDIR}" -I="${PROTOBUF_SOURCES}" --pyi_out=${BDIR}
    --python_out=${BDIR} --cpp_out=${BDIR} ${PROTO_FILES}
  DEPENDS ${PROTO_FILES})

add_library(protocol_o OBJECT helpers.cpp protocol.cpp ${PROTO_SRCS}
                              ../util_string.cpp ../errors.cpp)
add_library(protocol INTERFACE protocol.cpp)
if(NOT protobuf_PROTOC_EXE)
  add_dependencies(protocol_o compile-protobuf)
endif()
target_link_libraries(protocol INTERFACE protocol_o)

# TODO: should make generated headers as install targets
