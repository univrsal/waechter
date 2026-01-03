add_library(util STATIC
        NetworkInterface.cpp
        SignalHandler.cpp
        Filesystem.hpp
        ErrnoUtil.hpp
        Types.hpp
        Socket.hpp
        ClientSocket.cpp
        Json.hpp
        Format.hpp
        Time.cpp
)
add_library(waechter::util ALIAS util)

target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(util PRIVATE
        thirdparty::spdlog
        thirdparty::cereal
        thirdparty::sigslot
        thirdparty::deps_includes
)

target_compile_options(util PRIVATE
        -Wall -Wextra
        -Wshadow -Wformat=2 -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wcast-align
        -Wduplicated-cond -Wredundant-decls -Wpointer-arith
        -Werror -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
        -Wnon-virtual-dtor -pedantic -Wmisleading-indentation -Wduplicated-branches -Wlogical-op -Wformat=2
        -Wuseless-cast -Wimplicit-fallthrough
)

add_subdirectory(Data)
