include(FindPkgConfig)
pkg_check_modules(LIBBPF REQUIRED libbpf)

# Detect BPF target arch from host
set(BPF_TARGET_ARCH x86)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(BPF_TARGET_ARCH x86)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|AARCH64|arm64")
    set(BPF_TARGET_ARCH arm64)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|armv7l|arm")
    set(BPF_TARGET_ARCH arm)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
    set(BPF_TARGET_ARCH powerpc)
endif()

find_program(CLANG_EXECUTABLE NAMES clang REQUIRED)

# Build eBPF object
set(BPF_SRC ${CMAKE_CURRENT_SOURCE_DIR}/ebpf/waechter_bpf.c)
set(BPF_OBJ ${CMAKE_CURRENT_BINARY_DIR}/ebpf/waechter_bpf.o)

add_custom_command(
    OUTPUT ${BPF_OBJ}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/ebpf
    COMMAND ${CLANG_EXECUTABLE} -O2 -g -target bpf -D__TARGET_ARCH_${BPF_TARGET_ARCH} -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -Werror
    -I/usr/include -I/usr/include/bpf
    -c ${BPF_SRC} -o ${BPF_OBJ}
    DEPENDS ${BPF_SRC}
    COMMENT "Building eBPF object ${BPF_OBJ}"
    VERBATIM
)

add_custom_target(bpfobj ALL DEPENDS ${BPF_OBJ})
add_dependencies(waechterd bpfobj)

