#set_target_properties(test_command_manager test_instrumentation_service PROPERTIES LINK_SEARCH_START_STATIC ON LINK_SEARCH_END_STATIC ON)

# Now simply link against gtest or gtest_main as needed. Eg
include_directories(../src)
function(make_test test_name)
    add_executable(${test_name} ${ARGV})
    target_Link_libraries(${test_name} gtest_main)
    add_test(NAME ${test_name} COMMAND test_name)
endfunction()
make_test(test_hooks test_hooks.cpp ../src/hook.cpp ../src/process.cpp ../src/errors.cpp)
make_test(test_process test_process.cpp ../src/process.cpp ../src/errors.cpp ../src/utility/scope_guard.hpp)