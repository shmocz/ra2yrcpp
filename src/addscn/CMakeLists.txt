project(addscn)

add_executable(addscn addscn.cpp)
target_link_libraries(addscn -static)
install(TARGETS addscn RUNTIME)
