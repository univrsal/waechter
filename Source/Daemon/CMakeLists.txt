add_executable(waechterd
        Waechterd.cpp
        DaemonConfig.cpp
        DaemonConfig.hpp
        Daemon.cpp
        Daemon.hpp
        EbpfObj.cpp
        EbpfObj.hpp
        WaechterEbpf.cpp
        WaechterEbpf.hpp
        EbpfMap.hpp
        EbpfData.cpp
        EbpfData.hpp
        EbpfRingBuffer.hpp
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
pkg_check_modules(LIBNL3 REQUIRED libnl-3.0)
pkg_check_modules(LIBNL_ROUTE3 REQUIRED libnl-route-3.0)

target_include_directories(waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${LIBNL3_INCLUDE_DIRS} ${LIBNL_ROUTE3_INCLUDE_DIRS})

target_link_libraries(waechterd PRIVATE ${LIBBPF_LIBRARIES}
        waechter::util thirdparty::spdlog thirdparty::inih thirdparty::json11 thirdparty::cereal thirdparty::deps_includes
        cap ${LIBNL3_LIBRARIES} ${LIBNL_ROUTE3_LIBRARIES}
)

add_subdirectory(EBPF)
add_subdirectory(Net)
add_subdirectory(Data)
add_subdirectory(Communication)
add_subdirectory(Rules)

target_compile_definitions(waechterd PRIVATE
        $<$<CONFIG:Debug>:WDEBUG>
)

install(TARGETS waechterd
        RUNTIME DESTINATION bin)