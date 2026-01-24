add_library(xtray STATIC
    tray.c
)

add_library(thirdparty::xtray ALIAS xtray)

target_link_libraries(xtray PRIVATE
        X11
        m
)

target_include_directories(xtray PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)