add_library(libapp STATIC app.cpp)
add_library(winutils STATIC winutils.cpp)
add_executable(app main.cpp hook.cpp)
target_link_libraries(app PUBLIC libapp)