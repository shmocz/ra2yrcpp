add_library(ra2yrcppcli ra2yrcppcli.cpp ../multi_client.cpp
                        ../instrumentation_client.cpp)
target_link_libraries(
  ra2yrcppcli
  PUBLIC protocol_o
         fmt::fmt
         errors
         connection
         process
         dll_inject
         network
         is_instance)
add_dependencies(ra2yrcppcli yrclient)

add_executable(ra2yrcppcli-bin main.cpp)
target_link_libraries(ra2yrcppcli-bin PUBLIC ra2yrcppcli)
set_target_properties(ra2yrcppcli-bin PROPERTIES OUTPUT_NAME ra2yrcppcli)
