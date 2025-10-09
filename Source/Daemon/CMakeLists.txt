add_executable(waechterd
        Waechterd.cpp
)

target_compile_options(waechterd PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS})

target_link_libraries(waechterd PRIVATE ${LIBBPF_LIBRARIES}
    waechter::util Thirdparty::Spdlog
)
target_compile_definitions(waechterd PRIVATE BPF_OBJECT_PATH="${BPF_OBJ}")
