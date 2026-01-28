add_subdirectory(spdlog)
add_subdirectory(cereal)
add_subdirectory(sigslot)
add_subdirectory(tracy)
add_subdirectory(stb)
add_subdirectory(json11)
add_subdirectory(inih)

if (WAECHTER_BUILD_CLIENT)
    add_subdirectory(flags)
    add_subdirectory(glfw)
    add_subdirectory(glad)
    add_subdirectory(imgui)
    add_subdirectory(implot)
endif()

add_library(deps_includes INTERFACE
        flags/CountryFlags.cpp)
add_library(thirdparty::deps_includes ALIAS deps_includes)

# Special target to ignore warnings from thirdparty includes
target_include_directories(deps_includes SYSTEM INTERFACE
        ${CMAKE_SOURCE_DIR}/Thirdparty/tracy/public
        ${CMAKE_SOURCE_DIR}/Thirdparty/imgui
        ${CMAKE_SOURCE_DIR}/Thirdparty/implot
        ${CMAKE_SOURCE_DIR}/Thirdparty/json11
        ${CMAKE_SOURCE_DIR}/Thirdparty/cereal
        ${CMAKE_SOURCE_DIR}/Thirdparty/inih
        ${CMAKE_SOURCE_DIR}/Thirdparty/stb
        ${CMAKE_SOURCE_DIR}/Thirdparty/spdlog/include
        ${CMAKE_SOURCE_DIR}/Thirdparty/cereal/include
        ${CMAKE_SOURCE_DIR}/Thirdparty/sigslot/include
)
