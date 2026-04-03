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

# Find a clang that explicitly supports the BPF target (e.g. ROCm clang does not).
# Search well-known system paths first so that a vendor clang shadowing the system
# clang via PATH (e.g. /opt/rocm/lib/llvm/bin/clang) is not picked up accidentally.
set(_CLANG_CANDIDATES clang-22 clang-21 clang-20 clang-19 clang-18 clang-17 clang-16 clang)
set(_CLANG_SYSTEM_PATHS /usr/bin /usr/local/bin)
set(CLANG_EXECUTABLE "")

foreach (_CANDIDATE IN LISTS _CLANG_CANDIDATES)
    # 1) Prefer the binary found in known system directories (ignores PATH order).
    find_program(_CLANG_CANDIDATE_BIN NAMES ${_CANDIDATE}
            PATHS ${_CLANG_SYSTEM_PATHS} NO_DEFAULT_PATH)
    # 2) Fall back to a regular PATH search if not found in system dirs.
    if (NOT _CLANG_CANDIDATE_BIN)
        find_program(_CLANG_CANDIDATE_BIN NAMES ${_CANDIDATE})
    endif ()

    if (_CLANG_CANDIDATE_BIN)
        execute_process(
                COMMAND ${_CLANG_CANDIDATE_BIN} --print-targets
                OUTPUT_VARIABLE _CLANG_TARGETS
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (_CLANG_TARGETS MATCHES "bpf")
            set(CLANG_EXECUTABLE ${_CLANG_CANDIDATE_BIN})
        endif ()
    endif ()
    unset(_CLANG_CANDIDATE_BIN CACHE)
    if (CLANG_EXECUTABLE)
        break()
    endif ()
endforeach ()

if (NOT CLANG_EXECUTABLE)
    message(FATAL_ERROR "Could not find a clang executable that supports the 'bpf' target. "
            "Tried: ${_CLANG_CANDIDATES}. "
            "Make sure a standard LLVM/clang is installed.")
endif ()

message(STATUS "Using clang for bpf compilation: ${CLANG_EXECUTABLE}")

find_program(BPFTTOOL_EXECUTABLE bpftool)

set(VMLINUX_SRC "/sys/kernel/btf/vmlinux")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/vmlinux")
set(VMLINUX_OUT "${CMAKE_CURRENT_BINARY_DIR}/vmlinux/vmlinux.h")
if (BPFTTOOL_EXECUTABLE)
    message(STATUS "Found bpftool: ${BPFTTOOL_EXECUTABLE}")

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
set(BPF_SRC ${CMAKE_SOURCE_DIR}/Source/EBPF/WaechterEBPF.c)
set(BPF_HDR
        ${VMLINUX_OUT}
        EBPFConnect.h
        EBPFTraffic.h
        EBPFBind.h
        EBPFAccept.h
        EBPFInternal.h
        EBPFLifetime.h
        EBPFUdp.h
        ${CMAKE_SOURCE_DIR}/Source/Util/EBPFCommon.h
)
set(BPF_OBJ ${CMAKE_CURRENT_BINARY_DIR}/waechter-ebpf.o)
set(SKELETON_OUT "${CMAKE_CURRENT_BINARY_DIR}/WaechterEBPF.skel.h")


add_library(ebpf_includes INTERFACE)

target_include_directories(ebpf_includes SYSTEM INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(
        OUTPUT ${BPF_OBJ} ${SKELETON_OUT}
        COMMAND ${CLANG_EXECUTABLE} -O2 -g -target bpf -D__TARGET_ARCH_${BPF_TARGET_ARCH} -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -Wno-missing-declarations -Werror
        -I/usr/include -I/usr/include/bpf
        -I${CMAKE_CURRENT_BINARY_DIR}/vmlinux
        -I${CMAKE_SOURCE_DIR}/Source/Util
        -c ${BPF_SRC} -o ${BPF_OBJ}
        COMMAND ${BPFTTOOL_EXECUTABLE} gen skeleton ${BPF_OBJ} > ${SKELETON_OUT}
        DEPENDS ${BPF_SRC} ${BPF_HDR}
        COMMENT "Building eBPF object ${BPF_OBJ}"
        VERBATIM
)

add_custom_target(bpfobj ALL DEPENDS ${BPF_OBJ})
if (TARGET generate_vmlinux_h)
    add_dependencies(bpfobj generate_vmlinux_h)
endif ()

message(STATUS "EBPF object is build to " ${BPF_OBJ})