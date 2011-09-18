add_executable(fishtop
    gfx.c
    fscreen.c
    main.c
    state.c
)
target_link_libraries(fishtop core jorm ${CURSES_LIBRARIES})
