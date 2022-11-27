add_dependencies(ra2yrcppcli yrclient)

add_executable(ra2yrcppcli-bin main.cpp)
target_link_libraries(ra2yrcppcli-bin PRIVATE yrclient)
set_target_properties(ra2yrcppcli-bin PROPERTIES OUTPUT_NAME ra2yrcppcli)

install(TARGETS ra2yrcppcli-bin RUNTIME)
