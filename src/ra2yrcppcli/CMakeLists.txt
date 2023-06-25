add_dependencies(ra2yrcppcli yrclient)

add_executable(ra2yrcppcli-bin main.cpp)
set(LINK_LIBS "")
if(MINGW)
  set(LINK_LIBS
      "-static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive"
  )
endif()
target_link_libraries(ra2yrcppcli-bin PUBLIC yrclient "${LINK_LIBS}")
set_target_properties(ra2yrcppcli-bin PROPERTIES OUTPUT_NAME ra2yrcppcli)

install(TARGETS ra2yrcppcli-bin RUNTIME)
