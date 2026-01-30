add_executable(waechter
        Client.cpp
        Client.hpp
        Waechter.cpp
        AppIconAtlas.cpp
        AppIconAtlas.hpp
        ClientRuleManager.cpp
        ClientRuleManager.hpp
        Assets.cpp
        Assets.hpp
)

# Add Windows resource file for icon and metadata
if(WIN32)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/waechter.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/waechter.rc
        @ONLY
    )
    target_sources(waechter PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/waechter.rc)

    target_compile_definitions(waechter PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

target_compile_definitions(waechter PRIVATE
        "COUNTRY_FLAG_ATLAS_IMAGE=\"${CMAKE_SOURCE_DIR}/Thirdparty/flags/atlas.png\""
        "ICON_ATLAS_IMAGE=\"${CMAKE_SOURCE_DIR}/Meta/atlas.png\""
        "LOGO_IMAGE=\"${CMAKE_SOURCE_DIR}/Meta/logo.png\""
        "WATERMARK_IMAGE=\"${CMAKE_SOURCE_DIR}/Meta/watermark.png\""
        "LANGUAGES_DIR=\"${CMAKE_SOURCE_DIR}/Meta/I18n/\""
)

add_subdirectory(Windows)
add_subdirectory(Util)
add_subdirectory(Icons)
add_subdirectory(Communication)


if (WAECHTER_WITH_WEBSOCKETCLIENT)
    target_compile_definitions(waechter PRIVATE WAECHTER_WITH_WEBSOCKETCLIENT)
    find_package(libwebsockets CONFIG REQUIRED)
    if (UNIX OR APPLE)
        target_include_directories(waechter PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS})
        target_link_libraries(waechter PRIVATE ${LIBWEBSOCKETS_LIBRARIES} dl)
    else ()
        target_link_libraries(waechter PRIVATE websockets_shared)
    endif ()
endif ()

target_link_libraries(waechter PRIVATE
        thirdparty::spdlog
        thirdparty::imgui
        thirdparty::implot
        thirdparty::stb
        thirdparty::deps_includes
        thirdparty::json11
        thirdparty::flags
        thirdparty::sigslot
        thirdparty::cereal
        thirdparty::tracy
        waechter::util
)

if (EMSCRIPTEN)
    target_link_options(waechter PRIVATE
            -sUSE_GLFW=3
            -sUSE_WEBGL2=1
            -sALLOW_MEMORY_GROWTH=1
            -sNO_DISABLE_EXCEPTION_CATCHING
            -sWEBSOCKET_URL=wss://
            -sWEBSOCKET_SUBPROTOCOL=binary
            -sASSERTIONS=2 -g
            --shell-file ${CMAKE_SOURCE_DIR}/Meta/Package/shell.html
            --bind)

    set_target_properties(waechter PROPERTIES
            SUFFIX ".html"
    )
else ()
    find_package(ZLIB REQUIRED)
    find_package(CURL REQUIRED)
    target_link_libraries(waechter PRIVATE CURL::libcurl ZLIB::ZLIB
            thirdparty::glfw
            thirdparty::glad
    )
endif ()

target_include_directories(waechter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    target_compile_options(waechter PRIVATE
            -Wall -Wextra
            -Wshadow -Wformat=2 -Wconversion -Wsign-conversion
            -Wnull-dereference -Wdouble-promotion -Wcast-align
            -Wduplicated-cond -Wredundant-decls -Wpointer-arith
            -Werror -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
            -Wnon-virtual-dtor -pedantic -Wmisleading-indentation -Wduplicated-branches -Wlogical-op -Wformat=2
            -Wuseless-cast -Wimplicit-fallthrough
    )
    #  TODO: We should probably enable most warnings on Windows and macOS as well but
    # - I don't know which flags we should use right now
    # - I don't feel like fixing a bunch of warnings on those platforms at the moment
endif ()

if (APPLE)
    target_compile_definitions(waechter PRIVATE 
	INCBIN_SILENCE_BITCODE_WARNING
    )
endif()

include("${CMAKE_SOURCE_DIR}/Meta/Licenses.cmake")
add_thirdparty_licenses(waechter)


install(TARGETS waechter
        RUNTIME DESTINATION bin)

target_compile_definitions(waechter PRIVATE
        $<$<CONFIG:Debug>:WDEBUG>
)
