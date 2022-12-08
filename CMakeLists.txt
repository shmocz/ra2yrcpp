# CMakeLists files in this project can refer to the root source directory of the
# project as ${HELLO_SOURCE_DIR} and to the root binary directory of the project
# as ${HELLO_BINARY_DIR}.
cmake_minimum_required(VERSION 3.0)
project(app)

# Fix warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra)
endif()
add_compile_definitions(__MINGW_FORCE_SYS_INTRINS)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_library(LIB_WSOCK ws2_32)

# find MinGW libraries
set(MINGW_LIBS libgcc_s_dw2-1.dll libstdc++-6.dll libwinpthread-1.dll zlib1.dll)
set(MINGW_LIBS_FULL "")
foreach(X IN LISTS MINGW_LIBS)
  find_file(LIB_OUT ${X} PATHS ${CMAKE_SYSROOT}/lib NO_CACHE REQUIRED)
  list(APPEND MINGW_LIBS_FULL ${LIB_OUT})
  unset(LIB_OUT)
endforeach()

find_package(ZLIB REQUIRED)
add_compile_definitions(protobuf_MSVC_STATIC_RUNTIME=OFF)
# Find protobuf and generate sources
add_compile_definitions(protobuf_WITH_ZLIB=ON)
set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf REQUIRED)
set(Protobuf_PROTOC_EXECUTABLE ${PROTOBUF_PATH})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/protocol
                    ${Protobuf_INCLUDE_DIRS})
include_directories(src)
include_directories(3rdparty/xbyak)
include_directories(3rdparty/argparse/include)
include_directories(3rdparty/fmt/include)
add_subdirectory(3rdparty/fmt)
add_subdirectory(src)
add_subdirectory(tests)
