add_library(util STATIC
        NetworkInterface.cpp
        SignalHandler.cpp
        Filesystem.hpp
        ErrnoUtil.hpp
        Types.hpp
        Socket.hpp
        ClientSocket.cpp
        Json.hpp
)

target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(util PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wformat=2 -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wcast-align
        -Wduplicated-cond -Wredundant-decls -Wpointer-arith
        -Werror
)

add_library(waechter::util ALIAS util)