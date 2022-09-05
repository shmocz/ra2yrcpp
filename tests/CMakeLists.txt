# FIXME: find better way to include protocol stuff
include_directories(${CMAKE_BINARY_DIR}/src)

include_directories(../src)

function(new_make_test)
  cmake_parse_arguments(MY_FN "" "NAME" "SRC;LIB" ${ARGN})
  add_executable("${MY_FN_NAME}" ${MY_FN_SRC})
  target_link_libraries("${MY_FN_NAME}" gtest_main ${MY_FN_LIB})
  add_test(NAME ${MY_FN_NAME} COMMAND ${MY_FN_NAME})
endfunction()

function(make_test test_name)
  add_executable(${test_name} ${ARGV})
  target_link_libraries(${test_name} gtest_main)
  add_test(NAME ${test_name} COMMAND test_name)
endfunction()

add_library(test_common OBJECT common.cpp)
target_link_libraries(test_common gtest_main)

new_make_test(
  NAME test_process
  SRC test_process.cpp ../src/utility/scope_guard.hpp
  LIB process)

new_make_test(
  NAME test_hooks
  SRC test_hooks.cpp $<TARGET_OBJECTS:errors> ../src/util_string.cpp
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

new_make_test(
  NAME test_integration_basic_communication
  SRC test_integration_basic_communication.cpp
  LIB yrclient)
