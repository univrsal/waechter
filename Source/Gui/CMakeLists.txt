add_executable(waechter)

target_sources(waechter PRIVATE
    Waechter.cpp
)

target_link_libraries(waechter PRIVATE
    thirdparty::spdlog
    thirdparty::inih
    thirdparty::glfw
    thirdparty::glad
)