add_subdirectory(Util)
add_subdirectory(EBPF)

if (WAECHTER_BUILD_DAEMON)
    add_subdirectory(Daemon)
endif ()

if (WAECHTER_BUILD_CLIENT)
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
endif ()
