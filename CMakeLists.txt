cmake_minimum_required(VERSION 3.0)
project(app)

# Fix warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

option(RA2YRCPP_BUILD_MAIN_DLL "Build the main library for Windows targets" ON)
option(RA2YRCPP_BUILD_TESTS "Build tests" ON)
option(RA2YRCPP_DEBUG_LOG "Enable debug logs even for non-debug builds" OFF)

add_compile_definitions(RA2YRCPP_VERSION=${RA2YRCPP_VERSION})

if(MINGW)
  add_compile_definitions(__MINGW_FORCE_SYS_INTRINS)
  add_compile_options(-masm=intel)
endif()

if(RA2YRCPP_DEBUG_LOG)
  add_compile_definitions(DEBUG_LOG)
endif()

if(NOT RA2YRCPP_PROTOBUF_LIB)
  set(RA2YRCPP_PROTOBUF_LIB protobuf)
endif()

# FIXME: needed?
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT PROTOC_PATH)
  find_program(PROTOC_PATH protoc REQUIRED)
endif()

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

if(WIN32)
  if(MINGW)
    find_library(LIB_WSOCK32 wsock32 REQUIRED)
    find_library(LIB_WS2_32 ws2_32 REQUIRED)
    find_package(Threads REQUIRED)
  else()
    set(LIB_WSOCK32 wsock32)
    set(LIB_WS2_32 ws2_32)
  endif()
endif()

if(MINGW)
  # find MinGW libraries these are copied just for test executables, to avoid
  # statically linking each of them
  set(MINGW_LIBS zlib1.dll libgcc_s_dw2-1.dll libstdc++-6.dll
                 libwinpthread-1.dll)
  set(MINGW_LIBS_FULL "")
  foreach(X IN LISTS MINGW_LIBS)
    find_file(LIB_OUT ${X} PATHS ${CMAKE_SYSROOT}/lib ${CMAKE_SYSROOT}/bin
                                 NO_CACHE REQUIRED)
    list(APPEND MINGW_LIBS_FULL ${LIB_OUT})
    unset(LIB_OUT)
  endforeach()
endif()

if(NOT MINGW AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(cdecl_s "__attribute__((cdecl))")
  add_compile_definitions(__cdecl=${cdecl_s})
endif()

find_package(ZLIB REQUIRED)
# TODO: check that works for MSVC too
if(WIN32)
  add_compile_definitions(protobuf_MSVC_STATIC_RUNTIME=OFF)
endif(WIN32)
set(protobuf_WITH_ZLIB
    ON
    CACHE BOOL "" FORCE)
if(MINGW)
  add_compile_definitions(_WEBSOCKETPP_CPP11_THREAD_)
endif()
add_compile_definitions(ASIO_STANDALONE)
set(Protobuf_USE_STATIC_LIBS
    ON
    CACHE BOOL "" FORCE)

if(ZLIB_INCLUDE_DIR)
  include_directories(${ZLIB_INCLUDE_DIR})
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/protocol)
include_directories(src)
# FIXME: override protobuf include path
if(NOT RA2YRCPP_SYSTEM_PROTOBUF)
  include_directories(3rdparty/protobuf/src)
endif()
include_directories(SYSTEM 3rdparty/xbyak)
include_directories(3rdparty/argparse/include)
include_directories(SYSTEM 3rdparty/fmt/include)
include_directories(SYSTEM 3rdparty/asio/asio/include)
include_directories(SYSTEM 3rdparty/websocketpp)
add_subdirectory(3rdparty/fmt)
if(RA2YRCPP_BUILD_MAIN_DLL)
  include_directories(3rdparty/YRpp)
  add_subdirectory(3rdparty/YRpp)
  add_subdirectory(src/addscn)
  target_compile_options(YRpp INTERFACE -Wno-pragmas -Wno-return-type
                                        -Wno-inconsistent-missing-override)
endif()
add_subdirectory(src)

if(RA2YRCPP_BUILD_TESTS)
  add_subdirectory(tests)
endif()
