add_executable(waechter
        GlfwWindow.cpp
        GlfwWindow.hpp
        MainWindow.cpp
        MainWindow.hpp
        AboutDialog.cpp
        AboutDialog.hpp)

target_sources(waechter PRIVATE
        Waechter.cpp
)

target_link_libraries(waechter PRIVATE
        thirdparty::spdlog
        thirdparty::inih
        thirdparty::glfw
        thirdparty::glad
        thirdparty::imgui
        thirdparty::incbin
        thirdparty::stb
        waechter::util
)

target_compile_definitions(
        waechter PRIVATE ICON_PATH="${CMAKE_SOURCE_DIR}/Meta/Icon.png"
)