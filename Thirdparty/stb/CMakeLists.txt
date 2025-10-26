add_library(stb INTERFACE)
add_library(thirdparty::stb ALIAS stb)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})