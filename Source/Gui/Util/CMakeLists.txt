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
)
add_subdirectory(Keylogic)
if (WAECHTER_WITH_IPQUERY_IO_API)
    target_compile_definitions(waechter PRIVATE WAECHTER_WITH_IPQUERY_IO_API=1)
    target_sources(waechter
            PRIVATE
            IpQueryIntegration.cpp
            IpQueryIntegration.hpp
    )
    message(STATUS "ipquery.io API integration enabled")
else ()
    target_compile_definitions(waechter PRIVATE WAECHTER_WITH_IPQUERY_IO_API=0)
    message(STATUS "ipquery.io API integration disabled")
endif ()