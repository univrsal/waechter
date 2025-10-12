add_executable(waechterd
        Waechterd.cpp
        DaemonConfig.cpp
        DaemonConfig.hpp
        EbpfObj.cpp
        EbpfObj.hpp
        WaechterEpf.cpp
        WaechterEpf.hpp
)

target_compile_options(waechterd PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS})

target_link_libraries(waechterd PRIVATE ${LIBBPF_LIBRARIES}
    waechter::util thirdparty::Spdlog thirdparty::inih
)
target_compile_definitions(waechterd PRIVATE BPF_OBJECT_PATH="${BPF_OBJ}")
