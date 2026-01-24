target_sources(waechter
        PRIVATE
        FlagAtlas.cpp
        FlagAtlas.hpp
        LibCurl.cpp
        LibCurl.hpp
        Settings.cpp
        Settings.hpp
        RuleWidget.cpp
        RuleWidget.hpp
        ProtocolDB.cpp
        ProtocolDB.hpp
        ImageUtil.cpp
        ImageUtil.hpp
        IP2Asn.cpp
        IP2Asn.hpp
        DbusUtil.cpp
        DbusUtil.hpp
        I18n.hpp
        I18n.cpp
        TrayIcon.cpp
        TrayIcon.hpp
)

add_subdirectory(Keylogic)

set(I18N_JSON_FILES
        "${CMAKE_SOURCE_DIR}/Meta/I18n/en_US.json"
        "${CMAKE_SOURCE_DIR}/Meta/I18n/de_DE.json"
)
message(STATUS "I18N JSON FILES: ${I18N_JSON_FILES}")

set_source_files_properties(I18n.cpp PROPERTIES
        OBJECT_DEPENDS "${I18N_JSON_FILES}"
)
