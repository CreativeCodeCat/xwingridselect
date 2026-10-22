find_package (PkgConfig)

IF(UNIX)
    pkg_check_modules (DEPS REQUIRED gtk4 x11 xext)
ENDIF(UNIX)

IF(WIN32)
    pkg_check_modules (DEPS REQUIRED gtk4)
ENDIF(WIN32)

set (CFLAGS ${DEPS_CFLAGS}
    ${DEPS_CFLAGS_OTHER})
set (LIB_PATHS ${DEPS_LIBRARY_DIRS})
link_directories (${LIB_PATHS})
add_definitions (${CFLAGS})

IF(UNIX)
    add_executable (xwingridselect x_interaction.c gridselect_box.c gridselect_window_box.c gridselect_search_box.c main.c)
ENDIF(UNIX)

IF(WIN32)
    add_executable (xwingridselect win32_interaction.c gridselect_box.c gridselect_window_box.c gridselect_search_box.c main.c)
ENDIF(WIN32)

target_link_libraries (xwingridselect ${DEPS_LIBRARIES})

install (TARGETS xwingridselect
    RUNTIME DESTINATION bin)
