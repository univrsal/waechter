add_library(tracy STATIC
        public/TracyClient.cpp)
target_include_directories(tracy PUBLIC public)

add_library(thirdparty::tracy ALIAS tracy)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    #target_compile_definitions(tracy PUBLIC TRACY_ENABLE)
endif ()