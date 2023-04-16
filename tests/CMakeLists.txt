# FIXME: find better way to include protocol stuff

include_directories(../src)

set(YRCLIENT_LIB yrclient)
# MinGW exports everything automatically, so we can link to DLL for better
# performance
if(MINGW)
  message("LINKAGE SHARED")
  set(YRCLIENT_LIB ra2yrcpp_dll)
endif()

set(NATIVE_TARGETS "")

function(new_make_test)
  cmake_parse_arguments(MY_FN "" "NAME" "SRC;LIB" ${ARGN})
  add_executable("${MY_FN_NAME}" ${MY_FN_SRC})
  target_link_libraries("${MY_FN_NAME}" PUBLIC gtest_main ${MY_FN_LIB})
  # FIXME: this is just ugly hack to ensure protoc is present
  add_dependencies("${MY_FN_NAME}" protoc)
  add_test(NAME ${MY_FN_NAME} COMMAND ${MY_FN_NAME})
  install(TARGETS "${MY_FN_NAME}" RUNTIME)
  list(APPEND NATIVE_TARGETS "${MY_FN_NAME}")
endfunction()

if(RA2YRCPP_BUILD_MAIN_DLL)
  new_make_test(
    NAME test_process
    SRC test_process.cpp ../src/utility/scope_guard.hpp
    LIB ${YRCLIENT_LIB})

  new_make_test(
    NAME test_hooks
    SRC test_hooks.cpp ../src/util_string.cpp
    LIB ${YRCLIENT_LIB} fmt::fmt)

  add_executable(dummy_program dummy_program.cpp)
  install(TARGETS dummy_program RUNTIME)

  new_make_test(
    NAME test_dll_inject
    SRC test_dll_inject.cpp
    LIB ${YRCLIENT_LIB} fmt::fmt)
endif()

add_library(common_multi STATIC common_multi.cpp)
target_link_libraries(common_multi PUBLIC ra2yrcpp_core)

new_make_test(
  NAME test_instrumentation_service
  SRC test_instrumentation_service.cpp
  LIB ra2yrcpp_core)

new_make_test(
  NAME test_server
  SRC test_server.cpp
  LIB ra2yrcpp_core)
new_make_test(
  NAME test_multi_client
  SRC test_multi_client.cpp
  LIB common_multi)

new_make_test(
  NAME test_is_stress_test
  SRC test_is_stress_test.cpp
  LIB common_multi)

new_make_test(
  NAME test_command_manager
  SRC test_command_manager.cpp
  LIB common_multi)

add_library(tests_native INTERFACE ${NATIVE_TARGETS})

target_compile_options(tests_native INTERFACE ${RA2YRCPP_EXTRA_FLAGS})
target_link_options(tests_native INTERFACE ${RA2YRCPP_EXTRA_FLAGS})
