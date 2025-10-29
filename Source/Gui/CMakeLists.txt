add_executable(waechter
        Client.cpp
        Client.hpp
        Waechter.cpp
)

add_subdirectory(Windows)

target_link_libraries(waechter PRIVATE
        thirdparty::spdlog
        thirdparty::inih
        thirdparty::glfw
        thirdparty::glad
        thirdparty::imgui
        thirdparty::incbin
        thirdparty::stb
        thirdparty::deps_includes
        thirdparty::json11
        waechter::util
)

target_include_directories(waechter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(waechter PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wformat=2 -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wcast-align
        -Wduplicated-cond -Wredundant-decls -Wpointer-arith
        -Werror
)

file(GLOB thirdparty_dirs RELATIVE "${CMAKE_SOURCE_DIR}/Thirdparty" "${CMAKE_SOURCE_DIR}/Thirdparty/*")

set(_license_defs)
set(_thirdparty_libs)
foreach (dir IN LISTS thirdparty_dirs)
    if (IS_DIRECTORY "${CMAKE_SOURCE_DIR}/Thirdparty/${dir}")
        set(license_rel "${CMAKE_SOURCE_DIR}/Thirdparty/${dir}/LICENSE.txt")
        if (EXISTS "${license_rel}")
            get_filename_component(license_abs "${license_rel}" ABSOLUTE)
            file(TO_CMAKE_PATH "${license_abs}" license_path)  # normalize separators

            string(TOUPPER "${dir}" _name_upper)
            string(REGEX REPLACE "[^A-Z0-9]" "_" _name_sanitized "${_name_upper}")

            list(APPEND _thirdparty_libs ${dir})
            list(APPEND _license_defs "THIRD_PARTY_${_name_sanitized}_LICENSE_PATH=\"${license_path}\" ")
        endif ()
    endif ()
endforeach ()

if (_license_defs)
    message(STATUS "Added licenses for ${_thirdparty_libs}")
    target_compile_definitions(waechter PRIVATE ${_license_defs})
endif ()


install(TARGETS waechter
        RUNTIME DESTINATION bin)