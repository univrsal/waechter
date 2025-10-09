add_library(util STATIC
    NetworkInterface.cpp
    SignalHandler.cpp
        Filesystem.hpp
        ErrnoUtil.hpp
)

target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(util PRIVATE -Wall -Wextra -Wpedantic)

add_library(waechter::util ALIAS util)