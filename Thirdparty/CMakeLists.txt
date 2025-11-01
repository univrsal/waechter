add_subdirectory(spdlog)
add_subdirectory(inih)
add_subdirectory(glfw)
add_subdirectory(glad)
add_subdirectory(imgui)
add_subdirectory(implot)
add_subdirectory(incbin)
add_subdirectory(stb)
add_subdirectory(json11)
add_subdirectory(cereal)

add_library(deps_includes INTERFACE)
add_library(thirdparty::deps_includes ALIAS deps_includes)

target_include_directories(deps_includes SYSTEM INTERFACE
        ${CMAKE_SOURCE_DIR}/Thirdparty/imgui
        ${CMAKE_SOURCE_DIR}/Thirdparty/stb
        ${CMAKE_SOURCE_DIR}/Thirdparty/spdlog/include
        ${CMAKE_SOURCE_DIR}/Thirdparty/cereal/include
)
