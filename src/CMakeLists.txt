
set(3DCLCSCRN_BIN	3dclock_screensaver.c)

add_executable(3dclock_screensaver ${3DCLCSCRN_BIN})
set_target_properties(3dclock_screensaver PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(3dclock_screensaver ${CMAKE_REQUIRED_LIBRARIES} ${CMAKE_EXE_LINKER_FLAGS})

install(TARGETS 3dclock_screensaver RUNTIME DESTINATION bin)
