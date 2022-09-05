add_dependencies(ra2yrcppcli yrclient)

add_executable(ra2yrcppcli-bin main.cpp)
target_link_libraries(ra2yrcppcli-bin PUBLIC ra2yrcppcli yrclient)
set_target_properties(ra2yrcppcli-bin PROPERTIES OUTPUT_NAME ra2yrcppcli)
