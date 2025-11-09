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
endif ()

find_program(CLANG_EXECUTABLE NAMES clang REQUIRED)

find_program(BPFTTOOL_EXECUTABLE bpftool)

if (BPFTTOOL_EXECUTABLE)
    message(STATUS "Found bpftool: ${BPFTTOOL_EXECUTABLE}")

    set(VMLINUX_SRC "/sys/kernel/btf/vmlinux")
    set(VMLINUX_OUT "${CMAKE_CURRENT_SOURCE_DIR}/vmlinux.h")

    if (EXISTS ${VMLINUX_SRC})
        add_custom_command(
                OUTPUT ${VMLINUX_OUT}
                COMMAND /bin/sh -c "${BPFTTOOL_EXECUTABLE} btf dump file ${VMLINUX_SRC} format c > ${VMLINUX_OUT}"
                DEPENDS ${BPFTTOOL_EXECUTABLE}
                COMMENT "Generating ${VMLINUX_OUT} from ${VMLINUX_SRC}"
                VERBATIM
        )

        add_custom_target(generate_vmlinux_h ALL DEPENDS ${VMLINUX_OUT})
    else ()
        message(WARNING "BTF not found at ${VMLINUX_SRC}; skipping generation of `vmlinux.h`")
    endif ()
else ()
    message(FATAL_ERROR "bpftool not found; skipping generation of `vmlinux.h`")
endif ()

# Build eBPF object
set(BPF_SRC ${CMAKE_CURRENT_SOURCE_DIR}/WaechterEBPF.c)
set(BPF_HDR
        ${CMAKE_CURRENT_SOURCE_DIR}/vmlinux.h
        ${CMAKE_CURRENT_SOURCE_DIR}/EBPFConnect.h
        ${CMAKE_CURRENT_SOURCE_DIR}/EBPFTraffic.h
        ${CMAKE_CURRENT_SOURCE_DIR}/EBPFAccept.h
        ${CMAKE_CURRENT_SOURCE_DIR}/EBPFInternal.h
        ${CMAKE_CURRENT_SOURCE_DIR}/EBPFLifetime.h
        ${CMAKE_SOURCE_DIR}/Source/Util/EBPFCommon.h
)
set(BPF_OBJ ${CMAKE_CURRENT_BINARY_DIR}/waechter-ebpf.o)
set(SKELETON_OUT "${CMAKE_CURRENT_BINARY_DIR}/WaechterEBPF.skel.h")


add_library(ebpf_includes INTERFACE)

target_include_directories(ebpf_includes SYSTEM INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(waechterd PRIVATE ebpf_includes)

add_custom_command(
        OUTPUT ${BPF_OBJ} ${SKELETON_OUT}
        COMMAND ${CLANG_EXECUTABLE} -O2 -g -target bpf -D__TARGET_ARCH_${BPF_TARGET_ARCH} -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -Werror
        -I/usr/include -I/usr/include/bpf
        -I${CMAKE_SOURCE_DIR}/Source/Util
        -c ${BPF_SRC} -o ${BPF_OBJ}
        COMMAND ${BPFTTOOL_EXECUTABLE} gen skeleton ${BPF_OBJ} > ${SKELETON_OUT}
        DEPENDS ${BPF_SRC} ${BPF_HDR}
        COMMENT "Building eBPF object ${BPF_OBJ}"
        VERBATIM
)

add_custom_target(bpfobj ALL DEPENDS ${BPF_OBJ})
add_dependencies(bpfobj generate_vmlinux_h)
add_dependencies(waechterd bpfobj)

message(STATUS "EBPF object is build to " ${BPF_OBJ})

install(FILES ${BPF_OBJ} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)