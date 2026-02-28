target_sources(util
        PRIVATE
        TrafficItem.hpp
        SystemItem.hpp
        ApplicationItem.hpp
        ProcessItem.hpp
        SocketItem.hpp
        AppIconAtlasData.hpp
        TrafficTreeUpdate.hpp
        ConnectionHistoryUpdate.hpp
)

if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    target_sources(util
            PRIVATE
            SocketStateParser.cpp
            SocketStateParser.hpp
    )
endif ()