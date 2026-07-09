add_library(mini INTERFACE)
add_library(thirdparty::mini ALIAS mini)

target_include_directories(mini INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
