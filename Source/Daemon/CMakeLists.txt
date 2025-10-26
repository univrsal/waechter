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
        EbpfRingBuffer.hpp
)


target_compile_options(waechterd PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS})

target_link_libraries(waechterd PRIVATE ${LIBBPF_LIBRARIES}
        waechter::util thirdparty::spdlog thirdparty::inih
)

add_executable(old-waechterd
        old-main.cpp
)
target_compile_options(old-waechterd PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(old-waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS})

target_link_libraries(old-waechterd PRIVATE ${LIBBPF_LIBRARIES}
        waechter::util thirdparty::spdlog thirdparty::inih
)

add_subdirectory(EBPF)
add_subdirectory(Net)
add_subdirectory(Data)
