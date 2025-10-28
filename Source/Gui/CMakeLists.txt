add_executable(waechter
        WClient.cpp
        WClient.hpp
        Waechter.cpp
)

add_subdirectory(Windows)
add_subdirectory(Communication)

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
    message(STATUS " Added licenses for ${_thirdparty_libs}")
    target_compile_definitions(waechter PRIVATE ${_license_defs})
endif ()

string(TIMESTAMP TODAY "%Y.%m.%d %H:%M")

# Get the current working branch
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get the latest commit hash
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

target_compile_definitions(${PROJECT_NAME} PUBLIC
        GIT_BRANCH="${GIT_BRANCH}   "
        GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
        WAECHTER_VERSION="${CMAKE_PROJECT_VERSION}"
        BUILD_TIME="${TODAY}"
        ICON_PATH="${CMAKE_SOURCE_DIR}/Meta/Icon.png"
        FONT_PATH="${CMAKE_SOURCE_DIR}/Thirdparty/jetbrains-mono/JetBrainsMono-Regular.ttf"
)