add_executable(waechterd
        Waechterd.cpp
        DaemonConfig.cpp
        DaemonConfig.hpp
        EbpfObj.cpp
        EbpfObj.hpp
        WaechterEbpf.cpp
        WaechterEbpf.hpp
        EbpfMap.hpp
        EbpfData.cpp
        EbpfData.hpp
)


target_compile_options(waechterd PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS})

target_link_libraries(waechterd PRIVATE ${LIBBPF_LIBRARIES}
    waechter::util thirdparty::Spdlog thirdparty::inih
)

add_executable(old-waechterd
        old-main.cpp
)
target_compile_options(old-waechterd PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(old-waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS})

target_link_libraries(old-waechterd PRIVATE ${LIBBPF_LIBRARIES}
        waechter::util thirdparty::Spdlog thirdparty::inih
)

add_subdirectory(EBPF)
