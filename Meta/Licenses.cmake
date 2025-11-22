# Function to attach third-party LICENSE paths as
# compile definitions to a given target.
#
# Usage:
#   include("${CMAKE_SOURCE_DIR}/Meta/Licenses.cmake")
#   add_thirdparty_licenses(<target>)
#
# The function will scan the ${CMAKE_SOURCE_DIR}/Thirdparty directory for
# subdirectories that contain a LICENSE.txt and will add a compile definition
# of the form THIRD_PARTY_<SANITIZED_NAME>_LICENSE_PATH="/abs/path/to/LICENSE.txt"
# to the provided target with PRIVATE visibility.

function(add_thirdparty_licenses _target)
    if ("${_target}" STREQUAL "")
        message(FATAL_ERROR "add_thirdparty_licenses: target name required")
    endif ()

    if (NOT TARGET "${_target}")
        message(FATAL_ERROR "add_thirdparty_licenses: target '${_target}' does not exist")
    endif ()

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
                list(APPEND _license_defs "THIRD_PARTY_${_name_sanitized}_LICENSE_PATH=\"${license_path}\"")
            endif ()
        endif ()
    endforeach ()

    if (_license_defs)
        message(STATUS "Added licenses for ${_thirdparty_libs} to target '${_target}'")
        target_compile_definitions(${_target} PRIVATE ${_license_defs})
    endif ()
endfunction()
