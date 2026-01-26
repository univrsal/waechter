add_subdirectory(Util)
add_subdirectory(EBPF)

if (WAECHTER_BUILD_DAEMON)
    add_subdirectory(Daemon)
    add_dependencies(
            waechterd
            waechter-iplink
    )
endif ()

if (WAECHTER_BUILD_CLIENT)
    add_subdirectory(Gui)
    if (WAECHTER_BUILD_DAEMON)
        add_dependencies(
                waechter
                waechterd
                waechter-iplink
        )
    endif ()
endif ()
