# FIXME: find better way to include protocol stuff

include_directories(../src)

function(new_make_test)
  cmake_parse_arguments(MY_FN "" "NAME" "SRC;LIB" ${ARGN})
  add_executable("${MY_FN_NAME}" ${MY_FN_SRC})
  target_link_libraries("${MY_FN_NAME}" PUBLIC gtest_main ${MY_FN_LIB})
  # FIXME: this is just ugly hack to ensure protoc is present
  add_dependencies("${MY_FN_NAME}" protoc)
  add_test(NAME ${MY_FN_NAME} COMMAND ${MY_FN_NAME})
  install(TARGETS "${MY_FN_NAME}" RUNTIME)
endfunction()

function(make_test test_name)
  add_executable(${test_name} ${ARGV})
  target_link_libraries(${test_name} gtest_main)
  add_test(NAME ${test_name} COMMAND test_name)
endfunction()

add_library(test_common OBJECT common.cpp)
target_link_libraries(test_common gtest_main yrclient)

new_make_test(
  NAME test_process
  SRC test_process.cpp ../src/utility/scope_guard.hpp
  LIB yrclient)

new_make_test(
  NAME test_hooks
  SRC test_hooks.cpp ../src/util_string.cpp
  LIB yrclient fmt::fmt)

new_make_test(
  NAME test_server
  SRC test_server.cpp
  LIB yrclient)

new_make_test(
  NAME test_instrumentation_service
  SRC test_instrumentation_service.cpp
  LIB yrclient test_common)

new_make_test(
  NAME test_instrumentation_service_daemon
  SRC test_instrumentation_service_daemon.cpp
  LIB yrclient)

add_executable(dummy_program dummy_program.cpp)
install(TARGETS dummy_program RUNTIME)

new_make_test(
  NAME test_dll_inject
  SRC test_dll_inject.cpp
  LIB yrclient fmt::fmt)

new_make_test(
  NAME test_new_commands
  SRC test_new_commands.cpp
  LIB yrclient test_common)

new_make_test(
  NAME test_command_manager
  SRC test_command_manager.cpp
  LIB yrclient)

new_make_test(
  NAME test_multi_client
  SRC test_multi_client.cpp
  LIB yrclient)
