# CMakeLists files in this project can
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and
# to the root binary directory of the project as ${HELLO_BINARY_DIR}.
cmake_minimum_required(VERSION 3.0)
project(app)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra)
endif()
add_compile_definitions(__MINGW_FORCE_SYS_INTRINS)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_library(LIB_WSOCK ws2_32)

# Find protobuf and generate sources
set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf REQUIRED)
set(Protobuf_PROTOC_EXECUTABLE ${PROTOBUF_PATH})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_compile_definitions(protobuf_MSVC_STATIC_RUNTIME=ON)
include_directories(src)
include_directories(3rdparty/xbyak)
add_subdirectory(src)
add_subdirectory(tests)

#add_compile_definitions(app __MINGW_FORCE_SYS_INTRINS)

# Assembly targets
function(add_asm_target target_name source)
    if (DEFINED ARGV2)
        set(args_extra ${ARGV2})
    else()
        set(args_extra "")
    endif()

    set(src ${CMAKE_CURRENT_SOURCE_DIR}/${source})
    add_custom_target(${target_name}
    COMMAND nasm ${args_extra} ${src} -o ${target_name}.obj
    DEPENDS ${src})
endfunction()
