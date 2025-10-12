add_library(glad STATIC src/glad.c)

target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(thirdparty::glad ALIAS glad)