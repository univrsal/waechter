add_library(implot
        STATIC
        implot.cpp
        implot_items.cpp
        implot_demo.cpp
        implot.h
        implot_internal.h
)

target_include_directories(implot
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(thirdparty::implot ALIAS implot)

target_link_libraries(implot PUBLIC thirdparty::imgui)