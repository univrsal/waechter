add_library(inih STATIC)

target_sources(inih PUBLIC
    ini.c INIReader.cpp
)
target_include_directories(inih PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(thirdparty::inih ALIAS inih)
