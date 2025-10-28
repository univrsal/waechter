add_subdirectory(Util)
add_subdirectory(Daemon)
add_subdirectory(Gui)

add_dependencies(
        waechter
        waechterd
)