add_library(flags STATIC
        CountryFlags.cpp
        CountryFlags.hpp
)

add_library(thirdparty::flags ALIAS flags)

target_include_directories(flags PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})