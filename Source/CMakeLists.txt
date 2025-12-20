add_subdirectory(Util)
add_subdirectory(EBPF)
add_subdirectory(Daemon)
add_subdirectory(Gui)

add_dependencies(
        waechter
        waechterd
        waechter-iplink
)
add_dependencies(
        waechterd
        waechter-iplink
)
