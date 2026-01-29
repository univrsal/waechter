add_library(imgui STATIC
        imgui.cpp
        imgui_demo.cpp
        imgui_draw.cpp
        imgui_tables.cpp
        imgui_widgets.cpp
        imgui_stdlib.cpp
        backends/imgui_impl_glfw.cpp
        backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

if (EMSCRIPTEN)
    target_compile_definitions(imgui PUBLIC
            IMGUI_IMPL_OPENGL_ES2
            )
else()
    target_link_libraries(imgui PRIVATE
            thirdparty::glad
            thirdparty::glfw
    )
    target_compile_definitions(imgui PUBLIC
            IMGUI_IMPL_OPENGL_LOADER_GLAD
            )
endif()

add_library(
        thirdparty::imgui ALIAS imgui
)
