add_library(util STATIC
        SignalHandler.cpp
        Filesystem.hpp
        ErrnoUtil.hpp
        Types.hpp
        Json.hpp
        Format.hpp
        Time.cpp
)

add_library(waechter::util ALIAS util)

if (WAECHTER_BUILD_DAEMON)
	target_sources(util PRIVATE
		NetworkInterface.cpp
	)
endif()

if(WIN32)
	target_compile_definitions(util PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
	target_sources(util PRIVATE
		ClientSocket.cpp
		Socket.hpp
    )
endif()

target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(util PRIVATE
        thirdparty::spdlog
        thirdparty::cereal
        thirdparty::sigslot
        thirdparty::deps_includes
)

if (UNIX AND NOT APPLE)
    # Only warnings for linux right now
    target_compile_options(util PRIVATE
            -Wall -Wextra
            -Wshadow -Wformat=2 -Wconversion -Wsign-conversion
            -Wnull-dereference -Wdouble-promotion -Wcast-align
            -Wduplicated-cond -Wredundant-decls -Wpointer-arith
            -Werror -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
            -Wnon-virtual-dtor -pedantic -Wmisleading-indentation -Wduplicated-branches -Wlogical-op -Wformat=2
            -Wuseless-cast -Wimplicit-fallthrough
    )
endif ()

add_subdirectory(Data)
add_subdirectory(IP2Asn)
