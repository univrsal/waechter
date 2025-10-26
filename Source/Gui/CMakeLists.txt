add_executable(waechter
        GlfwWindow.cpp
        GlfwWindow.hpp)

target_sources(waechter PRIVATE
        Waechter.cpp
)

target_link_libraries(waechter PRIVATE
        thirdparty::spdlog
        thirdparty::inih
        thirdparty::glfw
        thirdparty::glad
        waechter::util
)