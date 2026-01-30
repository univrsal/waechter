target_sources(waechter
        PRIVATE
        FlagAtlas.cpp
        FlagAtlas.hpp
        Settings.cpp
        Settings.hpp
        RuleWidget.cpp
        RuleWidget.hpp
        ImageUtil.cpp
        ImageUtil.hpp
        I18n.hpp
        SysUtil.cpp
        SysUtil.hpp
        ProtocolDB.cpp
        ProtocolDB.hpp
        BuiltinProtocolDB.cpp
)

if (NOT EMSCRIPTEN)
    target_sources(waechter
            PRIVATE
            LibCurl.cpp
            LibCurl.hpp
            IP2Asn.cpp
            IP2Asn.hpp
    )
endif ()

add_subdirectory(Keylogic)


set(I18N_JSON_FILES
        "${CMAKE_SOURCE_DIR}/Meta/I18n/en_US.json"
        "${CMAKE_SOURCE_DIR}/Meta/I18n/de_DE.json"
)
message(STATUS "I18N JSON FILES: ${I18N_JSON_FILES}")

set_source_files_properties(I18n.cpp PROPERTIES
        OBJECT_DEPENDS "${I18N_JSON_FILES}"
)

target_sources(waechter
        PRIVATE
        I18n.cpp
)