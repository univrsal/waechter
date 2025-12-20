add_executable(waechterd
        Waechterd.cpp
        DaemonConfig.cpp
        DaemonConfig.hpp
        Daemon.cpp
        Daemon.hpp
)


target_compile_options(waechterd PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wformat=2 -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wcast-align
        -Wduplicated-cond -Wredundant-decls -Wpointer-arith
        -Werror
)

include(FindPkgConfig)
pkg_check_modules(LIBBPF REQUIRED libbpf)


target_include_directories(waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(waechterd PRIVATE ${LIBBPF_LIBRARIES}
        waechter::util thirdparty::spdlog thirdparty::inih thirdparty::json11 thirdparty::cereal thirdparty::deps_includes
        ebpf_includes
)
add_dependencies(waechterd bpfobj)

add_subdirectory(Net)
add_subdirectory(Data)
add_subdirectory(Communication)
add_subdirectory(Rules)
add_subdirectory(EBPF)

target_compile_definitions(waechterd PRIVATE
        $<$<CONFIG:Debug>:WDEBUG>
)

install(TARGETS waechterd
        RUNTIME DESTINATION bin)