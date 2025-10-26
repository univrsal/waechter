add_library(incbin
        STATIC
        incbin.c
        incbin.h
)

add_library(thirdparty::incbin ALIAS incbin)

target_include_directories(incbin
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)