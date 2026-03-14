add_library(imgui STATIC
        imgui.cpp
        imgui_demo.cpp
        imgui_draw.cpp
        imgui_tables.cpp
        imgui_widgets.cpp
        imgui_stdlib.cpp
        backends/imgui_impl_sdl2.cpp
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
    target_compile_options(imgui PUBLIC -sUSE_SDL=2)
    target_link_options(imgui PUBLIC -sUSE_SDL=2)
else()
    find_package(SDL2 REQUIRED)
    target_link_libraries(imgui PRIVATE
            thirdparty::glad
            SDL2::SDL2
    )
    target_compile_definitions(imgui PUBLIC
            IMGUI_IMPL_OPENGL_LOADER_GLAD
            )
endif()

if (BSD)
    find_package(OpenGL REQUIRED)
    target_link_libraries(imgui PUBLIC OpenGL::GL)
endif ()

add_library(
        thirdparty::imgui ALIAS imgui
)
