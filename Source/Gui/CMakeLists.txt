add_executable(waechter
        GlfwWindow.cpp
        GlfwWindow.hpp
        MainWindow.cpp
        MainWindow.hpp)

target_sources(waechter PRIVATE
        Waechter.cpp
)

target_link_libraries(waechter PRIVATE
        thirdparty::spdlog
        thirdparty::inih
        thirdparty::glfw
        thirdparty::glad
        thirdparty::imgui
        waechter::util
)