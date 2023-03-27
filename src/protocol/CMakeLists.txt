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

set(BDIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SDIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(PROTOBUF_SOURCES "${protobuf_SOURCE_DIR}/src")

# Convert to Windows path for protoc.exe if cross-compiling
if(CMAKE_CROSSCOMPILING)
  string(REPLACE "/" "\\\\" BDIR "${BDIR}")
  string(REPLACE "/" "\\\\" SDIR "${SDIR}")
  string(REPLACE "/" "\\\\" PROTOBUF_SOURCES "${PROTOBUF_SOURCES}")
  string(PREPEND BDIR "Z:")
  string(PREPEND SDIR "Z:")
  string(PREPEND PROTOBUF_SOURCES "Z:")
endif()

add_custom_target(compile-protobuf ALL DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})
add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
  COMMAND
    ${PROTOC_PATH} -I=. -I="${SDIR}" -I="${PROTOBUF_SOURCES}" --pyi_out=${BDIR}
    --python_out=${BDIR} --cpp_out=${BDIR} ${PROTO_FILES}
  DEPENDS ${PROTO_FILES})
add_dependencies(compile-protobuf protoc)

add_library(protocol_o OBJECT protocol.cpp ${PROTO_SRCS} ../util_string.cpp)
add_library(protocol INTERFACE protocol.cpp)
add_dependencies(protocol_o compile-protobuf)
target_link_libraries(protocol INTERFACE protocol_o)

# TODO: should make generated headers as intall targets
