cmake_minimum_required(VERSION 3.0)
project(app)

# Fix warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

if(MSVC)

elseif(MINGW)
  add_compile_definitions(__MINGW_FORCE_SYS_INTRINS)
  add_compile_options(-masm=intel)
else()
  message(FATAL_ERROR "No compiler found")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# FIXME: conditional if native protoc present
set(PROTOC_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/protoc.exe)

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

if(MINGW)
  find_library(LIB_WSOCK ws2_32)
else()
  set(LIB_WSOCK ws2_32)
endif()

if(MINGW)
  # find MinGW libraries
  set(MINGW_LIBS libgcc_s_dw2-1.dll libstdc++-6.dll libwinpthread-1.dll
                 zlib1.dll)
  set(MINGW_LIBS_FULL "")
  foreach(X IN LISTS MINGW_LIBS)
    find_file(LIB_OUT ${X} PATHS ${CMAKE_SYSROOT}/lib ${CMAKE_SYSROOT}/bin
                                 NO_CACHE REQUIRED)
    list(APPEND MINGW_LIBS_FULL ${LIB_OUT})
    unset(LIB_OUT)
  endforeach()
endif()

find_package(ZLIB REQUIRED)
add_compile_definitions(protobuf_MSVC_STATIC_RUNTIME=OFF)
# Find protobuf and generate sources
add_compile_definitions(protobuf_WITH_ZLIB=ON)
if(MINGW)
  add_compile_definitions(_WEBSOCKETPP_CPP11_THREAD_)
endif()
add_compile_definitions(ASIO_STANDALONE)
set(Protobuf_USE_STATIC_LIBS ON)
if(ZLIB_INCLUDE_DIR)
  include_directories(${ZLIB_INCLUDE_DIR})
endif()
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/protocol)
include_directories(src)
set(protobuf_BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(protobuf_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(protobuf_MODULE_COMPATIBLE
    ON
    CACHE BOOL "" FORCE)
set(protobuf_INSTALL
    ON
    CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/protobuf ${CMAKE_CURRENT_BINARY_DIR}/bin)
include_directories(3rdparty/protobuf/src)
include_directories(3rdparty/xbyak)
include_directories(3rdparty/argparse/include)
include_directories(3rdparty/fmt/include)
include_directories(3rdparty/asio/asio/include)
include_directories(3rdparty/websocketpp)
include_directories(3rdparty/YRpp SYSTEM)
add_subdirectory(3rdparty/fmt)
add_subdirectory(3rdparty/YRpp SYSTEM)
add_subdirectory(src)
add_subdirectory(tests)
