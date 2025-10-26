add_library(json11 STATIC
        json11.cpp
        json11.hpp
)

target_include_directories(json11 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(thirdparty::json11 ALIAS json11)