add_subdirectory(Util)

if (WAECHTER_BUILD_DAEMON)
    add_subdirectory(EBPF)
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
